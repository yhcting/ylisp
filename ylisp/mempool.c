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



#include "mempool.h"
#include "stack.h"
#include "gsym.h"

typedef struct {
    unsigned int i;     /**< index of free block pointer */
    yle_t        e;
} _blk_t;

/*
 * memory pool
 *     * U(used) / F(free)
 *
 *                fbp
 *             +-------+
 *             |   U   | <- index [ylmpsz()-1]
 *             +-------+
 *             |   U   |
 *             +-------+
 *             |  ...  |
 *             +-------+
 *             |   U   |
 *             +-------+
 *      fbi -> |   U   |
 *             +-------+
 *             |   F   |
 *             +-------+
 *             |  ...  |
 *             +-------+
 *             |   F   |
 *             +-------+
 *             |   F   | <- index [0]
 *             +-------+
 *
 */
static struct {
    _blk_t*    pool;    /**< pool */
    yle_t**    fbp;     /**< Free Block Pointers */
    int        fbi;     /**< Free Block Index - grow to bottom */
} _m; /**< s-Expression PooL */

/*
 * Stack is enough!
 * Usually, "ylmp_rm_bb" is very close with "ylmp_add_bb".
 * So, searching backward can be very efficient!
 * And there are not many blocks to adjust array (because it's very close to top!)
 */
ylstk_t* _bbs;       /**< Base-Block-Stack */

static inline int
_used_block_count() { return ylmpsz() - _m.fbi; }

/*
 * Memory usage ratio! (percent.)
 */
static inline int
_usage_ratio() { return _used_block_count() * 100 / ylmpsz(); }

yle_t*
ylmp_block() {
    if(_m.fbi <= 0) {
        yllogE1("Not enough Memory Pool.. Current size is %d\n", ylmpsz());
        ylassert(0);
        return NULL; /* to make compiler happy */
    } else {
        yle_t*  e;
        --_m.fbi;
        /* it's taken out from pool! */
        e = _m.fbp[_m.fbi];
        dbg_mem(e->evid = yleval_id(););
        /*
         * initialize block (make car and cdr be NULL)
         * (See comments ylmp_clean_block)
         * => This is important!
         */
        ylmp_clean_block(e);
        return e;
    }
}

void
ylmp_add_bb(yle_t* e) {
    ylstk_push(_bbs, e);
}

/*
 * This modifies stack directly!
 */
void
ylmp_rm_bb(yle_t* e) {
    register int i = ylstk_size(_bbs)-1; /* top */
    for(;i>=0; i--) {
        if(_bbs->item[i] == (void*)e) {
            /* we found! */
            memmove(&_bbs->item[i], &_bbs->item[i+1],
                    sizeof(_bbs->item[i])*(ylstk_size(_bbs)-i-1));
            _bbs->sz--;
            return; /* done! */
        }
    }
    if(i<0) {
        yllogW1("WARN!! Try to remove unregistered base block! : %p\n", e);
    }
}

void
ylmp_clean_bb() {
    ylstk_clean(_bbs);
}

/* =========================
 * GC !!! (START)
 * =========================*/

static void
_clean_block(yle_t* e) {
    ylassert(e != ylnil() && e != ylt() && e != ylq());
    yleclean(e);

    /* swap free block pointer */
    { /* Just scope */
        _blk_t* b1 = container_of(e, _blk_t, e);
        _blk_t* b2 = container_of(_m.fbp[_m.fbi], _blk_t, e);
        unsigned int ti; /* Temp I */

        /* swap fbp index */
        ti = b1->i; b1->i = b2->i; b2->i = ti;

        /* set fbp accordingly */
        _m.fbp[b1->i] = &b1->e;
        _m.fbp[b2->i] = &b2->e;
    }
    _m.fbi++;
}


_DEF_VISIT_FUNC(static, _gcmark, ,!yleis_gcmark(e), yleset_gcmark(e))

static void
_gc() {
    unsigned int  cnt, ratio_sv;
    int           i;
    yle_t*        e;

    /* clear all GC mark */
    for(i=_m.fbi; i<ylmpsz(); i++) {
        yleclear_gcmark(_m.fbp[i]);
    }

    /* we should keep memory blocks reachable from base blocks */
    stack_foreach(_bbs, e, i) {
        _gcmark(NULL, e);
    }

    /* memory blocks reachable from global symbol should be preserved */
    ylgsym_gcmark();

    ratio_sv = _usage_ratio();
    cnt = 0;
    /* Collect unmarked memory blocks */
    for(i=_m.fbi; i<ylmpsz(); i++) {
        if(!yleis_gcmark(_m.fbp[i])) {
            cnt++;
            _clean_block(_m.fbp[i]);
        }
    }
    yllogI4("GC Triggered (%d\% -> %d\%) :\n"
            "%d blocks collected\n"
            "bbs stack size : %d\n",
            ratio_sv, _usage_ratio(),
            cnt, ylstk_size(_bbs));
}

void
ylmp_gc_if_needed() {
    if(_usage_ratio() >= ylgctp()) {
        _gc();
    }
}

/* =========================
 * GC !!! (END)
 * =========================*/
void
ylmp_gc() {
    _gc();
}


ylerr_t
ylmp_init() {
    /* init memory pool */
    register int i;

    /* initialise pointers requiring mem. alloc. */
    _m.pool = NULL; _m.fbp = NULL;
    _bbs = NULL;

    /* allocated memory pool */
    _m.pool = ylmalloc(sizeof(_blk_t) * ylmpsz());
    if(!_m.pool) { goto bail; }
    _m.fbp = ylmalloc(sizeof(yle_t*) * ylmpsz());
    if(!_m.fbp) { goto bail; }
    _bbs = ylstk_create(ylmpsz()/2, NULL);
    if(!_bbs) { goto bail; }

    _m.fbi = ylmpsz(); /* from start */

    for(i=0; i<ylmpsz(); i++) {
        ylmp_clean_block(&_m.pool[i].e);
        _m.fbp[i] = &_m.pool[i].e;
        _m.pool[i].i = i;
    }
    return YLOk;

 bail:
    if(_m.pool) { ylfree(_m.pool); }
    if(_m.fbp)  { ylfree(_m.fbp); }
    if(_bbs)    { ylstk_destroy(_bbs); }
    return YLErr_out_of_memory;
}

void
ylmp_deinit() {
    /* force gc for deinit */
    ylmp_clean_bb();
    _gc();
    if(_bbs)    { ylstk_destroy(_bbs); }
    if(_m.pool) { ylfree(_m.pool); }
    if(_m.fbp)  { ylfree(_m.fbp); }
}

