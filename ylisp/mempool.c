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


/**************************************
 *
 * !!! NOTE - VERY IMPORTANT !!!
 * Memory blocks MUST be able to be freed ONLY by GC.!!!
 * Changing this design concept may cause side-effects
 *  those are almost impossible to guess.
 * (Lot's of implementations assume this - memory block is freed only by GC.)
 *
 **************************************/

#include "lisp.h"


typedef yle_t _mbtublk_t;

#include "blktbl.h"

/*
 * Memory pool.
 */
struct _mbt* _m;

/*
 * Stack is enough!
 * Usually, "ylmp_rm_bb" is very close with "ylmp_add_bb".
 * So, searching backward can be very efficient!
 * And there are not many blocks to adjust array
 * (because it's very close to top!)
 */
static ylstk_t*         _bbs;   /**< Base-Block-Stack */


/* condition - used at GC */
static pthread_cond_t   _condgc = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t  _mbbs;
static pthread_mutex_t  _mm;

static int              _gc_enabled = 1; /**< 0 means gc is disabled forcely */

/*
 * Memory usage ratio! (percent.)
 */
static inline int
_usage_ratio(void) { return _mbt_nr_used_blk(_m) * 100 / _m->sz; }

yle_t*
ylmp_block(void) {
	yle_t* e;
	_mlock(&_mm);
	e = _mbt_get(_m);
	_munlock(&_mm);
	if (!e) {
		/*
		 * size of _m is constant.
		 * So, we don't need to lock when read '_m->sz'
		 */
		yllogE("Not enough Memory Pool.. Current size is %d\n",
		       _m->sz);
		ylassert(0);
	} else {
		dbg_mem(e->evid = yleval_id(););
		/*
		 * initialize block (make car and cdr be NULL)
		 * (See comments ylmp_clean_block)
		 * => This is important!
		 */
		ylmp_clean_block(e);
	}
	return e;
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
	for (;i >= 0; i --) {
		if (_bbs->item[i] == (void*)e) {
			/* we found! */
			memmove(&_bbs->item[i], &_bbs->item[i+1],
				sizeof(_bbs->item[i])*(ylstk_size(_bbs)-i-1));
			_bbs->sz--;
			_munlock(&_mbbs);
			return; /* done! */
		}
	}
	_munlock(&_mbbs);
	if (i<0)
		yllogW("WARN!! Try to remove unregistered base block! : %p\n",
		       e);
}

void
ylmp_clean_bb(void) {
	_mlock(&_mbbs);
	ylstk_clean(_bbs);
	_munlock(&_mbbs);
}

/* =========================
 * GC !!! (START)
 * =========================*/
static void
_clean_block(yle_t* e) {
	ylassert(e != ylnil() && e != ylt() && e != ylq());
	yleclean(e);
	_mbt_put(_m, e);
}


_DEF_VISIT_FUNC(static, _gcmark, ,!yleis_gcmark(e), yleset_gcmark(e))

static int
_gc_perthread_mark(void* user, yletcxt_t* cxt) {
	ylslu_gcmark(cxt->slut);
	return 1; /* keep going to the end */
}

/*
 * Pre-condition
 *    - mthread module is locked!
 */
static void
_gc(void) {
	unsigned int  cnt __attribute__ ((unused));
	unsigned int  ratio_sv __attribute__ ((unused));
	int           i;
	yle_t*        e;

	/* clear all GC mark */
	_mbt_foreach_used(_m, i, e)
		yleclear_gcmark(e);

	_mlock(&_mbbs);
	/* we should keep memory blocks reachable from base blocks */
	stack_foreach(_bbs, e, i)
		_gcmark(NULL, e);

	_munlock(&_mbbs);

	/*
	 * memory blocks reachable from each thread symbol table
	 *   should be preserved
	 */
	ylmt_walk_locked(NULL, NULL, &_gc_perthread_mark);

	/* memory blocks reachable from global symbol should be preserved */
	ylgsym_gcmark();

	ratio_sv = _usage_ratio();
	cnt = 0;
	/* Collect unmarked memory blocks */
	_mbt_foreach_used(_m, i, e)
		if (!yleis_gcmark(e)) {
			cnt++;
			_clean_block(e);
		}

	yllogD("GC Triggered (%d\% -> %d\%) :\n"
	       "%d blocks collected\n"
	       "bbs stack size : %d\n",
	       ratio_sv, _usage_ratio(),
	       cnt, ylstk_size(_bbs));
}

static void
_mt_listener_pre_add(const yletcxt_t* cxt) {
	/*
	 * While GC, new thread may start.
	 * This new thread may access memory block and change something!
	 * (We need to avoid this! LOCK!)
	 */
	_mlock(&_mm);
}

static void
_mt_listener_post_add(const yletcxt_t* cxt) {
	_munlock(&_mm);
}

static void
_mt_listener_pre_rm(const yletcxt_t* cxt) {
	/* nothing to do */
}

static void
_mt_listener_post_rm(const yletcxt_t* cxt) {
	/* nothing to do */
}

static void
_mt_listener_thd_safe(const yletcxt_t* cxt, pthread_mutex_t* mtx) {
	/* try GC */
	int   btry;
	_mlock(&_mm);
	btry = (_usage_ratio() >= ylgctp());
	_munlock(&_mm);
	if (btry) {
		dbg_mutex(yllogD("+CondWait : TryGC ..."););
		if (pthread_cond_wait(&_condgc, mtx))
			ylassert(0);
		dbg_mutex(yllogD("OK\n"););
	}
}

static void
_mt_listener_all_safe(pthread_mutex_t* mtx) {
	_mlock(&_mm);
	if (_usage_ratio() >= ylgctp()) {
		if (_gc_enabled)
			_gc();
		else
			yllogW(
"Memory is running out! But GC is disabled!!!\n"
			       );
		_munlock(&_mm);
		dbg_mutex(yllogD("+CondBroadcast : GC done\n"););
		pthread_cond_broadcast(&_condgc);
	} else {
		_munlock(&_mm);
	}
}

/* =========================
 * GC !!! (END)
 * =========================*/

static ylmtlsn_t _mtlsnr = {
	&_mt_listener_pre_add,
	&_mt_listener_post_add,
	&_mt_listener_pre_rm,
	&_mt_listener_post_rm,
	&_mt_listener_thd_safe,
	&_mt_listener_all_safe,
};

int
ylmp_gc_enable(int v) {
	int sv;
	_mlock (&_mm);
	sv = _gc_enabled;
	_gc_enabled = v;
	_munlock(&_mm);
	return sv;
}

static ylerr_t
_mod_init(void) {
	/* init memory pool */
	register int i;
	yle_t*       e;

	/* initialise pointers requiring mem. alloc. */
	_bbs = NULL;

	pthread_mutex_init(&_mm, ylmutexattr());
	pthread_mutex_init(&_mbbs, ylmutexattr());

	/* allocated memory pool */
	_m = _mbt_create(ylmpsz());
	if (!_m)
		goto bail_m;
	_bbs = ylstk_create(_m->sz/2, NULL);
	if (!_bbs)
		goto bail_bbs;

	_mbt_foreach(_m, i, e)
		ylmp_clean_block(e);

	/* register to mt module to support Muti-Threading */
	ylmt_register_listener(&_mtlsnr);

	return YLOk;

 bail_bbs:
	_mbt_destroy(_m);
 bail_m:
	return YLErr_out_of_memory;
}

static ylerr_t
_mod_exit(void) {
	int    i;
	yle_t* e;
	_mlock(&_mm);
	/* Free all elements */
	_mbt_foreach_used(_m, i, e)
		yleclean(e);

	if (_bbs)
		ylstk_destroy(_bbs);

	_mbt_destroy(_m);

	pthread_mutex_destroy(&_mbbs);
	_munlock(&_mm);
	pthread_mutex_destroy(&_mm);
	return YLOk;
}

YLMODULE_INITFN(mp, _mod_init)
YLMODULE_EXITFN(mp, _mod_exit)

