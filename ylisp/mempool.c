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

/*
 * Full scan is trigerred when memory usage is over than _FULLSCAN_TRIGGER_POINT
 *   and current interpreting stack in empty (topmost interpreting stack).
 * (100 - _FULL_SCAN_TRIGGER_POINT)% is spare-buffer to run command till out from interpreting stack.
 */
#define _FULLSCAN_TRIGGER_POINT       80  /* percent */

typedef struct {
    unsigned int i;   /**< index of free block pointer */
    yle_t        e;
} _blk_t;

/* memory pool */
static struct {
    _blk_t     pool[MPSIZE];    /**< pool */
    yle_t*     fbp[MPSIZE];     /**< Free Block Pointers */
    int        fbi;             /**< Free Block Index - grow to bottom */
} _m; /**< s-Expression PooL */

static struct {
    unsigned int    hwm;
} _stat;

/* To handle push/pop of allocated memory block */
static struct {
    yle_t*     p[MPSIZE];    /**< Block Pointers */
    ylstk_t*   s;            /**< state Stack - store allocated block indexs */
    int        i;            /**< used block Index - grow to top */
} _ab; /* Allocated Block */

static inline unsigned int
_fbpi(yle_t* e) {
    return container_of(e, _blk_t, e)->i;
}

static inline void
_set_chain_reachable(yle_t* e) {
    yleset_reachable(e);
    if(!yleis_atom(e)) {
        _set_chain_reachable(ylcar(e));
        _set_chain_reachable(ylcdr(e));
    }
}

static inline int
_used_block_count() { return MPSIZE - _m.fbi; }

/*
 * Memory usage ratio! (percent.)
 */
static inline int
_usage_ratio() { return _used_block_count() * 100 / MPSIZE; }


static void
_mark_as_free(yle_t* e) { ylercnt(e) = YLEINVALID_REFCNT; }


ylerr_t
ylmp_init() {
    register int i;
    /* init memory pool */
    _m.fbi = MPSIZE; /* from start */
    _ab.i = 0;
    for(i=0; i<MPSIZE; i++) {
        _mark_as_free(&_m.pool[i].e);
        _m.fbp[i] = &_m.pool[i].e;
        _m.pool[i].i = i;
    }
    _ab.s = ylstk_create(0, NULL);
    if(!_ab.s) { return YLErr_out_of_memory;  }
    _stat.hwm = 0;
    return YLOk;
}

void
ylmp_deinit() {
    ylstk_destroy(_ab.s);
}

yle_t*
ylmp_get_block() {
    if(_m.fbi <= 0) {
        yllogE1("Not enough Memory Pool.. Current size is %d\n", MPSIZE);
        ylassert(0);
    } else {
        --_m.fbi;
        /* it's taken out from pool! */
        ylercnt(_m.fbp[_m.fbi]) = 0;
        dbg_mem(_m.fbp[_m.fbi]->evid = ylget_eval_id(););

        _ab.p[_ab.i] = _m.fbp[_m.fbi];
        _ab.i++;
        if(_ab.i >= MPSIZE) {
            yllogE0("Not enough used block checker..\n");
            ylassert(0);
        }

        if(_used_block_count() > _stat.hwm) { _stat.hwm = _used_block_count(); }
        return _m.fbp[_m.fbi];
    }
}

void
__ylmp_release_block(yle_t* e) {
    ylassert(e != ylnil() && e != ylt() && e != ylq()
             && _m.fbp[_fbpi(e)] == e ); /* sanity check */

    /*
     * Before call yleclean, we should mark that this block is free block.
     * If not, following unexpected case may happen!
     *    - In case of cross-reference, if block is not set as free, infinite loop of 'release' may happen!
     */
    _mark_as_free(e);
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

/*
 * GC with reference count has limitation.
 * This way cannot detect dangling-cyclic-block.
 * (ex. A->B->A : A and B has at least 1 reference count. But, those are dangling!)
 * So, sometimes we need to scan fully to dectect this kind of danglings!
 * ISSUE!
 *    Where is best place to put this function?
 *       - Calling this function is extreamly sensitive!
 *         Keeping base register for GC is very difficult to implementation due to handling return value!
 *
 * Mechanism.
 *    Keep in mind that, scanning GC is very expensive operation!
 *    Full-scan (bpartial == FALSE)
 *        This scans all memory blocks and keeps only blocks reachable from Trie space.
 *        So, using this during interpret transaction, is very dangerous.
 *        Therefore, this case should be used at the topmost of interpreting stack or in case of interpreting error!!.
 *    Partial-scan (bpartial == TRUE) -- Obsolete(Not useful. But hard to understand)
 *        This is less-effective but can be used at more variable cases.
 *        Following blocks are preserved.
 *            - Blocks that are newly allocated at current interpret transaction.
 *              (And definitely reference should be larger than 0)
 *            - Blocks reachable from Trie space.
 *        Because newly allocated blocks of current transaction are not GCed, This can be used at the end of evaluation even if it's not topmost!.
 *        !!WARNING!!
 *            Using this in the middle of evaluation, SHOULD BE VERY VERY CAREFUL!!!!
 *            (Actually, HIGHLY NOT RECOMMENDED, IF POSSIBLE!)
 */
static void
_scanning_gc() {
    register int     i;
    int              j; 

    /* for logging */
    int              sv = _usage_ratio();

    /* check for used blocks are enough */

    /* check all used blocks as NOT-REACHABLE */
    for(i=_m.fbi; i<MPSIZE; i++) {
        yleclear_reachable(_m.fbp[i]);
    }

    /* mark memory blocks that can be reachable from the Trie.*/
    yltrie_mark_reachable();

    /* Collect memory blocks those are not reachable (dangling) from the Trie! */
    for(i=_m.fbi; i<MPSIZE; i++) {
        /* We need to find blocks that is dangling but not in the pool. */
        if( !ylmp_is_free_block(_m.fbp[i])
            && !yleis_reachable(_m.fbp[i]) ) {
            /* 
             * This is dangling - may be cross referred...
             * We need to check it... why this happenned..
             */
            dbg_mem(yllogV2("--> FSGC : Dangling Block \n"
                            "        block index : %d\n"
                            "        eval id     : %d\n"
                            , i
                            , _m.pool[i].e.evid););

            __ylmp_release_block(_m.fbp[i]);
        }
    }

    ylassert(sv >= _usage_ratio());
    yllogI2("\n=== Result of Scanning GC\n"
            "    Usage ratio before: %d\%\n"
            "    Usage ratio after : %d\%\n",
            sv, _usage_ratio());
    /* 
     * We don't handle _ab.i/_ab.p here! 
     * Those should be handled at somewhere else.
     */
}

unsigned int
ylmp_stack_size() {
    return ylstk_size(_ab.s);
}

void
ylmp_push() {
    ylstk_push(_ab.s, (void*)_ab.i);
}


void
ylmp_pop() {
    register int   i, pabi; /* pabi : previous _ab.i */

    pabi = (int)ylstk_pop(_ab.s);
    /* uncheck reachable bit(initialize) all used block used between push&pop */
    ylassert(pabi <= _ab.i);
            

    /* filter trivial case - For performance! */
    if(pabi == _ab.i) {
        /* nothing to do. There is no newly allocated memory block */
        return; 
    }

    /* Start Garbage Collection for newly allocated blocks */
    for(i=pabi; i<_ab.i; i++) {
        if( ylmp_is_free_block(_ab.p[i]) ) {
            /* nothing to do */
            continue;
        } else if ( !ylercnt(_ab.p[i]) ) {
            __ylmp_release_block(_ab.p[i]);
        } else {
            /* packing array */
            _ab.p[pabi++] = _ab.p[i];
        }
    }
    _ab.i = pabi;

    /* Sanity Check */
    if( ylercnt(ylnil()) > 0x7fffffff
        || ylercnt(ylt()) > 0x7fffffff
        || ylercnt(ylq()) > 0x7fffffff) {
        yllogE0("Oops in memory pool...\n"
                "reference count of one of predefined symbols is too big...\n"
                "Definitely, there is unexpected error somewhere of memory block management!\n");
        ylassert(0);
    }


    /* special operation, when out from interpreting stack! */
    if(!ylstk_size(_ab.s)) {
        /* start from the first */
        _ab.i = 0;
        if(_usage_ratio() > _FULLSCAN_TRIGGER_POINT) {
            _scanning_gc();
        }
    }
}


#ifdef __DBG_MEM
void
yldbg_mp_gc() {
    _scanning_gc();
}
#endif /* __DBG_MEM */

void
ylmp_gc() {
    if(ylstk_size(_ab.s) > 0) {
        ylstk_clean(_ab.s);
        _ab.i = 0;
    }
    /* It's a trigger full scan gc */
    _scanning_gc();
}

unsigned int
ylmp_usage() {
    return _ab.i;
}


#define __PR_STAT                               \
    ">>>> Memory Pool Statistics\n"             \
        "        TOTAL   : %d\n"                \
        "        HWM     : %d (%d\%)\n"         \
        "        USED    : %d (%d\%)\n"         \
        "        FREE    : %d (%d\%)\n"         \
        "        abi     : %d (%d\%)\n"         \
        , MPSIZE                                \
        , _stat.hwm, _stat.hwm * 100 / MPSIZE   \
        , _used_block_count(), _usage_ratio()   \
        , _m.fbi, _m.fbi * 100 / MPSIZE     \
        , _ab.i, _ab.i * 100 / MPSIZE

void
ylmp_print_stat() {
    ylprint((__PR_STAT));
}

void
ylmp_log_stat(int loglevel) {
    yllog((loglevel, __PR_STAT));
}

#undef __PR_STAT
