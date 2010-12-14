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



#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>

/* enable logging & debugging */
#define CONFIG_ASSERT
#define CONFIG_LOG

#include "ylsfunc.h"
#include "yltrie.h"
#include "yldynb.h"

static int
_trie_value_cmp(const yle_t* e0, const yle_t* e1) {
    return yleis_nil(yleq(e0, e1))? 0: 1;
}

static int
_aif_trie_eq(const yle_t* e0, const yle_t* e1) {
    return yltrie_equal((yltrie_t*)ylacd(e0), (yltrie_t*)ylacd(e1),
                        (int(*)(const void*, const void*))&_trie_value_cmp);
}

#if 0 /* Keep it for future use! */
static void*
_cb_clone(void* map, const void* v) {
    return ylechain_clone(v);
}

static int
_aif_trie_copy(void* map, yle_t* n, const yle_t* e) {
    ylacd(n) = yltrie_create(yltrie_fcb((yltrie_t*)ylacd(e)));
    /*
     * Copy SHOULD BE USED (NOT 'yltrie_clone')
     * Why?
     *    If there is error at somewhere in trie during clone,
     *     'Memory Leak' is unavoidable in case of 'yltrie_clone'.
     *    But, we already alloced mp block and we are using this at 'yltrie_copy'.
     *    So, even if error raised, this newly allocated block is GCed and allocated trie will be freed at 'clean'!
     */
    yltrie_copy((yltrie_t*)ylacd(n), (yltrie_t*)ylacd(e), NULL, _cb_clone);

    return 0;
}
#endif /* Keep it for future use! */

static int
_cb_trie_print_walk(void* user, const unsigned char* key,
                    unsigned int sz, void* v) {
    /* HACK for dynamic buffer */
    yldynb_t* b = user; /* this is string buffer */
    /* remove trailing 0 of string buffer */
    ylassert(b->sz > 0);
    b->sz--;
    yldynb_append(b, key, sz);
    yldynb_append(b, (unsigned char*)" ", sizeof(" ")); /* add space and trailing 0 to make buffer string */
    /* keep going */
    return 1;
}

static int
_aif_trie_to_string(const yle_t* e, char* b, unsigned int sz) {
    yldynb_t  dyb;
    yltrie_t* t = (yltrie_t*)ylacd(e);
    int       bw = 0;

    if(0 > yldynbstr_init(&dyb, 1024)) { ylassert(0); }
    yldynbstr_append(&dyb, "[");
    yltrie_walk(t, &dyb, (unsigned char*)"", 0, &_cb_trie_print_walk);
    yldynbstr_append(&dyb, "]");

    bw = yldynbstr_len(&dyb);
    if(sz < bw) { bw = -1; }
    else { memcpy(b, yldynbstr_string(&dyb), bw); }
    yldynb_clean(&dyb);

    return bw;
}


struct _trie_walk_user {
    void*   user;
    int(*   cb)(void*, yle_t*);
};

static int
_aif_trie_visit(yle_t* e, void* user, int(*cb)(void*, yle_t*));

static int
_aif_trie_visit_cb(void* user, const unsigned char* key,
                   unsigned int sz, void* v) {
    struct _trie_walk_user* u = (struct _trie_walk_user*)user;
    (*u->cb)(u->user, (yle_t*)v);
    return 1; /* keep going */
}

static int
_aif_trie_visit(yle_t* e, void* user, int(*cb)(void*, yle_t*)) {
    struct _trie_walk_user     stu;
    stu.user = user;
    stu.cb = cb;
    yltrie_walk(ylacd(e), &stu, (unsigned char*)"", 0, &_aif_trie_visit_cb);
    return 1;
}

static void
_aif_trie_clean(yle_t* e) {
    yltrie_destroy(ylacd(e));
}


static ylatomif_t _aif_trie = {
    &_aif_trie_eq,
    NULL,
    &_aif_trie_to_string,
    &_aif_trie_visit,
    &_aif_trie_clean
};


static inline int
_is_trie_type(const yle_t* e) {
    return &_aif_trie == ylaif(e);
}

YLDEFNF(make_trie, 0, 1) {
    yltrie_t*  t = NULL;
    yle_t     *w, *r;

    /* Parameter validataion */
    if(pcsz > 0) {
        w = ylcar(e);
        /* 'nil' as an initial pair is allowed */
        if(yleis_atom(w) && !yleis_nil(w)) { goto invalid_param; }
        ylelist_foreach(w) {
            if(yleis_atom(ylcar(w))
               || !ylais_type(ylcaar(w), ylaif_sym())) {
                goto invalid_param;
            }
        }
    }

    /*
     * Do NOTHING when value isfreed.
     * UNLINKING is ENOUGH! (so fcb is NULL)
     */
    t = yltrie_create(NULL);
    /*
     * bind to memory block as soon as possible to avoid memory leak.
     * (one of eval. below may fails.
     *  Then trie 't' becomes dangling, if it is not binded to mem block)
     */
    r = ylacreate_cust(&_aif_trie, t);

    /* r should be protected from GC - there is eval below! */
    ylmp_add_bb1(r);
    if(pcsz > 0) {
        yle_t*  v;
        w = ylcar(e);
        ylelist_foreach(w) {
            v = yleval(cxt, ylcadar(w), a);
            if(1 == yltrie_insert(t, (unsigned char*)ylasym(ylcaar(w)).sym,
                                  (unsigned int)strlen(ylasym(ylcaar(w)).sym), v) ) {
                ylnflogW1("Trie duplicated intial value : %s\n", ylasym(ylcaar(w)).sym);
            }
        }
    }
    ylmp_rm_bb1(r);

    return r;

 invalid_param:
    ylnflogE0("invalid parameter type\n");
    ylinterpret_undefined(YLErr_func_invalid_param);
    return NULL; /* to make compiler be happy */
} YLENDNF(make-trie)

YLDEFNF(trie_insert, 2, 3) {
    yltrie_t*   t;
    yle_t*      v;

    ylnfcheck_parameter(_is_trie_type(ylcar(e))
                        && ylais_type(ylcadr(e), ylaif_sym()));

    t = (yltrie_t*)ylacd(ylcar(e));
    v = (pcsz > 2)? ylcaddr(e): ylnil();

    /* unref will be done inside of 'insert' by _element_freecb */
    switch(yltrie_insert(t, (unsigned char*)ylasym(ylcadr(e)).sym,
                         (unsigned int)strlen(ylasym(ylcadr(e)).sym), v)) {
        case -1:
            ylnflogE1("Fail to insert to trie : %s\n", ylasym(ylcadr(e)).sym);
            ylinterpret_undefined(YLErr_func_fail);

        case 1: return ylt();   /* overwritten */
        case 0: return ylnil(); /* newly inserted */
        default:
            ylassert(0); /* This should not happen! */
            return NULL; /* to make compiler be happy */
    }
} YLENDNF(trie_insert)

YLDEFNF(trie_del, 2, 2) {
    yltrie_t*   t;

    ylnfcheck_parameter(_is_trie_type(ylcar(e))
                        && ylais_type(ylcadr(e), ylaif_sym()));

    t = (yltrie_t*)ylacd(ylcar(e));
    if(0 > yltrie_delete(t, (unsigned char*)ylasym(ylcadr(e)).sym,
                         (unsigned int)strlen(ylasym(ylcadr(e)).sym))) {
        /* invalid slot name */
        ylnflogW1("invalid slot name : %s\n", ylasym(ylcadr(e)).sym);
        return ylnil();
    } else {
        return ylt();
    }
} YLENDNF(trie_del)

YLDEFNF(trie_get, 2, 2) {
    yltrie_t*   t;
    yle_t*      v;

    ylnfcheck_parameter(_is_trie_type(ylcar(e))
                        && ylais_type(ylcadr(e), ylaif_sym()));

    t = (yltrie_t*)ylacd(ylcar(e));
    v = yltrie_get(t, (unsigned char*)ylasym(ylcadr(e)).sym,
                   (unsigned int)strlen(ylasym(ylcadr(e)).sym));
    if(v) { return (yle_t*)v; }
    else {
        /* invalid slot name */
        ylnflogW1("invalid slot name : %s\n", ylasym(ylcadr(e)).sym);
        return ylnil();
    }
} YLENDNF(trie_get)
