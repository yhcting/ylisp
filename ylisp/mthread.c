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
    yllist_link_t       lk;
    const yletcxt_t*    cxt;
    int                 mark;
} _cxt_t;

typedef struct {
    yllist_link_t           lk;
    const ylmtlsn_t*        ops;
} _lsn_t;

#define _MKSAFE          0x01 /* mark ethread context is in safe state */
#define _mkset(c, m)     do { (c)->mark |=  m; } while(0)
#define _mkclear(c, m)   do { (c)->mark &= ~m; } while(0)
#define _mkisset(c, m)   ((c)->mark & m)


/*
 * Listener functions should not be between lock!
 * Why?
 * Some public interface functions (ex. ylmp_is_safe()) uses lock.
 * And, in the listener, these functions may be called.
 * This is cause of deadlock!
 */
#define _walk_listeners(OP, PARAM)                      \
    do {                                                \
        _lsn_t* ___p;                                   \
        yllist_foreach_item(___p, &_lsnl, _lsn_t, lk) { \
            (*___p->ops->OP)PARAM;                      \
        }                                               \
    } while(0)


static pthread_mutex_t  _m;

static yllist_link_t    _lsnl;  /**< listener list */
static yllist_link_t    _cxtl;  /**< thread context list */



static inline _cxt_t*
_find_cxt(const yletcxt_t* cxt) {
    _cxt_t*  ct;
    yllist_foreach_item(ct, &_cxtl, _cxt_t, lk) {
        if(ct->cxt == cxt) {
            return ct;
        }
    }
    yllogW1("WARN! Try to set mark to unlisted context! : %p\n", cxt);
    return NULL;
}

static inline void
_mark_clear_all(int mark) {
    _cxt_t*  ct;
    yllist_foreach_item(ct, &_cxtl, _cxt_t, lk) {
        _mkclear(ct, mark);
    }
}

static inline int
_mark_is_all_marked(int mark) {
    _cxt_t*  ct;
    yllist_foreach_item(ct, &_cxtl, _cxt_t, lk) {
        if(!_mkisset(ct, mark)) { return 0; }
    }
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
 *    lock(_m) -> lock(<client's mutex>) / unlock(<client's mutex) -> unlock(_m)
 *
 * !! Case Study !!
 * In 'ylmt_add', switching order between '_mlock(&_m)' and '_walk_listeners(...)'
 *  may case deadlock due to inconsistent mutex order.
 *
 *===================================================================*/
void
ylmt_add(const yletcxt_t* cxt) {
    _cxt_t* ct = ylmalloc(sizeof(_cxt_t));
    ct->cxt = cxt;
    ct->mark = 0;

    _mlock(&_m);
    _walk_listeners(pre_add, (cxt));
    yllist_add_last(&_cxtl, &ct->lk);
    _walk_listeners(post_add, (cxt));
    _munlock(&_m);
}

void
ylmt_rm(const yletcxt_t* cxt) {
   _cxt_t* ct;
    /* add/rm should be done in the same thread! */
    ylassert(pthread_self() == cxt->id);
    
    _mlock(&_m);
    _walk_listeners(pre_rm, (cxt));
    yllist_foreach_item(ct, &_cxtl, _cxt_t, lk) {
        if(ct->cxt == cxt) {
            yllist_del(&ct->lk);
            ylfree(ct);
            _walk_listeners(post_rm, (cxt));
            goto done;
        }
    }

    if(&ct->lk == &_cxtl) {
        yllogW1("Try to remove unlisted thread context : %p\n", cxt);
    }

 done:
    _munlock(&_m);

    /*
     * Some thread are not reported that they are in safe state.
     * Instead of it, those may finish their execution.
     * In this case, checking safty at 'ylmt_notify_safe()'
     *  CANNOT detect that current is safe.
     * So, we need to check safty again at this point.
     *
     * See 'ylmt_notify_safe()' for more related comments.
     */
    _mlock(&_m);
    if(_mark_is_all_marked(_MKSAFE)) {
        _walk_listeners(all_safe, (&_m));
    }
    _munlock(&_m);

    return;
}


void
ylmt_register_listener(const ylmtlsn_t* lsnr) {
    _lsn_t* p = ylmalloc(sizeof(_lsn_t));
    ylassert(p);
    p->ops = lsnr;
    yllist_add_last(&_lsnl, &p->lk);
}

int
ylmt_is_safe() {
    int ret;
    _mlock(&_m);
    ret = _mark_is_all_marked(_MKSAFE);
    _munlock(&_m);
    return ret;
}

/*
 * This can be performance bottleneck!
 * Be careful!
 *
 * !!! Further Thought !!!
 *    We may use 'mark count' for _MKSAFE,
 *     to reduce one-list-search for '_mark_is_all_marked'
 *    We may also can use 'Trie' instead of 'List'
 *     to reduce time from O(n) to O(1) for '_find_cxt()'
 *    So, "Trie + mark_count" can make complexity of 'ylmt_notify_safe()'
 *     from O(n) to O(1)!.
 *
 *    If needed, consider it!
 *    But, most cases, Simple is Better!!
 *    Keep simple as long as possible!
 */
void
ylmt_notify_safe(const yletcxt_t* cxt) {
    _cxt_t*  ct;
    /*
     * SHOULD NOT use 'ylmt_is_safe()' here!
     * We SHOULD KEEP SAFE while 'now_safe' is called.
     * So, mutex SHOULD be kept locking!
     */
    _mlock(&_m);

    ct = _find_cxt(cxt);
    ylassert(ct);
    _mkset(ct, _MKSAFE);
    if(_mark_is_all_marked(_MKSAFE)) {
        _walk_listeners(all_safe, (&_m));
    } else {
        _walk_listeners(thd_safe, (cxt, &_m));
    }
    _mkclear(ct, _MKSAFE);

    _munlock(&_m);
}



ylerr_t
ylmt_init() {
    yllist_init_link(&_cxtl);
    yllist_init_link(&_lsnl);
    pthread_mutex_init(&_m, ylmutexattr());
    return YLOk;
}

void
ylmt_deinit() {
    _lsn_t *p, *tmp;
    ylassert(0 == yllist_size(&_cxtl));
    pthread_mutex_destroy(&_m);
    yllist_foreach_item_removal_safe(p, tmp, &_lsnl, _lsn_t, lk) {
        yllist_del(&p->lk);
        ylfree(p);
    }
}

