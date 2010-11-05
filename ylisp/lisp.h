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

#include <pthread.h>
#include <errno.h>
#include "config.h"
/*
 * For debugging
 */
#ifdef CONFIG_DBG_EVAL
#   define dbg_eval(x)     do{ x } while(0)
#else /* _YLDBG_EVAL */
#   define dbg_eval(x)
#endif /* _YLDBG_EVAL */

#ifdef CONFIG_DBG_MEM
#   define dbg_mem(x)      do{ x } while(0)
#else /* CONFIG_DBG_MEM */
#   define dbg_mem(x)
#endif /* CONFIG_DBG_MEM */

#ifdef CONFIG_DBG_GEN
#   define dbg_gen(x)      do{ x } while(0)
#else /* CONFIG_DBG_MEM */
#   define dbg_gen(x)
#endif /* CONFIG_DBG_MEM */


#ifdef CONFIG_DBG_MUTEX
#   define dbg_mutex(x)  do{ x } while(0)
#else /* CONFIG_DBG_INTEVAL */
#   define dbg_mutex(x)
#endif /* CONFIG_DBG_INTEVAL */

#include "ylisp.h"
#include "ylsfunc.h"
#include "yldynb.h"
#include "ylut.h"
#include "stack.h"
#include "yllist.h"
#include "mempool.h"
#include "mthread.h"
#include "yltrie.h"
#include "gsym.h"


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
struct _etcxt {
    pthread_t          id;
    ylstk_t*           evalstk;
    yldynb_t           dynb;
}; /* Evaluation Thread ConteXT */

extern ylerr_t ylnfunc_init();
extern ylerr_t ylsfunc_init();
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


#endif /* ___LISp_h___ */
