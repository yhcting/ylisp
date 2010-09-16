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



#include <memory.h>
#include "mempool.h"
#include "stack.h"

/*
 * Full scan is trigerred when memory usage is over than _FULLSCAN_TRIGGER_POINT
 *   and current interpreting stack in empty (topmost interpreting stack).
 * But if fullscan fail to gather garbages more than (_MIN_FULLSCAN_EFFECT)% memory,
 *   program notifies 'memory shortage'!
 * (100 - _FULL_SCAN_TRIGGER_POINT)% is spare-buffer to run command till out from interpreting stack.
 */
#define _FULLSCAN_TRIGGER_POINT       80  /* percent */
#define _PARTIALSCAN_TRIGGER_POINT    95  /* percent */


/* memory pool */
static struct {
    yle_t      pool[MPSIZE];    /**< pool */
    yle_t*     fbis[MPSIZE];    /**< Free Block IndexS */
    yle_t*     ubis[MPSIZE];    /**< Used Block IndexS */
    /* for easy sanity check, below two should be grow in opposite direction */
    int        fbi;             /**< Free Block Index - grow to bottom */
    int        ubi;             /**< Used Block Index - grow to top */
} _epl; /**< s-Expression PooL */

static struct {
    unsigned int    hwm;
} _stat;

static ylstk_t*   _ststk = NULL; /* STate STacK */

static inline void
_set_chain_reachable(yle_t* e) {
    yleset_reachable(e);
    if(!yleis_atom(e)) {
        _set_chain_reachable(ylcar(e));
        _set_chain_reachable(ylcdr(e));
    }
}

static inline int
_used_block_count() { return MPSIZE - _epl.fbi; }

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
    _epl.fbi = MPSIZE; /* from start */
    _epl.ubi = 0;
    for(i=0; i<MPSIZE; i++) {
        _mark_as_free(&_epl.pool[i]);
        _epl.fbis[i] = &_epl.pool[i];
    }
    _ststk = ylstk_create(MAX_EVAL_DEPTH, NULL);
    if(!_ststk) { return YLErr_out_of_memory;  }
    _stat.hwm = 0;
    return YLOk;
}

void
ylmp_deinit() {
    ylstk_destroy(_ststk);
}

yle_t*
ylmp_get_block() {
    if(_epl.fbi <= 0) {
        yllogE1("Not enough Memory Pool.. Current size is %d\n", MPSIZE);
        ylassert(0);
    } else {
        --_epl.fbi;
        /* it's taken out from pool! */
        ylercnt(_epl.fbis[_epl.fbi]) = 0;
        dbg_mem(_epl.fbis[_epl.fbi]->evid = ylget_eval_id(););

        _epl.ubis[_epl.ubi] = _epl.fbis[_epl.fbi];
        _epl.ubi++;
        if(_epl.ubi >= MPSIZE) {
            yllogE0("Not enough used block checker..\n");
            ylassert(0);
        }

        if(_used_block_count() > _stat.hwm) { _stat.hwm = _used_block_count(); }
        return _epl.fbis[_epl.fbi];
    }
}

void
__ylmp_release_block(yle_t* e) {
    /* ylassert(!ylercnt(e)); - sometimes force-release is required! */
    ylassert(e != ylnil() && e != ylt() && e != ylq());
    /*
     * Before call yleclean, we should mark that this block is free block.
     * If not, following unexpected case may happen!
     *    - In case of cross-reference, if block is not set as free, infinite loop of 'release' may happen!
     */
    _mark_as_free(e);
    yleclean(e);
    memset(e, 0, sizeof(yle_t));
    _mark_as_free(e); /* memset cleared mark. So, mark it again */
    _epl.fbis[_epl.fbi] = e;
    _epl.fbi++;
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
 *    Partial-scan (bpartial == TRUE)
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
_scanning_gc(ylmp_gcscanty_t ty) {
    register int     i;
    int              j; 

    /* for logging */
    int              sv = _usage_ratio();

    /* check all blocks as NOT-REACHABLE (initialize) */
    for(i=0; i<MPSIZE; i++) {
        yleclear_reachable(&_epl.pool[i]);
    }

    /* mark memory blocks that can be reachable from the Trie.*/
    yltrie_mark_reachable();

    /* mark Reachable to blocks that are newly allocated at this interpret transaction */
    if(YLMP_GCSCAN_PARTIAL == ty) {
        for(i=0; i<_epl.ubi; i++) {
            yleset_reachable(_epl.ubis[i]);
        }
    }

    /* Collect memory blocks those are not reachable (dangling) from the Trie! */
    for(i=0; i<MPSIZE; i++) {
        /* We need to find blocks that is dangling but not in the pool. */
        if( !yleis_reachable(&_epl.pool[i])
            && !ylmp_is_free_block(&_epl.pool[i]) ) {
            /* 
             * This is dangling - may be cross referred...
             * We need to check it... why this happenned..
             */
            dbg_mem(yllogV2("--> FSGC : Dangling Block \n"
                            "        block index : %d\n"
                            "        eval id     : %d\n"
                            , i
                            , _epl.pool[i].evid););

            __ylmp_release_block(&_epl.pool[i]);
        }
    }

    ylassert(sv >= _usage_ratio());
    yllogI2("\n=== Result of Scanning GC\n"
            "    Usage ratio before: %d\%\n"
            "    Usage ratio after : %d\%\n",
            sv, _usage_ratio());
    /* 
     * We don't handle ubi/ubis here! 
     * Those should be handled at somewhere else.
     */
}

unsigned int
ylmp_stack_size() {
    return ylstk_size(_ststk);
}

void
ylmp_push() {
    ylstk_push(_ststk, (void*)_epl.ubi);
}


void
ylmp_pop() {
    register int   i, pubi; /* pubi : previous ubi */

    pubi = (int)ylstk_pop(_ststk);
    /* uncheck reachable bit(initialize) all used block used between push&pop */
    ylassert(pubi <= _epl.ubi);
            

    /* filter trivial case - For performance! */
    if(pubi == _epl.ubi) {
        /* nothing to do. There is no newly allocated memory block */
        return; 
    }

    /* Start Garbage Collection for newly allocated blocks */
    for(i=pubi; i<_epl.ubi; i++) {
        if( ylmp_is_free_block(_epl.ubis[i]) ) {
            /* nothing to do */
            continue;
        } else if ( !ylercnt(_epl.ubis[i]) ) {
            __ylmp_release_block(_epl.ubis[i]);
        } else {
            /* packing array */
            _epl.ubis[pubi++] = _epl.ubis[i];
        }
    }
    _epl.ubi = pubi;

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
    if(!ylstk_size(_ststk)) {
        /* start from the first */
        _epl.ubi = 0;
        if(_usage_ratio() > _FULLSCAN_TRIGGER_POINT) {
            _scanning_gc(YLMP_GCSCAN_FULL);
        }
    } else if(_usage_ratio() > _PARTIALSCAN_TRIGGER_POINT) {
        /*
         * I think this doesn't have any side effect.
         * But I'm not sure 100%.
         * It's very complicated and sensitive...
         * Let's keep eyes on it.
         */
        _scanning_gc(YLMP_GCSCAN_PARTIAL);
    }
}


void
ylmp_scan_gc(ylmp_gcscanty_t ty) {
    _scanning_gc(ty);
}

void
ylmp_recovery_gc() {
    if(ylstk_size(_ststk) > 0) {
        ylstk_clean(_ststk);
        _epl.ubi = 0;
    }
    /* It's a trigger full scan gc */
    _scanning_gc(YLMP_GCSCAN_FULL);
}

unsigned int
ylmp_usage() {
    return _epl.ubi;
}


#define __PR_STAT                               \
    ">>>> Memory Pool Statistics\n"             \
        "        TOTAL   : %d\n"                \
        "        HWM     : %d (%d\%)\n"         \
        "        USED    : %d (%d\%)\n"         \
        "        FREE    : %d (%d\%)\n"         \
        "        ubi     : %d (%d\%)\n"         \
        , MPSIZE                                \
        , _stat.hwm, _stat.hwm * 100 / MPSIZE   \
        , _used_block_count(), _usage_ratio()   \
        , _epl.fbi, _epl.fbi * 100 / MPSIZE     \
        , _epl.ubi, _epl.ubi * 100 / MPSIZE

void
ylmp_print_stat() {
    ylprint((__PR_STAT));
}

void
ylmp_log_stat(int loglevel) {
    yllog((loglevel, __PR_STAT));
}

#undef __PR_STAT
