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



/**
 * Main parser...
 * Simple FSA(Finite State Automata) is used to parse syntax.
 */
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "lisp.h"
#include "ylisp.h"
#include "mempool.h"
#include "stack.h"

/* single element string should be less than */
#define _MAX_SINGLE_ELEM_STR_LEN    4096

/* =====================================
 *
 * Static variable...
 *
 * =====================================*/
static ylstk_t*        _thdstk = NULL;
/*
 * child process id that ylisp waits for
 * '-1' means 'Error'(See, 'fork()')
 */
static pid_t           _cpid = -1;

/* evaluation stack for debugging */
static ylstk_t*        _evalstk = NULL;
/* =====================================
 *
 * For synchronization!
 *
 * =====================================*/
/* to prevent parallel interrpreting */
static pthread_mutex_t _interp_mutex;
/* mutex to handle child process */
static pthread_mutex_t _child_proc_mutex;
/* mutex to interrupt evaluation */
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
    pthread_mutexattr_settype(&_mutexattr, PTHREAD_MUTEX_NORMAL);

    pthread_mutex_init(&_interp_mutex, &_mutexattr);
    pthread_mutex_init(&_child_proc_mutex, &_mutexattr);
    pthread_mutex_init(&_inteval_mutex, &_mutexattr);
    return 0;
}

static inline void
_deinit_mutexes() {
    pthread_mutex_destroy(&_inteval_mutex);
    pthread_mutex_destroy(&_child_proc_mutex);
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
_DECL_MUTEX_FUNC(child_proc)
_DECL_MUTEX_FUNC(inteval)

#undef _DECL_MUTEX_FUNC


static inline void
_push_thread(pthread_t* thd) {
    _inteval_lock();
    ylstk_push(_thdstk, thd);
    _inteval_unlock();
}

static inline void
_pop_thread() {
    _inteval_lock();
    ylstk_pop(_thdstk);
    _inteval_unlock();
}


/* =====================================
 *
 * State Automata
 *
 * =====================================*/
/*
 * Common values of all state
 */
typedef struct {
    yle_t                sentinel;
    yle_t*               pe;               /* current expr (usually pair) */
    unsigned char       *b, *bend;         /* element buffer - start and (end+1) position.*/
    unsigned char*       pb;               /* current position in buffer */
    const unsigned char *s, *send;         /* stream / (end+1) of stream(pe) */
    int                  line;
    ylstk_t*             ststk;            /* state stack */
    ylstk_t*             pestk;            /* prev pair expression stack */
} _fsa_t; /* Finite State Automata - This is Automata context. */

typedef struct _fsas {
    const char*  name;  /* state name. usually used for debugging */
    void         (*enter)        (const struct _fsas*, _fsa_t*);
    void         (*come_back)    (const struct _fsas*, _fsa_t*);
    /*
     * @_fsas_t**:  next state. NULL menas 'exit current state'
     * @return: TRUE if character is handled. Otherwise FALSE.
     */
    int          (*next_char)    (const struct _fsas*, _fsa_t*, char, const struct _fsas**);
    void         (*exit)         (const struct _fsas*, _fsa_t*);
} _fsas_t; /* FSA State */


#define _DECL_FSAS_FUNC(nAME)                                           \
    static void _fsas_##nAME##_enter(const _fsas_t*, _fsa_t*);          \
    static void _fsas_##nAME##_come_back(const _fsas_t*, _fsa_t*);      \
    static int  _fsas_##nAME##_next_char(const _fsas_t*, _fsa_t*, char, const _fsas_t**); \
    static void _fsas_##nAME##_exit(const _fsas_t*, _fsa_t*);

_DECL_FSAS_FUNC(init)
_DECL_FSAS_FUNC(list)
_DECL_FSAS_FUNC(squote)
_DECL_FSAS_FUNC(symbol)
_DECL_FSAS_FUNC(dquote)
_DECL_FSAS_FUNC(comment)
_DECL_FSAS_FUNC(escape)

#undef _DECL_FSAS_FUNC


/* vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv Transition table vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
Notation :
    <> : chracter is not handled.
    X : error
    - : not changed
    E : exit state

===========================================================================================
state      "         other       '         \         (         )         ;         \s
-------------------------------------------------------------------------------------------
init       dquo      <symb>      squo      *esca     list      X         comm      -
list       dquo      <symb>      squo      *esca     list      E         comm      -
squote     dquo      <symb>      squo      *esca     list      <E>       <E>       <E>
symbol     <E>       -           <E>       esca      <E>       <E>       <E>       <E>
dquote     E         -           -         esca      -         -         -         -
comment    -         -           -         -         -         -         -         E(\n)
escape     E         E           X         E         X         X         X         X
===========================================================================================

Exceptional case
    *esca : enter symbol state and then enter to escape state. (2 steps)
           (escape state is only for symbol.
            And, exiting from escape state should return valid symbol or part of symbol)

^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/


static const  _fsas_t
    /*
     * Dummy state to represent "state is not changed.".
     */
    _fsas_nochg = { "nochg", NULL, NULL, NULL, NULL },

    /*
     * Dummy state to represent "exit from current state".
     */
    _fsas_exit = { "exit", NULL, NULL, NULL, NULL },

    /*
     * Initial state.
     */
    _fsas_init = {
        "init",
        &_fsas_init_enter,
        &_fsas_init_come_back,
        &_fsas_init_next_char,
        &_fsas_init_exit
    },

    /*
     * In symbol
     */
    _fsas_symbol = {
        "symbol",
        &_fsas_symbol_enter,
        &_fsas_symbol_come_back,
        &_fsas_symbol_next_char,
        &_fsas_symbol_exit
    },

    /*
     * In double quota
     */
    _fsas_dquote = {
        "dquote",
        &_fsas_dquote_enter,
        &_fsas_dquote_come_back,
        &_fsas_dquote_next_char,
        &_fsas_dquote_exit
    },

    /*
     *  In comment
     */
    _fsas_comment = {
        "comment",
        &_fsas_comment_enter,
        &_fsas_comment_come_back,
        &_fsas_comment_next_char,
        &_fsas_comment_exit
    },

    /*
     *  In escape
     */
    _fsas_escape = {
        "escape",
        &_fsas_escape_enter,
        &_fsas_escape_come_back,
        &_fsas_escape_next_char,
        &_fsas_escape_exit
    },

    /*
     *  In list
     */
    _fsas_list = {
        "list",
        &_fsas_list_enter,
        &_fsas_list_come_back,
        &_fsas_list_next_char,
        &_fsas_list_exit
    },

    /*
     *  In single quote
     */
    _fsas_squote = {
        "squote",
        &_fsas_squote_enter,
        &_fsas_squote_come_back,
        &_fsas_squote_next_char,
        &_fsas_squote_exit
    };



/* =====================================
 * Tool Functions
 * =====================================*/
static inline void
_fsa_add_symchar(_fsa_t* fsa, char c) {
    if(fsa->pb < (fsa->bend-1)) {
        *fsa->pb = c; fsa->pb++;
    } else {
        yllogE0("Syntax Error : too long symbol!!!!!");
        ylinterpret_undefined(YLErr_internal);
    }
}

static inline yle_t*
_create_atom_sym(unsigned char* sym, unsigned int len) {
    char* str = ylmalloc(sizeof(char)*(len+1));
    if(!str) {
        yllogE1("Out Of Memory : [%d]!\n", len);
        ylinterpret_undefined(YLErr_out_of_memory);
    }
    memcpy(str, sym, len);
    str[len] = 0; /* ylnull-terminator */
    return ylacreate_sym(str);
}

static inline void
_eval_exp(yle_t* e) {
    yle_t*  ev;

    dbg_gen(yllogD1(">>>>> Eval exp:\n"
                    "    %s\n", ylechain_print(e)););
    ylmp_push1(e);
    ev = yleval(e, ylnil());
    ylmp_pop1();
}

/* =====================================
 * State Functions
 * =====================================*/


static void
_fsas_init_enter(const _fsas_t* fsas, _fsa_t* fsa) {
    /*
     * Initialise sentinel.
     * At first, set invalid(NULL) car/cdr
     * If not, ylpassign try to unref car/cdr which is invalid address.
     */
    yleset_type(&fsa->sentinel, YLEPair); /* type of sentinel is 'pair' */
    fsa->sentinel.u.p.car = fsa->sentinel.u.p.cdr = NULL;

    fsa->pe = &fsa->sentinel;
}

static void
_fsas_init_come_back(const _fsas_t* fsas, _fsa_t* fsa) {
    /* check that there is expression to evaluate*/
    if(&fsa->sentinel != fsa->pe) {
        dbg_gen(yllogD1("\n\n\n------ Line : %d -------\n", fsa->line););
        _eval_exp(ylcadr(&fsa->sentinel));
    }
    /* unrefer to free dangling block */
    ylpassign(&fsa->sentinel, NULL, NULL);

    /* prepare for new interpretation */
    fsa->pe = &fsa->sentinel;
}

static int
_fsas_init_next_char(const _fsas_t* fsas, _fsa_t* fsa, char c, const _fsas_t** nexts) {
    int  bhandled = TRUE;
    switch(c) {
        case '"':  *nexts = &_fsas_dquote;                       break;
        case '\'': *nexts = &_fsas_squote;                       break;
        case '\\': *nexts = &_fsas_escape;                       break;
        case '(':  *nexts = &_fsas_list;                         break;
        case ')':
            yllogE0("Syntax Error : parenthesis mismatching\n");
            ylinterpret_undefined(YLErr_syntax_parenthesis);
        case ';':  *nexts = &_fsas_comment;                      break;
        case '\r':
        case '\n':
        case '\t':
        case ' ':  *nexts = &_fsas_nochg;                        break;
        default:   *nexts = &_fsas_symbol;    bhandled = FALSE;  break;
    }
    return bhandled;
}

static void
_fsas_init_exit(const _fsas_t* fsas, _fsa_t* fsa) {
    /*
     * we need to unref memory blocks that is indicated by sentinel
     * (Usually, both are nil.
     */
    ylpassign(&fsa->sentinel, NULL, NULL);
}

/* ------------------------------------------ */

static void
_fsas_list_enter(const _fsas_t* fsas, _fsa_t* fsa) {
    yle_t*  pe = NULL;
    /* add new pair node for list */
    yle_t*  pair = ylcons(ylnil(), ylnil());

    ylpsetcdr(fsa->pe, pair);
    /* create sentinel */
    pe = ylcons(ylnil(), ylnil());
    ylpsetcar(pair, pe);

    /* pair is new tail of this list */
    ylstk_push(fsa->pestk, pair);
    fsa->pe = pe;
}

static void
_fsas_list_come_back(const _fsas_t* fsas, _fsa_t* fsa) {
}

static int
_fsas_list_next_char(const _fsas_t* fsas, _fsa_t* fsa, char c, const _fsas_t** nexts) {
    int  bhandled = TRUE;
    switch(c) {
        case '"':  *nexts = &_fsas_dquote;                        break;
        case '\'': *nexts = &_fsas_squote;                        break;
        case '\\': *nexts = &_fsas_escape;                        break;
        case '(':  *nexts = &_fsas_list;                          break;
        case ')':  *nexts = &_fsas_exit;                          break;
        case ';':  *nexts = &_fsas_comment;                       break;
        case '\r':
        case '\n':
        case '\t':
        case ' ':  *nexts = &_fsas_nochg;                         break;
        default:   *nexts = &_fsas_symbol;    bhandled = FALSE;   break;
    }
    return bhandled;
}

static void
_fsas_list_exit(const _fsas_t* fsas, _fsa_t* fsa) {
    yle_t* pe = ylstk_pop(fsa->pestk);
    /* connect to real expression chain - exclude sentinel */
    ylpsetcar(pe, ylcdar(pe));
    fsa->pe = pe;
}

/* ------------------------------------------ */

static void
_fsas_squote_enter(const _fsas_t* fsas, _fsa_t* fsa) {
    /* add quote and connect with it*/
    yle_t*   pair = ylcons(ylnil(), ylnil());
    yle_t*   pe;

    ylpsetcdr(fsa->pe, pair);
    pe = ylcons(ylq(), ylnil());
    ylpsetcar(pair, pe);
    ylstk_push(fsa->pestk, pair);
    fsa->pe = pe;
}

static void
_fsas_squote_come_back(const _fsas_t* fsas, _fsa_t* fsa) {
}

static int
_fsas_squote_next_char(const _fsas_t* fsas, _fsa_t* fsa, char c, const _fsas_t** nexts) {
    int  bhandled = TRUE;
    switch(c) {
        case '"':  *nexts = &_fsas_dquote;                        break;
        case '\'': *nexts = &_fsas_squote;                        break;
        case '\\': *nexts = &_fsas_escape;                        break;
        case '(':  *nexts = &_fsas_list;                          break;

        case ')':
        case ';':
        case '\r':
        case '\n':
        case '\t':
        case ' ':  *nexts = &_fsas_exit;     bhandled = FALSE;    break;
        default:   *nexts = &_fsas_symbol;   bhandled = FALSE;    break;
    }
    return bhandled;
}

static void
_fsas_squote_exit(const _fsas_t* fsas, _fsa_t* fsa) {
    /* in single quote case, sentinel isn't used. */
    fsa->pe = ylstk_pop(fsa->pestk);
}

/* ------------------------------------------ */

static void
_fsas_symbol_enter(const _fsas_t* fsas, _fsa_t* fsa) {
    fsa->pb = fsa->b; /* initialize buffer pointer */
}

static void
_fsas_symbol_come_back(const _fsas_t* fsas, _fsa_t* fsa) {
}

static int
_fsas_symbol_next_char(const _fsas_t* fsas, _fsa_t* fsa, char c, const _fsas_t** nexts) {
    int  bhandled = TRUE;
    switch(c) {
        case '"':  *nexts = &_fsas_exit;    bhandled = FALSE;  break;
        case '\'': *nexts = &_fsas_exit;    bhandled = FALSE;  break;
        case '\\': *nexts = &_fsas_escape;                     break;
        case '(':  *nexts = &_fsas_exit;    bhandled = FALSE;  break;
        case ')':  *nexts = &_fsas_exit;    bhandled = FALSE;  break;
        case ';':  *nexts = &_fsas_exit;    bhandled = FALSE;  break;
        case '\r':
        case '\n':
        case '\t':
        case ' ':  *nexts = &_fsas_exit;    bhandled = FALSE;  break;
        default:   *nexts = &_fsas_nochg;                      break;
    }
    if(&_fsas_nochg == *nexts) { _fsa_add_symchar(fsa, c); }
    return bhandled;
}

static void
_fsas_symbol_exit(const _fsas_t* fsas, _fsa_t* fsa) {
    yle_t* pair = ylcons(ylnil(), ylnil());
    yle_t* se = _create_atom_sym(fsa->b, (unsigned int)(fsa->pb - fsa->b));
    ylpsetcar(pair, se);
    ylpsetcdr(fsa->pe, pair);
    /* update previous element pointer */
    fsa->pe = pair;
    fsa->pb = fsa->b;
}

/* ------------------------------------------ */

static void
_fsas_dquote_enter(const _fsas_t* fsas, _fsa_t* fsa) {
    _fsas_symbol_enter(fsas, fsa);
}

static void
_fsas_dquote_come_back(const _fsas_t* fsas, _fsa_t* fsa) {
}

static int
_fsas_dquote_next_char(const _fsas_t* fsas, _fsa_t* fsa, char c, const _fsas_t** nexts) {
    int  bhandled = TRUE;
    switch(c) {
        case '"':  *nexts = &_fsas_exit;         break;
        case '\\': *nexts = &_fsas_escape;       break;
        default:   *nexts = &_fsas_nochg;        break;
    }
    if(&_fsas_nochg == *nexts) { _fsa_add_symchar(fsa, c); }
    return bhandled;
}

static void
_fsas_dquote_exit(const _fsas_t* fsas, _fsa_t* fsa) {
    _fsas_symbol_exit(fsas, fsa);
}

/* ------------------------------------------ */

static void
_fsas_comment_enter(const _fsas_t* fsas, _fsa_t* fsa) {
}

static void
_fsas_comment_come_back(const _fsas_t* fsas, _fsa_t* fsa) {
}

static int
_fsas_comment_next_char(const _fsas_t* fsas, _fsa_t* fsa, char c, const _fsas_t** nexts) {
    int  bhandled = TRUE;
    switch(c) {
        case '\n': *nexts = &_fsas_exit;         break;
        default:   *nexts = &_fsas_nochg;        break;
    }
    return bhandled;
}

static void
_fsas_comment_exit(const _fsas_t* fsas, _fsa_t* fsa) {
}

/* ------------------------------------------ */

static void
_fsas_escape_enter(const _fsas_t* fsas, _fsa_t* fsa) {
}

static void
_fsas_escape_come_back(const _fsas_t* fsas, _fsa_t* fsa) {
}

static int
_fsas_escape_next_char(const _fsas_t* fsas, _fsa_t* fsa, char c, const _fsas_t** nexts) {
    int      bhandled = TRUE;
    char     ch = c;

    *nexts = &_fsas_exit;
    switch(c) {
        case '"':
        case '\\': break;

        /* "\n" is supported to represent line-feed */
        case 'n':  ch = '\n';        break;
        default:
            yllogE0("Syntax Error : Unsupported character for escape!!\n");
            ylinterpret_undefined(YLErr_syntax_escape);
    }
    _fsa_add_symchar(fsa, ch);
    return bhandled;
}

static void
_fsas_escape_exit(const _fsas_t* fsas, _fsa_t* fsa) {
}

/* ------------------------------------------ */

static void*
_interp_automata(void* arg) {
    void*                 ret = (void*)YLOk;
    const unsigned char*  p;
    _fsa_t*               fsa = (_fsa_t*)arg;
    const _fsas_t        *st, *nst; /* current st, next st */

    /*
     * we need to lock here... we are about to start evaluation!
     */
    _inteval_lock();

    p = fsa->s;

    st = &_fsas_init;
    ylstk_push(fsa->ststk, (void*)st);
    (*_fsas_init.enter)(&_fsas_init, fsa);

    while(*p) {
        /* yllogV(("+++ [%s] : '%c' => ", st->name, ('\n' == *p)? '@': *p)); */
        if((*st->next_char)(st, fsa, *p, &nst)) {
            if('\n' == *p) { fsa->line++; }
            p++;
        }
        /* yllogV1("[%s]", nst->name); */

        if(&_fsas_nochg == nst) {
            ; /* nothing to do */
        } else if(&_fsas_exit == nst) {
            (*st->exit)(st, fsa);
            /* pop current state from stack */
            ylstk_pop(fsa->ststk);
            /* move to previous state */
            st = ylstk_peek(fsa->ststk);
            (*st->come_back)(st, fsa);
            /* yllogV1(" => [%s]", st->name); */
        } else if(&_fsas_escape == nst) {
            /* special case */
            if( !(&_fsas_dquote == ylstk_peek(fsa->ststk)
                  || &_fsas_symbol == ylstk_peek(fsa->ststk)) ) {
                /*
                 * enter symbol state firstly.
                 * - 'dquote' state is PRACTICALLY same with 'symbol' state.
                 *   So, we don't need to handle specially not only 'symbol' but also 'dquote' state.
                 */
                ylstk_push(fsa->ststk, (void*)&_fsas_symbol);
                (_fsas_symbol.enter)(&_fsas_symbol, fsa);
            }
            ylstk_push(fsa->ststk, (void*)nst);
            (*nst->enter)(nst, fsa);
             st = nst;
        } else {
            ylstk_push(fsa->ststk, (void*)nst);
            (*nst->enter)(nst, fsa);
             st = nst;
        }
        /* yllogV0("\n"); */

        if(fsa->send == p) {
            /* for easy parsing, automata always adds '\n' at the end of stream!!! */
            p = (unsigned char*)"\n";
        }
    }

    st = ylstk_pop(fsa->ststk);
    if(&_fsas_init == st) {
        (*_fsas_init.exit)(&_fsas_init, fsa);
        if(0 == ylstk_size(fsa->pestk)) {
            goto done;
        } else {
            yllogE0("Syntax Error!!!!!!\n");
            ret = (void*)YLErr_syntax_unknown;
            goto done;
        }
    } else {
        yllogE0("Syntax Error!!!!!!\n");
        ret = (void*)YLErr_syntax_unknown;
        goto done;
    }

 done:
    _inteval_unlock();
    return ret;
}

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
    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;

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
 * Child process handling
 *
 * =====================================*/
static void
_child_proc_kill() {
    _child_proc_lock();
    if(-1 != _cpid) {
        kill(_cpid, SIGKILL);
        _cpid = -1;
    }
    _child_proc_unlock();
}


/* =====================================
 *
 * Evaluation stack
 *
 * =====================================*/
static inline void
_show_eval_stack() {
    while(ylstk_size(_evalstk)) {
        ylprint(("    %s\n", ylechain_print((yle_t*)ylstk_pop(_evalstk))));
    }
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
ylchild_proc_set(long pid) {
    _child_proc_lock();
    if(-1 != _cpid) {
        yllogE0("Cannot wait more than one process at one time!!\n");
        ylassert(FALSE);
        _child_proc_unlock();
        return -1;
    }
    _cpid = pid;
    _child_proc_unlock();
    return 0;
}

void
ylchild_proc_unset() {
    _child_proc_lock();
    _cpid = -1;
    _child_proc_unlock();
}

void
ylpush_eval_info(const yle_t* e) {
    ylstk_push(_evalstk, (void*)e);
}

void
ylpop_eval_info() {
    ylstk_pop(_evalstk);
}

ylerr_t
ylinterpret_internal(const unsigned char* stream, unsigned int streamsz) {
    /* Declar space to use */
    static unsigned char elembuf[_MAX_SINGLE_ELEM_STR_LEN];

    ylerr_t              ret = YLErr_force_stopped;
    _fsa_t               fsa;
    pthread_t            thd;

    if(!stream || 0==streamsz) { return YLOk; } /* nothing to do */

    /*
     * NOTE! : We SHOULD NOT initialize sentinel here!
     */
    fsa.pb = fsa.b = elembuf;
    fsa.bend = fsa.b + _MAX_SINGLE_ELEM_STR_LEN;
    fsa.s = stream;
    fsa.send = fsa.s + streamsz;
    fsa.line = 1;
    fsa.ststk = ylstk_create(0, NULL);
    fsa.pestk = ylstk_create(0, NULL);

    /*
     * Force stop uses inteval_lock, and it access thread stack.
     * So, thread creation should be synchronized with force stop!
     */

    if( !(fsa.ststk && fsa.pestk) ) {
        ret = YLErr_out_of_memory;
        goto bail;
    }

    if(pthread_create(&thd, NULL, &_interp_automata, &fsa)) {
        ylassert(0);
        goto bail;
    }

    _push_thread(&thd);

    if(pthread_join(thd, (void**)&ret)) {
        ylassert(0);
        pthread_cancel(thd);
        _pop_thread();
        goto bail;
    }

    if(YLOk != ret) {
        ylprint(("Interpret FAILS! : ERROR Line : %d\n", fsa.line));
        goto bail;
    }

    _pop_thread();

    ylstk_destroy(fsa.pestk);
    ylstk_destroy(fsa.ststk);

    return YLOk;

 bail:
    if(fsa.ststk) { ylstk_destroy(fsa.ststk); }
    if(fsa.pestk) { ylstk_destroy(fsa.pestk); }
    return ret;
}

ylerr_t
ylinterpret(const unsigned char* stream, unsigned int streamsz) {
    _gctt_unset_timer();

    switch(_interp_trylock()) {
        case TRUE: {
            ylerr_t  ret;

            ylstk_clean(_thdstk);
            ylstk_clean(_evalstk);

            ret = ylinterpret_internal(stream, streamsz);
            if(YLOk == ret) { _gctt_set_timer(); }
            else {
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
                 * GC should be here.
                 * If interpreting is success, next interpreting may be requested
                 *  in short time with high possibility. In this case, GC SHOULD NOT executed.
                 */
                ylmp_clean_stack();
                /*
                 * Interpreting fails. So, let's clean garbage!
                 */
                ylmp_gc();

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
     */
    while(!_inteval_trylock()) {
        _child_proc_kill();
        usleep(20000); /* 20ms */
    }

    while(ylstk_size(_thdstk) > 0) {
        pthread_cancel(*((pthread_t*)ylstk_pop(_thdstk)));
    }
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
    _thdstk = ylstk_create(0, NULL);
    _evalstk = ylstk_create(0, NULL);
    return YLOk;
}

void
ylinterp_deinit() {
    _gctt_delete();
    _deinit_mutexes();
    ylstk_destroy(_thdstk);
    ylstk_destroy(_evalstk);
}
