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
#include "lisp.h"
#include "ylisp.h"
#include "mempool.h"
#include "stack.h"

/* single element string should be less than */
#define _MAX_SINGLE_ELEM_STR_LEN    4096
#define _MAX_SYNTAX_DEPTH           64

/*
 * Common values of all state
 */
typedef struct {
    yle_t            sentinel;
    yle_t*           pe;               /* current expr (usually pair) */
    char            *b, *bend;         /* element buffer - start and (end+1) position.*/
    char*            pb;               /* current position in buffer */
    const char      *s, *send;         /* stream / (end+1) of stream(pe) */
    int              line;
    ylstk_t*         ststk;            /* state stack */
    ylstk_t*         pestk;            /* prev pair expression stack */
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

/* =====================================
 *
 * State Automata
 *
 * =====================================*/

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
init       dquo      <symb>      squo      esca      list      X         comm      -
list       dquo      <symb>      squo      esca      list      E         comm      -      
squote     dquo      <symb>      squo      esca      list      <E>       <E>       <E>
symbol     <E>       -           <E>       esca      <E>       <E>       <E>       <E>
dquote     E         -           -         esca      -         -         -         -
comment    -         -           -         -         -         -         -         E(\n)
escape     E         E           X         E         X         X         X         X
===========================================================================================
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
_create_pair() {
    yle_t* e = ylmp_get_block();
    ylpassign(e, ylnil(), ylnil());
    return e;
}

static inline yle_t*
_create_atom_sym(char* sym, unsigned int len) {
    yle_t* e = ylmp_get_block();
    char* str = ylmalloc(sizeof(char)*(len+1));
    if(!str) {
        yllogE1("Out Of Memory : [%d]!\n", len);
        ylinterpret_undefined(YLErr_out_of_memory);
    }
    memcpy(str, sym, len);
    str[len] = 0; /* ylnull-terminator */
    ylaassign_sym(e, str);
    return e;

}

static inline void
_eval_exp(yle_t* e) {
    /* "2 == ylmp_stack_size()" means "this evaluation is topmost one" */
#define __TOPMOST_EVAL_MPSTACK_SIZE 2
    int     ret;
    yle_t*  ev;

    dbg_gen(yllogD1(">>>>> Eval exp:\n"
                    "    %s\n", yleprint(e)););
    ylmp_push();
    ev = yleval(e, ylnil());
    if( __TOPMOST_EVAL_MPSTACK_SIZE == ylmp_stack_size() ) {
        ylprint(("\n%s\n", yleprint(ev)));
    }
    ylmp_pop();
#undef __TOPMOST_EVAL_MPSTACK_SIZE
}

/* =====================================
 * State Functions 
 * =====================================*/


static void
_fsas_init_enter(const _fsas_t* fsas, _fsa_t* fsa) {
    memset(&fsa->sentinel, 0, sizeof(fsa->sentinel));
    ylpassign(&fsa->sentinel, ylnil(), ylnil());
    fsa->pe = &fsa->sentinel;
    ylmp_push();
    dbg_mem(yllogD0("\n+++ START - Interpret +++\n"); 
            ylmp_log_stat(YLLogD););
}

static void
_fsas_init_come_back(const _fsas_t* fsas, _fsa_t* fsa) {
    if(&fsa->sentinel != fsa->pe) {
        dbg_gen(yllogD1("\n\n\n------ Line : %d -------\n", fsa->line););
        _eval_exp(ylcadr(&fsa->sentinel));
        /* unrefer to free dangling block */
        ylpsetcdr(&fsa->sentinel, ylnil());
        dbg_mem(yllogD0("\n+++ END - Interpret +++\n");
                ylmp_log_stat(YLLogD););

    }
    ylmp_pop();
    _fsas_init_enter(fsas, fsa);
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
    dbg_mem(yllogD0("\n+++ END - Interpret +++\n");
            ylmp_log_stat(YLLogD););
    ylmp_pop();

    /* 
     * we need to unref memory blocks that is indicated by sentinel
     * (Usually, both are nil.
     */
    yleunref(ylcar(&fsa->sentinel));
    yleunref(ylcdr(&fsa->sentinel));
}

/* ------------------------------------------ */

static void
_fsas_list_enter(const _fsas_t* fsas, _fsa_t* fsa) {
    yle_t*  pe = NULL;
    /* add new pair node for list */
    yle_t*  pair = _create_pair();

    ylpsetcdr(fsa->pe, pair);
    /* create sentinel */
    pe = _create_pair(); 
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
    yle_t*   pair = _create_pair();
    yle_t*   pe  = ylmp_get_block();

    ylpsetcdr(fsa->pe, pair);
    ylpassign(pe, ylq(), ylnil());
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
    yle_t* pair = _create_pair();
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
    const char*       p;
    _fsa_t*           fsa = (_fsa_t*)arg;
    const _fsas_t    *st, *nst; /* current st, next st */
    int               bdone = FALSE;
    
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
        } else {
            ylstk_push(fsa->ststk, (void*)nst);
            (*nst->enter)(nst, fsa);
             st = nst;
        }
        /* yllogV0("\n"); */

        if(fsa->send == p) {
            /* for easy parsing, automata always adds '\n' at the end of stream!!! */
            p = "\n";
        }
    }

    st = ylstk_pop(fsa->ststk);
    if(&_fsas_init == st) {
        (*_fsas_init.exit)(&_fsas_init, fsa);
        if(0 == ylstk_size(fsa->pestk)) { 
            return (void*)YLOk; 
        } else {
            yllogE0("Syntax Error!!!!!!\n");
            return (void*)YLErr_syntax_unknown; 
        }
    } else { 
        yllogE0("Syntax Error!!!!!!\n");
        return (void*)YLErr_syntax_unknown; 
    }
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
static pthread_mutex_t _gcttmutex = PTHREAD_MUTEX_INITIALIZER;

static inline void
_gctt_lock() {
    if(0 > pthread_mutex_lock(&_gcttmutex)) { ylassert(0); }
}

static inline void
_gctt_unlock() {
    if(0 > pthread_mutex_unlock(&_gcttmutex)) { ylassert(0); }
}

static void
__gctt_settime(long sec) {
    struct itimerspec its;

    /* Start the timer */
    its.it_value.tv_sec = sec;
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

static inline void
_gctt_handler(int sig, siginfo_t* si, void* uc) {
    _gctt_lock();
    ylmp_scan_gc(YLMP_GCSCAN_FULL);
    _gctt_unlock();
}

int
_gctt_create() {
    struct sigevent      sev;
    struct itimerspec    its;
    struct sigaction     sa;

    /* initialize mutex */
    if( 0 > pthread_mutex_init(&_gcttmutex, NULL) ) {
        yllogE0("Error gctt : pthread_mutex_init\n");
        return -1;
    }

    /* Establish handler for timer signal */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = _gctt_handler;
    sigemptyset(&sa.sa_mask);
    if ( -1 == sigaction(_GCTT_SIG, &sa, NULL) ) {
        yllogE0("Error gctt : sigaction\n");
        pthread_mutex_destroy(&_gcttmutex);
        return -1;
    }

    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = _GCTT_SIG;
    sev.sigev_value.sival_ptr = &_gcttid;
    if ( -1 == timer_create(_GCTT_CLOCKID, &sev, &_gcttid) ) {
        yllogE0("Error gctt : timer_create\n");
        pthread_mutex_destroy(&_gcttmutex);
        return -1;
    }

    return 0;
}
/* Unset locally used preprocess symbols */
#undef _GCTT_CLOCKID
#undef _GCTT_SIG
#undef _GCTT_DELAY

/* =====================================
 *
 * Public interfaces
 *
 * =====================================*/

ylerr_t
ylinterpret_internal(const char* stream, unsigned int streamsz) {
    /* Declar space to use */
    static char          elembuf[_MAX_SINGLE_ELEM_STR_LEN];

    ylerr_t              ret = YLErr_internal;
    _fsa_t               fsa;
    int                  rc;
    pthread_t            thd;

    if(!stream || 0==streamsz) { return YLOk; } /* nothing to do */

    memset(&fsa.sentinel, 0, sizeof(fsa.sentinel));

    /* 
     * NOTE! : We SHOULD NOT initialize sentinel here!
     */
    fsa.pb = fsa.b = elembuf; 
    fsa.bend = fsa.b + _MAX_SINGLE_ELEM_STR_LEN;
    fsa.s = stream;
    fsa.send = fsa.s + streamsz;
    fsa.line = 1;
    fsa.ststk = ylstk_create(_MAX_SYNTAX_DEPTH, NULL);
    fsa.pestk = ylstk_create(_MAX_SYNTAX_DEPTH, NULL);

    if( !(fsa.ststk && fsa.pestk) ) {
        ret = YLErr_out_of_memory;
        goto bail;
    }
    
    if(rc = pthread_create(&thd, NULL, &_interp_automata, &fsa)) {
        goto bail;
    }

    if(rc = pthread_join(thd, (void**)&ret)) {
        goto bail;
    }

    if(YLOk != ret) {
        ylprint(("Interpret FAILS! : ERROR Line : %d\n", fsa.line));
        goto bail;
    }

    ylstk_destroy(fsa.pestk);
    ylstk_destroy(fsa.ststk);

    dbg_mem(yllogI1("current MP after interpret : %d\n", ylmp_usage()););
    ylmp_log_stat(YLLogI);
    return YLOk;

 bail:
    if(fsa.ststk) { ylstk_destroy(fsa.ststk); }
    if(fsa.pestk) { ylstk_destroy(fsa.pestk); }
    return ret;
}


ylerr_t
ylinterpret(const char* stream, unsigned int streamsz) {
    ylerr_t  ret;

    _gctt_lock();
    _gctt_unset_timer();

    ret = ylinterpret_internal(stream, streamsz);
    if(YLOk == ret) { _gctt_set_timer(); }
    else { ylmp_recovery_gc(); }

    _gctt_unlock();

    return ret;
}

void
ylinterpret_undefined(int reason) {
    pthread_exit((void*)reason);
}

ylerr_t 
ylinterp_init() {
    if(0 > _gctt_create() ) {
        return YLErr_internal;
    }
    return YLOk;
}
