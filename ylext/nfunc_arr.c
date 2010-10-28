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
#include "ylut.h"

#define _arr(e) ((_err_t*)ylacd(e))

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
    if(at->sz > 0) {
        at->arr = ylmalloc(sizeof(*at->arr) * at->sz);
        if(!at->arr) { goto oom; }
        memset(at->arr, 0, sizeof(*at->arr) * at->sz);
    } else {
        at->arr = NULL;
    }

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

#define _check_dim_param(e)                                             \
    ylelist_foreach(e) {                                                \
        ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_dbl())           \
                            && (long long)(yladbl(ylcar(e))) == yladbl(ylcar(e)) \
                            && yladbl(ylcar(e)) >= 0);                  \
    }


YLDEFNF(make_array, 1, 9999) {
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
} YLENDNF(make_array)


/*
 * Get array value pointer of given index!
 * return NULL if fails
 */
static yle_t**
_arr_get(yle_t* e, yle_t* ie) {
    const _earr_t*  at;
    long long       i = yladbl(ylcar(ie));
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
    ylnfcheck_parameter(_is_arr_type(ylcar(e)));
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

    ylnfcheck_parameter(_is_arr_type(ylcar(e)));
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
