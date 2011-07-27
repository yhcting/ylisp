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


/******************************************
 * ASSUMPTION in this module!
 *    'ETST_SAFE' is used only in '_m' lock!
 ******************************************/

#include <signal.h>

#include "lisp.h"

struct _lsn {
	yllist_link_t    lk;
	const ylmtlsn_t* ops;
};

struct _pres {
	yllist_link_t    lk;
	void*            pres;
	void           (*ccb)(void*); /* close callback */
};
/*
 * Listener functions should not be between lock!
 * Why?
 * Some public interface functions (ex. ylmp_is_safe()) uses lock.
 * And, in the listener, these functions may be called.
 * This is cause of deadlock!
 */
#define _walk_listeners(OP, PARAM)					\
	do {								\
		struct _lsn* ___p;					\
		yllist_foreach_item(___p, &_lsnl, struct _lsn, lk) {	\
			(*___p->ops->OP)PARAM;				\
		}							\
	} while (0)

#define _walk_ethreads(exp)					\
	do {							\
		yletcxt_t* w;                                   \
		yllist_foreach_item(w, &_cxtl, yletcxt_t, lk) { \
			exp;					\
		}						\
	} while (0)


static pthread_mutex_t  _m;     /**< lock for thread management */

static yllist_link_t    _lsnl;  /**< listener list */
static yllist_link_t    _cxtl;  /**< thread context list */


static inline int
_etst_is_all(unsigned int state) {
    _walk_ethreads(
		   if (!etst_isset(w, state))
			   return 0;
		   );
    return 1;
}

/*===================================================================
 *
 * !!! DEADLOCK !!!
 * In listener function, client module may use its own mutex(lock / unlock).
 * But, 'mthread' module uses it's own lock - '_m'.
 * That is, two different mutex is used in same routine.(Typical case!)
 *    ==> We need to worry about deadlock.
 * To avoid deadlock, order to lock/unlock mutexs should be consistent
 *
 * Mutex order of 'mthread' is
 *    lock(_m) -> lock(<client's mutex>)
 *      / unlock(<client's mutex) -> unlock(_m)
 *
 * !! Case Study !!
 * In 'ylmt_add', switching order between '_mlock(&_m)'
 *   and '_walk_listeners(...)'
 *   may case deadlock due to inconsistent mutex order.
 *
 *===================================================================*/
void
ylmt_add(yletcxt_t* cxt) {
	_mlock(&_m);
	_walk_listeners(pre_add, (cxt));
	yllist_add_last(&_cxtl, &cxt->lk);
	_walk_listeners(post_add, (cxt));
	_munlock(&_m);
}

void
ylmt_rm(yletcxt_t* cxt) {
	_mlock(&_m);
	_walk_listeners(pre_rm, (cxt));
	yllist_del(&cxt->lk);
	_walk_listeners(post_rm, (cxt));

	/*
	 * Some thread are not reported that they are in safe state.
	 * Instead of it, those may finish their execution.
	 * In this case, checking safty at 'ylmt_notify_safe()'
	 *   CANNOT detect that current is safe.
	 * So, we need to check safty again at this point.
	 *
	 * See 'ylmt_notify_safe()' for more related comments.
	 */
	if (_etst_is_all(ETST_SAFE))
		_walk_listeners(all_safe, (&_m));

	_munlock(&_m);

	return;
}


void
ylmt_register_listener(const ylmtlsn_t* lsnr) {
	struct _lsn* p = ylmalloc(sizeof(struct _lsn));
	ylassert(p);
	p->ops = lsnr;
	yllist_add_last(&_lsnl, &p->lk);
}


unsigned int
ylmt_nr_pres(yletcxt_t* cxt) {
	return yllist_size(&cxt->pres);
}

void
ylmt_close_all_pres(yletcxt_t* cxt) {
	struct _pres *p, *n;
	yllist_foreach_item_removal_safe(p, n, &cxt->pres, struct _pres, lk) {
		yllist_del(&p->lk);
		ylprint ("close... resource %p\n", p->pres);
		(*p->ccb)(p->pres);
		ylfree(p);
	}
}

int
ylmt_add_pres(yletcxt_t* cxt, void* pres, void(*ccb)(void*)) {
	struct _pres* pr = ylmalloc(sizeof(struct _pres));
	ylassert(pres && ccb);
	ylassert(pr); /* very small size of memory. This should not fail! */
	pr->pres = pres;
	pr->ccb = ccb;
	/*
	 * like stack because recently added value is referenced frequently.
	 */
	yllist_add_first(&cxt->pres, &pr->lk);
	return 0;
}

int
ylmt_rm_pres(yletcxt_t* cxt, void* pres) {
	struct _pres* p;
	ylassert(pres);
	yllist_foreach_item(p, &cxt->pres, struct _pres, lk)
		/* to delete first item of list */
		break;
	yllist_del(&p->lk);
	ylfree(p);
	return 0;
}

int
ylmt_is_safe(yletcxt_t* cxt) {
	int ret;
	_mlock(&_m);
	ret = _etst_is_all(ETST_SAFE);
	_munlock(&_m);
	not_used(cxt);
	return ret;
}

void static
_handle_etsignal(yletcxt_t* cxt) {
}

/*
 * This can be performance bottleneck!
 * Be careful!
 *
 * !!! Further Thought !!!
 *    We may use 'mark count' for ETST_SAFE,
 *      to reduce one-list-search for '_etst_is_all'
 *    So, 'mark_count' can make complexity of 'ylmt_notify_safe()'
 *      from O(n) to O(1)!.
 *
 *    If needed, consider it!
 *    But, most cases, Simple is Better!!
 *    Keep simple as long as possible!
 */
void
ylmt_notify_safe(yletcxt_t* cxt) {
	/*
	 * SHOULD NOT use 'ylmt_is_safe()' here!
	 * We SHOULD KEEP SAFE while 'all_safe' is called.
	 * So, mutex '_m' SHOULD be kept locking to make this be atomic!
	 * Case
	 *    Threads are all in safe. So, thread A walks 'all_safe' listeners.
	 *    During this, thread B goes out from 'ylmt_notify_safe'.
	 *    (thread B is not safe anymore).
	 *    => walking 'all_safe' in thread A may give unexpected result!.
	 */
	_mlock(&_m);

	/*
	 * We don't need to use 'cxt->m' lock to mark 'ETST_SAFE'.
	 * 'ETST_SAFE' is used only in '_m' lock!
	 */
	etst_set(cxt, ETST_SAFE);
	if (_etst_is_all(ETST_SAFE))
		_walk_listeners(all_safe, (&_m));
	else
		_walk_listeners(thd_safe, (cxt, &_m));

	/* Handle special signal! */
	if (etsig_isset(cxt, ETSIG_KILL)) {
		etst_clear(cxt, ETST_SAFE);
		_munlock(&_m);
		ylinterpret_undefined (YLErr_killed);
	} else
		_handle_etsignal(cxt);
	_munlock(&_m);
}

void
ylmt_notify_unsafe(yletcxt_t* cxt) {
	_mlock(&_m);
	etst_clear(cxt, ETST_SAFE);
	_munlock(&_m);
}

static inline void
_kill(yletcxt_t* cxt) {
	/*
	 *=========================
	 * ASSUMPTION. - IMPORTANT!
	 *=========================
	 *    One CNF spwaning only ONE child process!
	 */
	_mlock(&cxt->m);

	if (etst_isset(cxt, ETST_SAFE))
		/* target thread is in safe state. We can kill it! */
		pthread_cancel((pthread_t)ylstk_peek(cxt->thdstk));
	else
		etsig_set(cxt, ETSIG_KILL);

	ylmt_close_all_pres(cxt);

	_munlock(&cxt->m);
}

int
ylmt_kill(yletcxt_t* cxt, pthread_t tid) {
	int ret = -1;
	/* Killing self is not allowed! */
	if (cxt->base_id == tid || INVALID_TID == tid)
		return -1;
	_mlock(&_m);
	_walk_ethreads(
		       if (w->base_id == tid) {
			       _kill(w);
			       ret = 0;
			       break;
		       });
	_munlock(&_m);
	return ret;
}

void
ylmt_walk(yletcxt_t* cxt, void* user,
           /* return 1 to keep going, 0 to stop */
          int(*cb)(void*, yletcxt_t* cxt)) {
	_mlock(&_m);
	_walk_ethreads(
		       if (!(*cb)(user, w))
			       break;
		       );
	_munlock(&_m);
	not_used(cxt);
}

void
ylmt_walk_locked(yletcxt_t* cxt, void* user,
                 /* return 1 to keep going, 0 to stop */
                 int(*cb)(void*, yletcxt_t* cxt)) {
	_walk_ethreads(
		       if (!(*cb)(user, w))
			       break;
		       );
	not_used(cxt);
}

static ylerr_t
_mod_init(void) {
	yllist_init_link(&_cxtl);
	yllist_init_link(&_lsnl);
	pthread_mutex_init(&_m, ylmutexattr());
	return YLOk;
}

static ylerr_t
_mod_exit(void) {
	struct _lsn *p, *tmp;
	ylassert(0 == yllist_size(&_cxtl));
	pthread_mutex_destroy(&_m);
	yllist_foreach_item_removal_safe(p, tmp, &_lsnl, struct _lsn, lk) {
		yllist_del(&p->lk);
		ylfree(p);
	}
	return YLOk;
}

YLMODULE_INITFN(mt, _mod_init)
YLMODULE_EXITFN(mt, _mod_exit)
