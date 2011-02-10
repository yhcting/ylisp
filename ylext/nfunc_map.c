/*****************************************************************************
 *    Copyright (C) 2010 Younghyung Cho. <yhcting77@gmail.com>
 *
 *    This file is part of YLISP.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as
 *    published by the Free Software Foundation either version 3 of the
 *    License, or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License
 *    (<http://www.gnu.org/licenses/lgpl.html>) for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>

/*
 * To support multi-threading in trie!
 */
#include <unistd.h>
#include <pthread.h>
#include "ylsfunc.h"
#include "yltrie.h"
#include "yldynb.h"
#include "hash.h"
#include "crc.h"

/*
 * amap Interface
 */
struct _amapif {
    void  (*destroy) (void*);

    /*
     * Can be NULL.
     * (NULL means 'shallow compare' - compare memory address only)
     * @cmp    : compare function of value. this should return 1 if value is same, otherwise 0.
     * @return : 1 if same, otherwise 0.
     */
    int   (*equal)   (const void*, const void*, int (*cmp) (const void* v0, const void* v1));

    /*
     * @return:
     *   -1: error
     *    0: newly inserted
     *    1: overwritten
     */
    int   (*insert)  (void*, const unsigned char* key, unsigned int, void* v);

    /*
     * @return:
     *    <0: fail. cannot be found
     *    0 : deleted.
     */
    int   (*delete)  (void*, const unsigned char* key, unsigned int);

    /*
     * @return     : NULL if @key is not in data structure.
     */
    void* (*get)     (void*, const unsigned char* key, unsigned int);

    /*
     * walking all nodes
     * @return:
     *    1 : success. End of trie.
     *    0 : stop by user callback
     *   -1 : fail.
     */
    int   (*walk)    (void*, void* user,
                      /* return : b_keepgoing => return 1 for keep going, 0 for stop and don't do anymore*/
                      int (*cb) (void* user,
                                const unsigned char* key, unsigned int sz,
                                void* v));
    

};

/* atom trie */
struct _amap {
    void*                  d; /* data structure - can be hash or trie or something else */
    pthread_rwlock_t       m;
    const struct _amapif*  i;
};

static const struct _amapif _trieif = {
    (void (*) (void*))
        &yltrie_destroy,
    (int (*) (const void*, const void*, int (*) (const void*, const void*)))
        &yltrie_equal,
    (int (*) (void*, const unsigned char*, unsigned int, void*))
        &yltrie_insert,
    (int (*) (void*, const unsigned char*, unsigned int))
        &yltrie_delete,
    (void* (*) (void*, const unsigned char*, unsigned int))
        &yltrie_get,
    (int (*) (void*, void*, int (*) (void*, const unsigned char*, unsigned int, void*)))
        &yltrie_full_walk,
};

static const struct _amapif _hashif = {
    (void (*) (void*))
        &hash_destroy,
    NULL,
    (int (*) (void*, const unsigned char*, unsigned int, void*))
        &hash_add,
    (int (*) (void*, const unsigned char*, unsigned int))
        &hash_del,
    (void* (*) (void*, const unsigned char*, unsigned int))
        &hash_get,
    (int (*) (void*, void*, int (*) (void*, const unsigned char*, unsigned int, void*)))
        &hash_walk
};

static struct _amap*
_alloc (void* d, const struct _amapif* i) {
    struct _amap* am = ylmalloc (sizeof (*am));
    ylassert (am);
    am->d = d;
    am->i = i;
    /* use default mutex attribute */
    pthread_rwlock_init (&am->m, NULL);
    return am;
}

static void
_amap_destroy (struct _amap* am) {
    (*am->i->destroy) (am->d);
    /*
     * am->i is NOT allocated memory.
     * So, skip freeing.
     */

    /*
     * Destroying locked mutex is undefined at 'pthread_mutex_destroy'.
     * But, this atom SHOULD be destroied only at GC.
     * So, at this moment, mutex SHOULD NOT be in locked state.
     */
    pthread_rwlock_destroy (&am->m);
    ylfree (am);
}

static inline void*
_amapd (const yle_t* e) { return ((struct _amap*)ylacd (e))->d; }

static inline pthread_rwlock_t*
_amapm (const yle_t* e) { return &((struct _amap*)ylacd (e))->m; }

static inline const struct _amapif*
_amapi (const yle_t* e) { return ((struct _amap*)ylacd (e))->i; }

static int
_amap_value_cmp (const yle_t* e0, const yle_t* e1) {
    return yleis_nil (yleq (e0, e1))? 0: 1;
}

static int
_aif_map_eq (const yle_t* e0, const yle_t* e1) {
    int ret;
    pthread_rwlock_rdlock (_amapm (e0));
    pthread_rwlock_rdlock (_amapm (e1));
    if (_amapi (e0)->equal)
        ret = (*_amapi (e0)->equal)(_amapd (e0), _amapd (e1),
                                    (int(*)(const void*, const void*))&_amap_value_cmp);
    else ret = (e0 == e1);
    pthread_rwlock_unlock (_amapm (e1));
    pthread_rwlock_unlock (_amapm (e0));
    return ret;
}

static int
_cb_map_print_walk (void* user, const unsigned char* key,
                    unsigned int sz, void* v) {
    /* HACK for dynamic buffer */
    yldynb_t* b = user; /* this is string buffer */
    /* remove trailing 0 of string buffer */
    ylassert (b->sz > 0);
    b->sz--;
    yldynb_append (b, key, sz);
    yldynb_append (b, (unsigned char*)" ", sizeof (" ")); /* add space and trailing 0 to make buffer string */
    /* keep going */
    return 1;
}

static int
_aif_map_to_string (const yle_t* e, char* b, unsigned int sz) {
    yldynb_t  dyb;
    int       bw = 0;

    if (0 > yldynbstr_init (&dyb, 1024)) ylassert(0);
    yldynbstr_append (&dyb, "[");

    pthread_rwlock_rdlock (_amapm (e));
    (*_amapi (e)->walk) (_amapd (e), &dyb, &_cb_map_print_walk);
    pthread_rwlock_unlock (_amapm (e));

    yldynbstr_append (&dyb, "]");

    bw = yldynbstr_len (&dyb);
    if (sz < bw) bw = -1;
    else memcpy (b, yldynbstr_string (&dyb), bw);
    yldynb_clean (&dyb);

    return bw;
}


struct _map_visit_user {
    void*   user;
    int(*   cb) (void*, yle_t*);
};

static int
_aif_map_visit_cb (void* user, const unsigned char* key,
                   unsigned int sz, void* v) {
    struct _map_visit_user* u = (struct _map_visit_user*)user;
    (*u->cb) (u->user, (yle_t*)v);
    return 1; /* keep going */
}

static int
_aif_map_visit (yle_t* e, void* user, int(*cb) (void*, yle_t*)) {
    struct _map_visit_user     stu;
    stu.user = user;
    stu.cb = cb;

    /*
     * Map data structure is not changed during visiting.
     * But, map value may be modified.
     * Because map value is also ylisp element,
     *  synchronization for value is not this data structure's responsibility.
     * Map value SHOULD synchronize modification of it's value, for itself.
     * That's why 'rdlock' is used instead of 'wrlock'
     */
    pthread_rwlock_rdlock (_amapm (e));
    (*_amapi (e)->walk) (_amapd (e), &stu, &_aif_map_visit_cb);
    pthread_rwlock_unlock (_amapm (e));

    return 1;
}

static void
_aif_map_clean (yle_t* e) {
    /*
     * Locking mutex is not used here...
     * See comments in '_atrie_destroy' for details.
     */
    _amap_destroy ((struct _amap*)ylacd (e));
}


static ylatomif_t _aif_map = {
    &_aif_map_eq,
    NULL,
    &_aif_map_to_string,
    &_aif_map_visit,
    &_aif_map_clean
};

static inline int
_is_map_type (const yle_t* e) {
    return &_aif_map == ylaif (e);
}

static yle_t*
_make_map (yletcxt_t* cxt, yle_t* e, yle_t* a, int pcsz,
           void* d, const struct _amapif* i) {
    yle_t     *w, *r;

    /* Parameter validataion */
    if (pcsz > 0) {
        w = ylcar (e);
        /* 'nil' as an initial pair is allowed */
        if (yleis_atom (w) && !yleis_nil (w)) goto invalid_param;
        ylelist_foreach (w)
            if (yleis_atom (ylcar (w))
               || !ylais_type (ylcaar (w), ylaif_sym ()))
                goto invalid_param;
    }

    /*
     * bind to memory block as soon as possible to avoid memory leak.
     * (one of eval. below may fails.
     *  Then trie 't' becomes dangling, if it is not binded to mem block)
     */
    r = ylacreate_cust (&_aif_map, _alloc (d, i));

    /* r should be protected from GC - there is eval below! */
    ylmp_add_bb1 (r);
    if (pcsz > 0) {
        yle_t*  v;
        w = ylcar (e);
        ylelist_foreach (w) {
            v = yleval (cxt, ylcadar (w), a);
            if (1 == (*_amapi (r)->insert) (_amapd (r), (unsigned char*)ylasym (ylcaar (w)).sym,
                                  (unsigned int)strlen (ylasym (ylcaar (w)).sym), v) )
                yllogW ("Map duplicated intial value : %s\n", ylasym (ylcaar (w)).sym);
        }
    }
    ylmp_rm_bb1 (r);

    return r;

 invalid_param:
    yllogE ("invalid parameter type\n");
    return NULL; /* to make compiler be happy */
}

YLDEFNF(make_trie_map, 0, 1) {
    yltrie_t*  t = NULL;
    yle_t*     r;
    /*
     * Do NOTHING when value is freed.
     * UNLINKING is ENOUGH! (so fcb is NULL)
     */
    t = yltrie_create (NULL);
    ylassert (t);
    r = _make_map (cxt, e, a, pcsz, t, &_trieif);
    if (r) return r;
    else {
        yltrie_destroy (t);
        ylinterpret_undefined (YLErr_func_fail);
    }
} YLENDNF(make_trie_map)

YLDEFNF(make_hash_map, 0, 1) {
    hash_t*  h = NULL;
    yle_t*   r;

    h = hash_create (NULL);
    ylassert (h);
    r = _make_map (cxt, e, a, pcsz, h, &_hashif);
    if (r) return r;
    else {
        hash_destroy (h);
        ylinterpret_undefined (YLErr_func_fail);
    }
} YLENDNF(make_hash_map)

YLDEFNF(map_insert, 2, 3) {
    yle_t*      v;
    int         r;

    ylnfcheck_parameter (_is_map_type (ylcar (e))
                        && ylais_type (ylcadr (e), ylaif_sym ()));

    v = (pcsz > 2)? ylcaddr (e): ylnil ();

    pthread_rwlock_wrlock (_amapm (ylcar (e)));
    r = (*_amapi (ylcar (e))->insert) (_amapd (ylcar (e)),
                                       (unsigned char*)ylasym (ylcadr (e)).sym,
                                       (unsigned int)strlen (ylasym (ylcadr(e)).sym), v);
    pthread_rwlock_unlock (_amapm (ylcar (e)));

    /* unref will be done inside of 'insert' by _element_freecb */
    switch (r) {
        case -1:
            ylnfinterp_fail (YLErr_func_fail, "Fail to insert to trie : %s\n", ylasym (ylcadr (e)).sym);
        case 1: return ylt ();   /* overwritten */
        case 0: return ylnil (); /* newly inserted */
        default:
            ylassert (0); /* This should not happen! */
            return NULL; /* to make compiler be happy */
    }
} YLENDNF(map_insert)

YLDEFNF(map_del, 2, 2) {
    int         r;

    ylnfcheck_parameter (_is_map_type (ylcar (e))
                        && ylais_type (ylcadr (e), ylaif_sym ()));

    pthread_rwlock_wrlock (_amapm (ylcar (e)));
    r = (*_amapi (ylcar (e))->delete) (_amapd (ylcar (e)),
                                       (unsigned char*)ylasym (ylcadr (e)).sym,
                                       (unsigned int)strlen (ylasym (ylcadr (e)).sym));
    pthread_rwlock_unlock (_amapm (ylcar (e)));

    if (0 > r) {
        /* invalid slot name */
        ylnflogW ("invalid slot name : %s\n", ylasym (ylcadr (e)).sym);
        return ylnil ();
    } else
        return ylt ();
} YLENDNF(map_del)

YLDEFNF(map_get, 2, 2) {
    yle_t*      v;

    ylnfcheck_parameter (_is_map_type (ylcar (e))
                         && ylais_type (ylcadr (e), ylaif_sym ()));

    pthread_rwlock_rdlock (_amapm (ylcar (e)));
    v = (*_amapi (ylcar (e))->get) (_amapd (ylcar (e)),
                                    (unsigned char*)ylasym (ylcadr (e)).sym,
                                    (unsigned int)strlen (ylasym (ylcadr (e)).sym));
    pthread_rwlock_unlock (_amapm (ylcar (e)));

    if (v) return (yle_t*)v;
    else {
        /* invalid slot name */
        ylnflogW ("invalid slot name : %s\n", ylasym (ylcadr (e)).sym);
        return ylnil ();
    }
} YLENDNF(map_get)

/*
 * 
 */
YLDEFNF(crc, 1, 1) {
    ylnfcheck_parameter (ylais_type (ylcar (e), ylaif_bin ()));
    return ylacreate_dbl (crc32 (0, ylabin (ylcar (e)).d, ylabin (ylcar (e)).sz));
} YLENDNF(crc)
