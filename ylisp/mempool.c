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


#include "lisp.h"

typedef struct {
    unsigned int i;     /**< index of free block pointer */
    yle_t        e;
} _blk_t;

typedef struct {
    yllist_link_t   lk;
    yletcxt_t*      cxt;
    int             mark;
} _cxt_t;

#define _MKSAFE          0x01 /* mark ethread context is in safe state */
#define _mkset(c, m)     do { (c)->mark |=  m; } while(0)
#define _mkclear(c, m)   do { (c)->mark &= ~m; } while(0)
#define _mkisset(c, m)   ((c)->mark & m)

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
static ylstk_t*         _bbs;   /**< Base-Block-Stack */
static yllist_link_t    _cxtl;  /**< thread context list */   
static int              _gctrigger = 0; /* flag - used to trigger GC manually */


/* condition - used at GC */
pthread_cond_t          _condgc = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t  _mbbs;
static pthread_mutex_t  _mm;

static inline int
_used_block_count() { return ylmpsz() - _m.fbi; }

/*
 * Memory usage ratio! (percent.)
 */
static inline int
_usage_ratio() { return _used_block_count() * 100 / ylmpsz(); }

yle_t*
ylmp_block() {
    _mlock(&_mm);
    if(_m.fbi <= 0) {
        _munlock(&_mm);
        yllogE1("Not enough Memory Pool.. Current size is %d\n", ylmpsz());
        ylassert(0);
        return NULL; /* to make compiler happy */
    } else {
        yle_t*  e;
        --_m.fbi;
        /* it's taken out from pool! */
        e = _m.fbp[_m.fbi];
        _munlock(&_mm);

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
    _mlock(&_mbbs);
    ylstk_push(_bbs, e);
    _munlock(&_mbbs);
}

/*
 * This modifies stack directly!
 */
void
ylmp_rm_bb(yle_t* e) {
    register int i;
    _mlock(&_mbbs);
    i = ylstk_size(_bbs)-1; /* top */
    for(;i>=0; i--) {
        if(_bbs->item[i] == (void*)e) {
            /* we found! */
            memmove(&_bbs->item[i], &_bbs->item[i+1],
                    sizeof(_bbs->item[i])*(ylstk_size(_bbs)-i-1));
            _bbs->sz--;
            _munlock(&_mbbs);
            return; /* done! */
        }
    }
    _munlock(&_mbbs);
    if(i<0) {
        yllogW1("WARN!! Try to remove unregistered base block! : %p\n", e);
    }
}

void
ylmp_clean_bb() {
    _mlock(&_mbbs);
    ylstk_clean(_bbs);
    _munlock(&_mbbs);
}

/* =========================
 * GC !!! (START)
 * =========================*/
static inline void
_cxtmark_set(yletcxt_t* cxt, int mark) {
    _cxt_t*  ct;
    yllist_foreach_item(ct, &_cxtl, _cxt_t, lk) {
        if(ct->cxt == cxt) {
            _mkset(ct, mark);
            return;
        }
    }
    yllogW1("WARN! Try to set mark to unlisted context! : %p\n", cxt);
}

static inline void
_cxtmark_clear_all(int mark) {
    _cxt_t*  ct;
    yllist_foreach_item(ct, &_cxtl, _cxt_t, lk) {
        _mkclear(ct, mark);
    }
}

static int
_cxtmark_is_all_marked(int mark) {
    _cxt_t*  ct;
    yllist_foreach_item(ct, &_cxtl, _cxt_t, lk) {
        if(!_mkisset(ct, mark)) { return 0; }
    }
    return 1;
}

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

    _mlock(&_mbbs);
    /* we should keep memory blocks reachable from base blocks */
    stack_foreach(_bbs, e, i) {
        _gcmark(NULL, e);
    }
    _munlock(&_mbbs);

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

static inline void
_trigger_gc() {
    _gc();
    _gctrigger = 0;
    _cxtmark_clear_all(_MKSAFE);
    dbg_mutex(yllogI0("+CondBroadcast : GC done\n"););
    pthread_cond_broadcast(&_condgc);
}

static inline void
_try_gc(yletcxt_t* cxt) {
    _gctrigger = 1;
    _cxtmark_set(cxt, _MKSAFE);
    /* check all ethreads are in safe state? */
    if(_cxtmark_is_all_marked(_MKSAFE)) {
        _trigger_gc();
    } else {
        dbg_mutex(yllogI0("+CondWait : TryGC ..."););
        if(pthread_cond_wait(&_condgc, &_mm)) { ylassert(0); }
        dbg_mutex(yllogI0("OK\n"););
    }
}

void
ylmp_thread_context_add(yletcxt_t* cxt) {
    _cxt_t* ct = ylmalloc(sizeof(_cxt_t));
    ct->cxt = cxt;
    ct->mark = 0;
    /*
     * While GC new thread may start.
     * (We need to avoid this! - '_mm' is additionally used.)
     */
    _mlock(&_mm);
    yllist_add_last(&_cxtl, &ct->lk);
    _munlock(&_mm);
}

void
ylmp_thread_context_rm(yletcxt_t* cxt) {
    _cxt_t* ct;
    /* add/rm should be done in the same thread! */
    ylassert(pthread_self() == cxt->id);
    _mlock(&_mm);
    yllist_foreach_item(ct, &_cxtl, _cxt_t, lk) {
        if(ct->cxt == cxt) {
            yllist_del(&ct->lk);
            ylfree(ct);
            /*
             * - GC is triggered, but there are threads those are not in safe state.
             *   So, triggering thread waits.
             * - But, some of remaining threads already finish evaluation.
             *   So, exit without evaluation.
             * => In this case previous-waiting-threads are stucked!
             *   (To prevent this, we need to check gc here one more time.)
             *
             * [ GC is required? ] && [ all ethreads are in safe state? ]
             */
            if(_gctrigger && _cxtmark_is_all_marked(_MKSAFE)) {
                _trigger_gc();
            }
            goto done;
        }
    }
    yllogW1("Try to remove unlisted thread context : %p\n", cxt);
 done:
    _munlock(&_mm);
}

void
ylmp_notify_safe_state(yletcxt_t* cxt) {
    _mlock(&_mm);
    if(_gctrigger || _usage_ratio() >= ylgctp()) {
        _try_gc(cxt);
    }
    _munlock(&_mm);
}

/* =========================
 * GC !!! (END)
 * =========================*/

ylerr_t
ylmp_init() {
    /* init memory pool */
    register int i;

    /* initialise pointers requiring mem. alloc. */
    _m.pool = NULL; _m.fbp = NULL;
    _bbs = NULL;

    pthread_mutex_init(&_mm, ylmutexattr());
    pthread_mutex_init(&_mbbs, ylmutexattr());

    yllist_init_link(&_cxtl);

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
    int    i;
    _mlock(&_mm);
    /* Free all elements */
    for(i=_m.fbi; i<ylmpsz(); i++) {
        yleclean(_m.fbp[i]);
    }

    if(_bbs)    { ylstk_destroy(_bbs); }
    if(_m.pool) { ylfree(_m.pool); }
    if(_m.fbp)  { ylfree(_m.fbp); }

    ylassert(0 == yllist_size(&_cxtl));

    pthread_mutex_destroy(&_mbbs);
    _munlock(&_mm);
    pthread_mutex_destroy(&_mm);
}

