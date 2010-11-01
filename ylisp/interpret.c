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

#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "lisp.h"
#include "ylisp.h"
#include "mempool.h"
#include "yllist.h"
#include "stack.h"

/* #define _TEST_STRESS_GC */

typedef struct {
    yllist_link_t   lk;
    pthread_t       t;
} _ethd_t; /* Evaluation THreaD */

typedef struct {
    yllist_link_t   lk;
    pid_t           id;
} _eproc_t; /* Evaluation PROCess */

/* =====================================
 *
 * Static variable...
 * These should be synchronised by inteval lock.
 *
 * =====================================*/
static yllist_link_t   _ethdl;  /* ethd List */

static yllist_link_t   _eprocl; /* eproc List */

/* evaluation stack for debugging */
static ylstk_t*        _evalstk = NULL;
/* =====================================
 *
 * For synchronization!
 *
 * =====================================*/
/*
 * to prevent parallel interrpreting
 * This lock is locally used (only in this file!!)
 */
static pthread_mutex_t _interp_mutex;

/*
 * Killing child process in force stop is executed in the MIDDLE of EVALUATION!
 * So, this has smaller scope than 'inteval lock'!
 * This mutex is for 'Force stop'!
 * (Till now for stopping child process!)
 */
static pthread_mutex_t _fs_mutex; /* Force Stop mutex */

/*
 * mutex to interrupt evaluation
 * unlocking this mutex means "this is appropriate point to interrupt evaluation!"
 */
static pthread_mutex_t _inteval_mutex;

/* mutex attribute */
static pthread_mutexattr_t _mutexattr;

static inline int
_init_mutexes () {
    /*
     * [SOFT-TODO]
     * Mutex Initialization rarely fails.
     * So, "Error Handling" is missed here!.
     * (Implemenet it if required!)
     */
    pthread_mutexattr_init(&_mutexattr);
    pthread_mutexattr_settype(&_mutexattr, PTHREAD_MUTEX_ERRORCHECK);

    pthread_mutex_init(&_interp_mutex, &_mutexattr);
    pthread_mutex_init(&_fs_mutex, &_mutexattr);
    pthread_mutex_init(&_inteval_mutex, &_mutexattr);
    return 0;
}

static inline void
_deinit_mutexes() {
    pthread_mutex_destroy(&_inteval_mutex);
    pthread_mutex_destroy(&_fs_mutex);
    pthread_mutex_destroy(&_interp_mutex);

    pthread_mutexattr_destroy(&_mutexattr);
}

#define _DECL_MUTEX_FUNC(m)                                             \
    static inline void                                                  \
    _##m##_lock() {                                                     \
        int r;                                                          \
        r = pthread_mutex_lock(&_##m##_mutex);                          \
        if(r) {                                                         \
            yllogE2("ERROR LOCK Mutex [%s]\n"                           \
                    "    => %s\n",#m, strerror(r));                     \
            ylassert(0);                                                \
        }                                                               \
    }                                                                   \
                                                                        \
    static inline void                                                  \
    _##m##_unlock() {                                                   \
        int r;                                                          \
        r = pthread_mutex_unlock(&_##m##_mutex);                        \
        if(r) {                                                         \
            yllogE2("ERROR UNLOCK Mutex [%s]\n"                         \
                    "    => %s\n",#m, strerror(r));                     \
            ylassert(0);                                                \
        }                                                               \
    }                                                                   \
                                                                        \
    static inline int                                                   \
    _##m##_trylock() {                                                  \
        int r;                                                          \
        r = pthread_mutex_trylock(&_##m##_mutex);                       \
        switch(r) {                                                     \
            case 0:       return TRUE;  /* successfully get */          \
            case EBUSY:   return FALSE; /* fail to get */               \
            default:                                                    \
                yllogE2("ERROR TRYLOCK Mutex [%s]\n"                    \
                        "    => %s\n",#m, strerror(r));                 \
                ylassert(0);                                            \
                return -1;                                              \
        }                                                               \
    }



_DECL_MUTEX_FUNC(interp)
_DECL_MUTEX_FUNC(fs)
_DECL_MUTEX_FUNC(inteval)

#undef _DECL_MUTEX_FUNC


/* =====================================
 *
 * GC Triggering Timer (GCTT)
 *     - Full scanning GC is triggered if there is no interpreting request during pre-defined time (Doing expensive operation at the idle moment).
 *
 * =====================================*/
#define _GCTT_DELAY     1 /* sec */
#define _GCTT_SIG       SIGRTMIN
#define _GCTT_CLOCKID   CLOCK_REALTIME

static timer_t         _gcttid;

static void
__gctt_settime(long sec) {
    struct itimerspec its;

    /* Start the timer */
#ifdef _TEST_STRESS_GC
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 50000000; /* 50 ms */
#else /* _TEST_STRESS_GC */
    its.it_value.tv_sec = sec;
    its.it_value.tv_nsec = 0;
#endif /* _TEST_STRESS_GC */

    /* This is one-time shot! */
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if(0 > timer_settime(_gcttid, 0, &its, NULL) ) { ylassert(0); }
}

static inline void
_gctt_set_timer() {
    __gctt_settime(_GCTT_DELAY);
}

static inline void
_gctt_unset_timer() {
    __gctt_settime(0);
}

static void*
_gctt_gc(void* arg) {
    _inteval_lock();
    ylmp_gc();
    _inteval_unlock();
    return NULL;
}

static inline void
_gctt_handler(int sig, siginfo_t* si, void* uc) {
    pthread_t    thd;
    if(pthread_create(&thd, NULL, &_gctt_gc, NULL)) {
        ylassert(0);
    }
}

static int
_gctt_create() {
    struct sigevent      sev;
    struct sigaction     sa;

    /* Establish handler for timer signal */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = _gctt_handler;
    sigemptyset(&sa.sa_mask);
    if ( -1 == sigaction(_GCTT_SIG, &sa, NULL) ) {
        yllogE0("Error gctt : sigaction\n");
        return -1;
    }

    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = _GCTT_SIG;
    sev.sigev_value.sival_ptr = &_gcttid;
    if ( -1 == timer_create(_GCTT_CLOCKID, &sev, &_gcttid) ) {
        yllogE0("Error gctt : timer_create\n");
        return -1;
    }

    return 0;
}

void
_gctt_delete() {
    timer_delete(_gcttid);
}

/* Unset locally used preprocess symbols */
#undef _GCTT_CLOCKID
#undef _GCTT_SIG
#undef _GCTT_DELAY



/* =====================================
 *
 * Evaluation Process/Thread handling
 *
 * =====================================*/
static inline void
_thdinfo_init() {
    yllist_init_link(&_ethdl);
}

static inline void
_thdinfo_add(pthread_t thd) {
    _ethd_t* t = ylmalloc(sizeof(*t));
    t->t = thd;
    yllist_add_last(&_ethdl, &t->lk);
}

static inline void
_thdinfo_del(pthread_t thd) {
    _ethd_t* t;
    yllist_foreach_item(t, &_ethdl, _ethd_t, lk) {
        if(t->t == thd) { break; }
    }
    if(&t->lk != &_ethdl) {
        /* we found given thread info */
        yllist_del(&t->lk);
        ylfree(t);
    } else {
        yllogW1("WARN: Try to del unlisted thread info [%p] !!\n", thd);
    }
}

static inline void
_thdinfo_clean() {
    _ethd_t *t, *n;
    yllist_foreach_item_removal_safe(t, n, &_ethdl, _ethd_t, lk) {
        pthread_cancel(t->t);
        ylfree(t);
    }
}

/*
 * Usually child process that ylisp spawns!
 */
static inline void
_procinfo_init() {
    yllist_init_link(&_eprocl);
}

static inline void
_procinfo_add(pid_t id) {
    _eproc_t* p = ylmalloc(sizeof(*p));
    p->id = id;
    _fs_lock();
    yllist_add_last(&_eprocl, &p->lk);
    _fs_unlock();
}

static inline void
_procinfo_del(pid_t id) {
    _eproc_t* p;
    _fs_lock();
    yllist_foreach_item(p, &_eprocl, _eproc_t, lk) {
        if(p->id == id) { break; }
    }
    if(&p->lk != &_eprocl) {
        /* we found given thread info */
        yllist_del(&p->lk);
        ylfree(p);
    } else {
        yllogW1("WARN: Try to del unlisted process info [%d] !!\n", id);
    }
    _fs_unlock();
}

static inline void
_procinfo_clean() {
    _eproc_t *p, *n;
    _fs_lock();
    yllist_foreach_item_removal_safe(p, n, &_eprocl, _eproc_t, lk) {
        kill(p->id, SIGKILL);
        yllist_del(&p->lk);
        ylfree(p);
    }
    _fs_unlock();
}


/* =====================================
 *
 * Evaluation informations
 *
 * =====================================*/
/*
 * Should be synchronised with 'inteval_lock'
 */
static inline void
_show_eval_stack() {
    while(ylstk_size(_evalstk)) {
        ylprint(("    %s\n", ylechain_print((yle_t*)ylstk_pop(_evalstk))));
    }
}

static inline void
_clean_interpret_step() {

    /*
     * cleaning thread info firstly is better!
     * Why? thread waits for child process.
     * Without thread, result of process cannot make any effect to ylisp!
     */
    _thdinfo_clean();
    _procinfo_clean();

    /*
     * GC should be here.
     * If interpreting is success, next interpreting may be requested
     *  in short time with high possibility. In this case, GC SHOULD NOT executed.
     *
     * Interpreting fails. So, let's clean garbage!
     */
    ylmp_clean_bb();
    ylmp_gc();

    ylstk_clean(_evalstk);
}
/* =====================================
 *
 * Public interfaces
 *
 * =====================================*/
void
ylinteval_lock() {
    _inteval_lock();
}

void
ylinteval_unlock() {
    _inteval_unlock();
}

int
ylprocinfo_add(long pid) {
    _procinfo_add(pid);
    return 0;
}

void
ylprocinfo_del(long pid) {
    _procinfo_del(pid);
}

void
ylevalinfo_push(const yle_t* e) {
    ylstk_push(_evalstk, (void*)e);
}

void
ylevalinfo_pop() {
    ylstk_pop(_evalstk);
}

ylerr_t
ylinterpret_internal(const unsigned char* stream, unsigned int streamsz) {
    /* Declar space to use */
    ylerr_t                  ret = YLErr_force_stopped;
    pthread_t                thd;
    struct __interpthd_arg   arg;
    int                      line = 1;

    if(!stream || 0==streamsz) { return YLOk; } /* nothing to do */

    arg.s = stream;
    arg.sz = streamsz;
    arg.line = &line;
    arg.ststk = ylstk_create(0, NULL);
    arg.pestk = ylstk_create(0, NULL);

    if( !(arg.ststk && arg.pestk) ) {
        ret = YLErr_out_of_memory;
        goto bail;
    }

    if(pthread_create(&thd, NULL, &ylinterp_automata, &arg)) {
        ylassert(0);
        goto bail;
    }

    _thdinfo_add(thd);

    if(pthread_join(thd, (void**)&ret)) {
        ylassert(0);
        pthread_cancel(thd);
        _thdinfo_del(thd);
        goto bail;
    }

    _thdinfo_del(thd);

    if(YLOk != ret) {
        ylprint(("Interpret FAILS! : ERROR Line : %d\n", line));
        goto bail;
    }

 bail:
    if(arg.ststk) { ylstk_destroy(arg.ststk); }
    if(arg.pestk) { ylstk_destroy(arg.pestk); }
    return ret;
}

ylerr_t
ylinterpret(const unsigned char* stream, unsigned int streamsz) {
#ifndef _TEST_STRESS_GC
    _gctt_unset_timer();
#endif /* _TEST_STRESS_GC */

    switch(_interp_trylock()) {
        case TRUE: {
            ylerr_t  ret;

            ret = ylinterpret_internal(stream, streamsz);
            if(YLOk == ret) {
                _gctt_set_timer();
            } else {
                /*
                 * Following operations are reading ylisp state.
                 * So, we have to use inteval mutex!
                 */
                _inteval_lock();

                /*
                 * evaluation stack should be shown before GC.
                 * (After GC, expression in the stack may be invalid one!)
                 */
                _show_eval_stack();

                /*
                 * Evaluation fails!
                 * We should clean up all evaluations of this step and child stuffs!!
                 */
                _clean_interpret_step();

                /* back to */
                _inteval_unlock();
            }

            _interp_unlock();
            return ret;
        } break;

        case FALSE: {
            yllogE0("Under interpreting...Cannot interpret in parallel!\n");
            return YLErr_under_interpreting;
        }

        default: {
            yllogE0("Error in mutex... May be unrecoverable!\n");
            return YLErr_internal;
        }
    }
}

void
ylforce_stop() {
    /*
     * We are ASSUMING that this function is not re-enterable!
     * Just like "Fail to Interpret!"
     *
     * By killing subprocess, we can expect that ylisp enters interruptable state.(see - inteval_lock())
     * Then, interrupt evaluation and stop it!
     * This is useful when interpreter is stuck by long-executing-subprocess.
     * But, it is useless when evaluation itself takes very-long-time.
     * So, one-evalation-step should not take too much time!!
     * (Keep it in mind when create CNFs!)
     */
    while(!_inteval_trylock()) {
        /* kill subprocess forcely */
        _fs_lock();
        { /* Just Scope */
            _eproc_t *p, *n;
            yllist_foreach_item_removal_safe(p, n, &_eprocl, _eproc_t, lk) {
                kill(p->id, SIGKILL);
            }
        }
        _fs_unlock();
        usleep(20000); /* 20ms */
    }

    /*
     * Now interpreter is interruptable!.
     * So, we can forcely cancel thread like 'ylinterpet_undefined'!
     */
    _thdinfo_clean();

    _inteval_unlock();
    ylprint(("Force Stop !!!\n"));
}

void
ylinterpret_undefined(int reason) {
    /*
     * This is called in the evaluation lock.
     * So, we need to unlock firstly!
     */
    _inteval_unlock();
    pthread_exit((void*)reason);
}

ylerr_t
ylinterp_init() {
    if( 0 > _gctt_create()
       || 0 > _init_mutexes() ) {
        return YLErr_internal;
    }

    _thdinfo_init();
    _procinfo_init();

    _evalstk = ylstk_create(0, NULL);
    return YLOk;
}

void
ylinterp_deinit() {
    _gctt_delete();
    _thdinfo_clean();
    _procinfo_clean();
    ylstk_destroy(_evalstk);
    _deinit_mutexes();
}
