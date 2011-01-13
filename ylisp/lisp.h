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



#ifndef ___LISp_h___
#define ___LISp_h___

#include <limits.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "config.h"

/*
 * Check build environment
 */
#if USHRT_MAX != 65535 || UINT_MAX != 4294967295
#   error Unsupported platform.
#endif
/*
 * For debugging
 */
#ifdef CONFIG_DBG_EVAL
#   define dbg_eval(x)     do{ x } while(0)
#else
#   define dbg_eval(x)
#endif

#ifdef CONFIG_DBG_MT
#   define dbg_mt(x)     do{ x } while(0)
#else
#   define dbg_mt(x)
#endif

#ifdef CONFIG_DBG_MEM
#   define dbg_mem(x)      do{ x } while(0)
#else
#   define dbg_mem(x)
#endif

#ifdef CONFIG_DBG_GEN
#   define dbg_gen(x)      do{ x } while(0)
#else
#   define dbg_gen(x)
#endif

#ifdef CONFIG_DBG_MUTEX
#   define dbg_mutex(x)  do{ x } while(0)
#else
#   define dbg_mutex(x)
#endif

#include "ylisp.h"
#include "ylsfunc.h"
#include "yldynb.h"
#include "ylut.h"
#include "stack.h"
#include "yllist.h"
#include "mempool.h"
#include "mthread.h"
#include "yltrie.h"
#include "symlookup.h"
#include "gsym.h"

/**********************************************
 *
 * Symbol type
 *
 **********************************************/
/*
 * 0 is default symbol attribute
 * ST : Symbol Type
 */
enum {
    STymac        = 0x01, /* macro symbol */
    STyper_thread = 0x02, /* perthread symbol - 0 for global */
};

/* -- symbol -- */
#define styis(ty, v)          (!!((ty) & (v)))
#define styset(ty, v)         ((ty) |= (v))
#define styclear(ty, v)       ((ty) &= ~(v))

/**********************************************
 *
 * Macro Overriding!
 * (For Internal Use!)
 *
 **********************************************/


/**********************************************
 *
 * Evaluation Thread Context
 *
 **********************************************/

/*
 * @pres
 *    Thread may be killed during safe state.
 *    But, in this safe state, thread may open process resources.
 *    If thread is killed during this, opened process resources are leaked!
 *    To handle this exception case, thread should keep it's process resource list.
 *    And, if thread fails unexpectedly, this opened resource list source be closed!
 */
struct _sEtcxt {
    yllist_link_t          lk;       /**< link for linked list */
    pthread_t              base_id;  /**< base(owner) thread id */
    pthread_mutex_t        m;        /**< mutex for ethread internal use */
    unsigned int           sig;      /**< signal bits */
    unsigned int           state;    /**< thread state */
    ylstk_t*               thdstk;   /**< evaluation thread stack [pthraed_t] */
    ylstk_t*               evalstk;  /**< evaluation stack [yle_t*] - for debugging */
    yllist_link_t          pres;     /**< process resource list */
    slut_t*                slut;     /**< per-thread Symbol LookUp Table */
    yldynb_t               dynb;

    const unsigned char*   stream;   /**< target stream interpreted */
    unsigned int           streamsz; /**< stream size */
}; /* Evaluation Thread ConteXT */

extern ylerr_t ylnfunc_init();
extern ylerr_t ylsfunc_init();
extern void    ylsfunc_deinit();
extern ylerr_t ylinterp_init();
extern void    ylinterp_deinit();

extern void
yleclean(yle_t* e);

/*
 * GC Protection required to caller
 */
extern yle_t*
ylapply(yletcxt_t* cxt, yle_t* f, yle_t* args, yle_t* a);


#ifdef CONFIG_DBG_EVAL
/*
 * get current evaluation id.
 */
extern unsigned int
yleval_id();

#endif /* CONFIG_DBG_EVAL */

/*
 * Internal use only. (between interpret.c -> syntax.c)
 * (This struct is totally dependent on internal code!)
 */
struct __interpthd_arg {
    yletcxt_t*           cxt;
    const unsigned char* s;
    unsigned int         sz;
    int                 *line;
    ylstk_t             *ststk, *pestk;
    unsigned char*       b; /* buffer for parsing syntax */
    unsigned int         bsz; /* buffer size */
};


/*
 * Thread to interpret syntax automata!
 */
extern void*
ylinterp_automata(void* arg);

/*
 * *** NOTE ***
 * This function requires evaluation MUTEX HANDLING!!!
 * KEEP YOUR ATTENTION TO MUTEX when use this function!
 *
 * only for internel use - for 'interpret-file' command!
 * (To know whether interpreting started by user request or by batch script)
 */
extern ylerr_t
ylinterpret_internal(yletcxt_t* cxt, const unsigned char* stream, unsigned int streamsz);

extern ylerr_t
ylinterpret_async(pthread_t* thd, const unsigned char* stream, unsigned int streamsz);

extern ylerr_t
ylinit_thread_context(yletcxt_t* cxt);

extern void
yldeinit_thread_context(yletcxt_t* cxt);

extern pthread_mutexattr_t* ylmutexattr();

static inline int
__mtrylock(pthread_mutex_t* m) {
    int r;
    r = pthread_mutex_trylock(m);
    if(!r) { return 1; }
    else if(EBUSY == r) { return 0; }
    else {
        yllogE1("ERROR TRYLOCK Mutex [%s]\n", strerror(r));
        ylassert(0);
        return 0; /* to make compiler be happy */
    }
}


static inline void
__mlock(pthread_mutex_t* m) {
    int r;
    r = pthread_mutex_lock(m);
    if(r) {
        yllogE1("ERROR LOCK Mutex [%s]\n", strerror(r));
        ylassert(0);
    }
}

static inline void
__munlock(pthread_mutex_t* m) {
   int r;
    r = pthread_mutex_unlock(m);
    if(r) {
        yllogE1("ERROR UNLOCK Mutex [%s]\n", strerror(r));
        ylassert(0);
    }
}

#ifdef CONFIG_DBG_MUTEX

#define _mlock(m)                                                       \
    do {                                                                \
        yllogI3("+MLock: %p [%s][%d] ... ", m, __FILE__, __LINE__);     \
        __mlock(m);                                                     \
        yllogI0("OK\n");                                                \
    } while(0)

#define _munlock(m)                                                     \
    do {                                                                \
        yllogI3("+MUnlock: %p [%s][%d]\n", m, __FILE__, __LINE__);      \
        __munlock(m);                                                   \
    } while(0)

#define _mtrylock(m)                                                    \
    do {                                                                \
        int r;                                                          \
        yllogI3("+MTrylock: %p [%s][%d] ... ", m, __FILE__, __LINE__);  \
        r = __mtrylock(m);                                              \
        yllogI1("%s\n", r? "OK\n": "Fail\n");                           \
    } while(0)

#else /* CONFIG_DBG_MUTEX */

#define _mlock(m)    __mlock(m)
#define _munlock(m)  __munlock(m)
#define _mtrylock(m) __mtrylock(m)

#endif /* CONFIG_DBG_MUTEX */

/*
 * to avoid symbol name (function name) conflicts with plug-ins
 * ylisp uses special naming rules!
 */
#undef  YLNFN
#define YLNFN(n)    __Yl__LNF__##n##__





/**********************************************
 *
 * Internal s/n-functions
 *
 **********************************************/
/**
 * @a is a list of the form ((u1 v1) ... (uN vN))
 * < additional constraints : @x is atomic >
 * if @x is one of @u's, it changes the value of @u. If not, it changes global lookup map.
 *
 * @x: atomic symbol
 * @y: any S-expression
 * @a: map yllist
 * @desc: descrption for this symbol. Can be NULL(means "Do not change description").
 * @return: new value
 */
extern yle_t*
ylset (yletcxt_t* cxt, yle_t* s, yle_t* val, yle_t* a, const char* desc, int ty);

#endif /* ___LISp_h___ */
