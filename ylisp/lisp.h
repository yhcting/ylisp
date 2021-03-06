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

/*
 * For debugging
 */
#ifdef CONFIG_DBG_EVAL
#       define dbg_eval(x)     do { x } while (0)
#else
#       define dbg_eval(x)
#endif

#ifdef CONFIG_DBG_MT
#       define dbg_mt(x)     do { x } while (0)
#else
#       define dbg_mt(x)
#endif

#ifdef CONFIG_DBG_MEM
#       define dbg_mem(x)      do { x } while (0)
#else
#       define dbg_mem(x)
#endif

#ifdef CONFIG_DBG_GEN
#       define dbg_gen(x)      do { x } while (0)
#else
#       define dbg_gen(x)
#endif

#ifdef CONFIG_DBG_MUTEX
#       define dbg_mutex(x)  do { x } while (0)
#else
#       define dbg_mutex(x)
#endif

/**********************************************
 *
 * Symbol type
 *
 **********************************************/

enum {
	YLASym_def      = 0,     /**< default symbol - ylasymi() */
	YLASym_mac      = 0x01,  /**< symbol represents macro - ylasymi() */
	YLASym_num      = 0x02,  /**< symbol represents number- ylasymd() */
};

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


/*
 * For modules to register init function.
 */
extern void
ylregister_initfn(ylerr_t (*fn)(void));
extern void
ylregister_exitfn(ylerr_t (*fn)(void));

#define __YLMODULE_CTORFN(name, definition)			\
	static void name(void) __attribute__ ((constructor));	\
	static void name(void) {				\
		definition					\
	}

#define YLMODULE_INITFN(mod, fn) \
	__YLMODULE_CTORFN(__##mod##___init_##fn##__, ylregister_initfn(&fn);)

#define YLMODULE_EXITFN(mod, fn) \
	__YLMODULE_CTORFN(__##mod##___exit_##fn##__, ylregister_exitfn(&fn);)



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
 *    To handle this exception case,
 *      thread should keep it's process resource list.
 *    And, if thread fails unexpectedly,
 *      this opened resource list source be closed!
 */
struct _etcxt {
	yllist_link_t          lk;       /**< link for linked list */
	pthread_t              base_id;  /**< base(owner) thread id */
	pthread_mutex_t        m;        /**< mutex for ethread internal use */
	unsigned int           sig;      /**< signal bits */
	unsigned int           state;    /**< thread state */
	ylstk_t*               thdstk;   /**< evaluation thread stack
					    [pthraed_t] */
	ylstk_t*               evalstk;  /**< evaluation stack [yle_t*]
					    - for debugging */
	yllist_link_t          pres;     /**< process resource list */
	slut_t*                slut;     /**< per-thread Symbol LookUp Table */
	yldynb_t               dynb;

	const unsigned char*   stream;   /**< target stream interpreted */
	unsigned int           streamsz; /**< stream size */
}; /* Evaluation Thread ConteXT */

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
yleval_id(void);

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
ylinterpret_internal(yletcxt_t* cxt,
		     const unsigned char* stream,
		     unsigned int streamsz);

extern ylerr_t
ylinterpret_async(pthread_t* thd,
		  const unsigned char* stream,
		  unsigned int streamsz);

extern ylerr_t
ylinit_thread_context(yletcxt_t* cxt);

extern void
ylexit_thread_context(yletcxt_t* cxt);

extern pthread_mutexattr_t* ylmutexattr(void);

static inline int
__mtrylock(pthread_mutex_t* m) {
	int r;
	r = pthread_mutex_trylock(m);
	if (!r)
		return 1;
	else if (EBUSY == r)
		return 0;
	else {
		yllogE("ERROR TRYLOCK Mutex [%s]\n", strerror(r));
		ylassert(0);
		return 0; /* to make compiler be happy */
	}
}


static inline void
__mlock(pthread_mutex_t* m) {
	int r;
	r = pthread_mutex_lock(m);
	if (r) {
		yllogE("ERROR LOCK Mutex [%s]\n", strerror(r));
		ylassert(0);
	}
}

static inline void
__munlock(pthread_mutex_t* m) {
	int r;
	r = pthread_mutex_unlock(m);
	if (r) {
		yllogE("ERROR UNLOCK Mutex [%s]\n", strerror(r));
		ylassert(0);
	}
}

#ifdef CONFIG_DBG_MUTEX

#define _mlock(m)                                                       \
	do {								\
		yllogD("+MLock: %p [%s][%d] ... ", m, __FILE__, __LINE__); \
		__mlock(m);						\
		yllogD("OK\n");					\
    } while (0)

#define _munlock(m)                                                     \
	do {								\
		yllogD("+MUnlock: %p [%s][%d]\n", m, __FILE__, __LINE__); \
		__munlock(m);						\
	} while (0)

#define _mtrylock(m)                                                    \
	do {								\
		int r;							\
		yllogD("+MTrylock: %p [%s][%d] ... ", m, __FILE__, __LINE__); \
		r = __mtrylock(m);					\
		yllogD("%s\n", r? "OK\n": "Fail\n");			\
	} while (0)

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
 * if @x is one of @u's, it changes the value of @u.
 * If not, it changes global lookup map.
 *
 * @x: atomic symbol
 * @y: any S-expression
 * @a: map yllist
 * @desc: descrption for this symbol.
 *        Can be NULL(means "Do not change description").
 * @return: new value
 */
extern yle_t*
ylset(yletcxt_t* cxt,
      yle_t* s,
      yle_t* val,
      yle_t* a,
      const char* desc,
      int ty);

/**
 * Set at per-thread symbol table.
 * See 'ylset' for comments.
 */
extern yle_t*
yltset(yletcxt_t* cxt,
       yle_t* s,
       yle_t* val,
       yle_t* a,
       const char* desc,
       int ty);

#endif /* ___LISp_h___ */
