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
#include "ylut.h"

/* --------------------------------------
 * Define new type - trie START
 * --------------------------------------*/

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

    if(0 > ylutstr_init(&dyb, 1024)) { ylassert(0); }
    ylutstr_append(&dyb, "[");
    yltrie_walk(t, &dyb, (unsigned char*)"", 0, &_cb_trie_print_walk);
    ylutstr_append(&dyb, "]");

    bw = ylutstr_len(&dyb);
    if(sz < bw) { bw = -1; }
    else { memcpy(b, ylutstr_string(&dyb), bw); }
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
    u->cb(u->user, (yle_t*)v);
    return 1; /* keep going */
}

static int
_aif_trie_visit(yle_t* e, void* user, int(*cb)(void*, yle_t*)) {
    struct _trie_walk_user     stu;
    stu.user = user;
    stu.cb = cb;
    yltrie_walk(ylacd(e), &stu, (unsigned char*)"", 0, &_aif_trie_visit_cb);
    return 0;
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

YLDEFNF(trie_create, 0, 1) {
    yltrie_t*  t = NULL;
    yle_t     *w, *r;

    /* Parameter validataion */
    if(pcsz > 0) {
        w = ylcar(e);
        if(yleis_atom(w)) { goto invalid_param; }
        while( !yleis_nil(w) ) {
            if(yleis_atom(ylcar(w))
               || !ylais_type(ylcaar(w), ylaif_sym())) {
                goto invalid_param;
            }
            w = ylcdr(w);
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
    ylmp_push1(r);
    if(pcsz > 0) {
        yle_t*  v;
        w = ylcar(e);
        while(!yleis_nil(w)) {
            v = yleval(ylcadar(w), a);
            if(1 == yltrie_insert(t, (unsigned char*)ylasym(ylcaar(w)).sym,
                                  (unsigned int)strlen(ylasym(ylcaar(w)).sym), v) ) {
                ylnflogW1("Trie duplicated intial value : %s\n", ylasym(ylcaar(w)).sym);
            }
            w = ylcdr(w);
        }
    }
    ylmp_pop1();

    return r;

 invalid_param:
    ylnflogE0("invalid parameter type\n");
    ylinterpret_undefined(YLErr_func_invalid_param);
    return NULL; /* to make compiler be happy */
} YLENDNF(trie_create)

YLDEFNF(trie_insert, 2, 3) {
    yltrie_t*   t;
    yle_t*      v;
    if(!(_is_trie_type(ylcar(e))
         && ylais_type(ylcadr(e), ylaif_sym()))) {
        ylnflogE0("invalid parameter type\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }

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
    if(!(_is_trie_type(ylcar(e))
         && ylais_type(ylcadr(e), ylaif_sym()))) {
        ylnflogE0("invalid parameter type\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }

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
    if(!(_is_trie_type(ylcar(e))
         && ylais_type(ylcadr(e), ylaif_sym()))) {
        ylnflogE0("invalid parameter type\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }

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

/* -------------------------------------- */





/* --------------------------------------
 * Define new type - array START
 * --------------------------------------*/

typedef struct {
    yle_t**        arr;
    unsigned int   sz;
} _earr_t;

static inline void
_destroy_arr_data(_earr_t* at) {
    if(at) {
        /* unlinking is enough! */
        if(at->arr) { ylfree(at->arr); }
        ylfree(at);
    }
}

static int
_aif_arr_eq(const yle_t* e0, const yle_t* e1) {
    _earr_t* at0 = ylacd(e0);
    _earr_t* at1 = ylacd(e1);

    if(!at0 && !at1) {
        return 1;
    } else if(at0 && at1) {
        int i;
        if(at0->sz != at1->sz) { return 0; }
        for(i=0; i<at0->sz; i++) {
            if( yleis_nil(yleq(at0->arr[i], at1->arr[i])) ) { return 0; }
        }
        return 1;
    } else {
        return 0;
    }
}

#if 0 /* Keep it for future use! */
static int
_aif_arr_copy(void* map, yle_t* n, const yle_t* e) {
    _earr_t*    sat = ylacd(e);
    _earr_t*    at = NULL;
    if(sat) {
        int i;
        at = ylmalloc(sizeof(_earr_t));
        if(!at) { goto oom; }

        at->sz = sat->sz;
        at->arr = ylmalloc(sizeof(*at->arr) * at->sz);
        if(!at->arr) { goto oom; }
        memset(at->arr, 0, sizeof(*at->arr) * at->sz);
        for(i=0; i<sat->sz; i++) {
            if(sat->arr[i]) {
                at->arr[i] = ylechain_clone(sat->arr[i]);
            }
        }
    }
    ylacd(n) = at;
    return 0;

 oom:
    _destroy_arr_data(at);
    return -1;
}
#endif /* Keep it for future use! */

static int
_aif_arr_to_string(const yle_t* e, char* b, unsigned int sz) {
    /*
     * recursive printing by using 'ylechain_print' is impossible...
     * (ylechain_print uses static buffer.. and this function may be re-enterred by recursive printing!
     * atom should print it's own data... not containing reference!
     */
    _earr_t* at = ylacd(e);
    int      bw;
    bw = snprintf(b, sz, ">>ARR:%d<<", at->sz);
    if(bw >= sz) { return -1; } /* not enough buffer */
    return bw;
}

static int
_aif_arr_visit(yle_t* e, void* user, int(*cb)(void*, yle_t*)) {
    _earr_t* at = ylacd(e);
    if(at) {
        int    i;
        for(i=0; i<at->sz; i++) {
            if(at->arr[i]) { cb(user, at->arr[i]); }
        }
    }
    return 0;
}

static void
_aif_arr_clean(yle_t* e) {
    _destroy_arr_data(ylacd(e));
    ylacd(e) = NULL;
}

static ylatomif_t _aif_arr = {
    &_aif_arr_eq,
    NULL,
    &_aif_arr_to_string,
    &_aif_arr_visit,
    &_aif_arr_clean
};

static inline int
_is_arr_type(const yle_t* e) {
    return &_aif_arr == ylaif(e);
}

/*
 * return <0 if fails (Usually OOM)
 */
static int
_arr_alloc(yle_t* ar, yle_t* e) {
    int         i;
    _earr_t*    at;

    ylassert(!yleis_nil(e));

    at = ylmalloc(sizeof(_earr_t));
    if(!at) { goto oom; }
    /* only integer is allowed */
    at->sz = (unsigned int)(yladbl(ylcar(e)));
    at->arr = ylmalloc(sizeof(*at->arr) * at->sz);
    if(!at->arr) { goto oom; }
    memset(at->arr, 0, sizeof(*at->arr) * at->sz);

    /* move to next dimension */
    e = ylcdr(e);
    for(i=0; i<at->sz; i++) {
        if(yleis_nil(e)) {
            at->arr[i] = ylnil();
        } else {
            at->arr[i] = ylacreate_cust(&_aif_arr, NULL);
            if(0 > _arr_alloc(at->arr[i], e)) { goto oom; }
        }
    }

    ylacd(ar) = at;
    return 0;

 oom:
    /* clean */
    _destroy_arr_data(at);
    return -1;
 }

#define _check_dim_param(e)                                     \
    while( !yleis_nil(e) ) {                                    \
        if(!ylais_type(ylcar(e), ylaif_dbl())                   \
           || (long)(yladbl(ylcar(e))) != yladbl(ylcar(e))      \
           || yladbl(ylcar(e)) < 0 ) {                          \
            ylnflogE0("invalid parameter type\n");              \
            ylinterpret_undefined(YLErr_func_invalid_param);    \
        }                                                       \
        (e) = ylcdr(e);                                         \
    }


YLDEFNF(arr_create, 1, 9999) {
    yle_t*      r = NULL;

    r = e;
    _check_dim_param(r);

    r = ylacreate_cust(&_aif_arr, NULL);
    if(0 > _arr_alloc(r, e) ) { goto oom; }

    return r;

 oom:
    ylnflogE0("Out Of Memory\n");
    ylinterpret_undefined(YLErr_out_of_memory);
    return NULL; /* to make compiler be happy */
} YLENDNF(arr_create)


/*
 * Get array value pointer of given index!
 * return NULL if fails
 */
static yle_t**
_arr_get(yle_t* e, yle_t* ie) {
    const _earr_t*  at;
    long      i = yladbl(ylcar(ie));
    if(!_is_arr_type(e) || !ylacd(e)) { goto bail; }
    at = ylacd(e);
    if(at->sz <= i) { goto bail;  }

    if(yleis_nil(ylcdr(ie))) {
        /* this is last dim-index */
        return &at->arr[i];
    } else {
        return _arr_get(at->arr[i], ylcdr(ie));
    }

 bail:
    yllogE0("Invalid Array Access\n");
    return NULL;
}

YLDEFNF(arr_get, 2, 9999) {
    yle_t** pv;

    if(!_is_arr_type(ylcar(e))) {
        ylnflogE0("invalid parameter type\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }

    { /* Just scope */
        yle_t* w = ylcdr(e);
        _check_dim_param(w);
    }

    pv = _arr_get(ylcar(e), ylcdr(e));
    if(pv) { return *pv; }
    else {
        ylinterpret_undefined(YLErr_func_fail);
        return NULL; /* to make compiler be happy */
    }
} YLENDNF(arr_get)


YLDEFNF(arr_set, 3, 9999) {
    yle_t** pv;

    if(!_is_arr_type(ylcar(e))) {
        ylnflogE0("invalid parameter type\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }

    { /* Just scope */
        yle_t* w = ylcddr(e); /* dimension starts from 3rd parameter */
        _check_dim_param(w);
    }

    pv = _arr_get(ylcar(e), ylcddr(e));
    if(pv && *pv) { /* *pv cannot be NULL */
        *pv = ylcadr(e);
        return ylcadr(e);
    } else {
        ylinterpret_undefined(YLErr_func_fail);
        return NULL; /* to make compiler be happy */
    }
} YLENDNF(arr_set)


/* -------------------------------------- */
