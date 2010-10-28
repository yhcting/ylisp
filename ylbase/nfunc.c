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



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* enable logging & debugging */
#define CONFIG_ASSERT
#define CONFIG_LOG

#include "ylsfunc.h"
#include "yldynb.h"

/*
 * GC Protection required to caller
 * ASSUMPTION
 *    'c' and 'a' is already GC-protected.
 */
static inline yle_t*
_evcon(yle_t* c, yle_t* a) {
    /*
     * If there is no-true condition, 'cond' returns nil()
     * (This is some-what different from stuffs described in the S-Expression Report!
     *  In the report, this case is 'undefined')
     */
    if(yleis_nil(c)) {
        return ylnil();
    }
    if( yleis_atom(ylcar(c))
        || yleis_nil(ylcaar(c)) ) {
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
    /*
     * 'a' is always preserved. (whole a is passed as an parameter.)
     * But, we need to preserve 'c' explicitly.
     */
    return yleis_true(yleval(ylcaar(c), a))? yleval(ylcadar(c), a): _evcon(ylcdr(c), a);
}

YLDEFNF(f_cond, 1, 9999) {
    /* eq [car [e]; COND] -> evcon [cdr [e]; a]; */
    return _evcon(e, a);
} YLENDNF(f_cond)

YLDEFNF(f_and, 1, 9999) {
    while(!yleis_nil(e)) {
        if( yleis_false(yleval(ylcar(e), a)) ) { return ylnil(); }
        e = ylcdr(e);
    }
    return ylt();

} YLENDNF(f_and)

YLDEFNF(f_or, 1, 9999) {
    while(!yleis_nil(e)) {
        if( yleis_true(yleval(ylcar(e), a)) ) { return ylt(); }
        e = ylcdr(e);
    }
    return ylnil();
} YLENDNF(f_or)


/*
 * Attach new (x y) form in front of assoc. list.
 * And update it.
 */
static inline void
_update_assoc(yle_t** x, yle_t* y) {
    if(yleis_nil(y)) { return; }

    if(yleis_nil(*x)) {
        *x = ylcons(y, ylnil());
    } else {
        if(yleis_atom(*x)) {
            yllogE0("<!let!> incorrect argument syntax\n");
            ylinterpret_undefined(YLErr_func_invalid_param);
        } else {
            *x = ylcons(y, *x);
        }
    }
}

/*
 * GC Protection required to caller
 * ASSUMPTION
 *    'e' and '*a' is already GC-protected.
 */
static void
_evarg(yle_t* e, yle_t** a) {
    if(yleis_nil(e)) { return; }
    if(yleis_atom(e)
       || yleis_atom(ylcar(e))
       || !yleis_atom(ylcaar(e))) {
        /* ylinterpret_undefined.. syntax error */
        yllogE0("<!let!> incorrect argument syntax\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }

    /*
     * Expression itself SHOULD NOT be changed.
     * So, yllist(...) is used instead of ylpcar/ylpcdr!
     *
     * !IMPORTANT :
     *    '*a' should be protected from GC.
     *    '*a' is updated. So, old value should be preserved.
     */
    ylmp_push1(*a);
    _update_assoc(a, yllist(ylcaar(e), yleval(ylcadar(e), *a)));
    ylmp_pop1();
    _evarg(ylcdr(e), a);
}


YLDEFNF(f_let, 2, 9999) {
    yle_t*  p;
    if(yleis_atom(ylcar(e)) && !yleis_nil(ylcar(e))) {
        ylnflogE0("incorrect argument syntax\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
    _evarg(ylcar(e), &a);

    /*
     * updated 'a' should be protected from GC
     * ('e' is already protected!)
     */
    ylmp_push1(a);
    e = ylcdr(e);
    while(!yleis_nil(e)) {
        p = yleval(ylcar(e), a);
        e = ylcdr(e);
    }
    ylmp_pop1();
    return p;
} YLENDNF(f_let)


YLDEFNF(f_case, 2, 9999) {
#define __DEFAULT_KEYWORD "otherwise"
    yle_t *key, *r = ylnil();
    ylnfcheck_parameter(yleis_pair_chain(ylcdr(e)));
    key = yleval(ylcar(e), a);
    ylmp_push1(key);

    e = ylcdr(e);
    ylelist_foreach(e) {
        if(ylais_type(ylcaar(e), ylaif_sym())
           && (0 == strcmp(ylasym(ylcaar(e)).sym, __DEFAULT_KEYWORD))) {
            /* unconditionally TRUE */
            break;
        } else if(yleis_atom(ylcaar(e))) {
            if(yleis_true(yleq(key, yleval(ylcaar(e), a)))) { break; }
        } else {
            /* key is list (key list) */
            yle_t*     w = ylcaar(e);
            ylelist_foreach(w) {
                if(yleis_true(yleq(key, yleval(ylcar(w), a)))) { break; }
            }
            if(!yleis_nil(w)) { break; }
        }
    }

    ylmp_pop1(); /* key */

    if(!yleis_nil(e)) {
        /* we found! */
        yle_t*   w = ylcdar(e);
        ylelist_foreach(w) {
            r = yleval(ylcar(w), a);
        }
    }

    return r;

#undef __DEFAULT_KEYWORD
} YLENDNF(f_case)


YLDEFNF(f_while, 2, 9999) {
    static const int __MAX_LOOP_COUNT = 1000000;
    yle_t *cond, *exp;
    int   cnt = 0;
    cond = ylcar(e);
    while( yleis_true(yleval(cond, a)) ) {
        if(cnt < __MAX_LOOP_COUNT) {
            exp = ylcdr(e);
            while(!yleis_nil(exp)) {
                yleval(ylcar(exp), a);
                exp = ylcdr(exp);
            }
        } else {
            ylnflogE1("Loop count exceeded limits(%d)\n", __MAX_LOOP_COUNT);
            ylinterpret_undefined(YLErr_func_fail);
        }
    }
    return ylt();
} YLENDNF(f_while)

/* eq [car [e]; ATOM] -> atom [eval [cadr [e]; a]] */
YLDEFNF(atom, 1, 1) {
    return (yle_t*)ylatom(ylcar(e));
} YLENDNF(atom)

YLDEFNF(type, 1, 1) {
#define __SZ (1024) /* This is big enough */
    int    bw;
    char   b[__SZ]; /* even 128bit machine, 32bit is enough */
    if(yleis_atom(ylcar(e))) {
        bw = snprintf(b, __SZ, "%p", ylaif(ylcar(e)));
        ylassert(bw < __SZ); /* check unexpected result */
    } else {
        strcpy(b, "0x0");
    }

    { /* Just Scope */
        char*  s = ylmalloc(strlen(b) + 1);
        ylassert(s);
        strcpy(s, b);
        return ylacreate_sym(s);
    }
#undef __SZ
} YLENDNF(type)

#if 0 /* Keep it for future use! */
YLDEFNF(clone, 1, 1) {
} YLENDNF(clone)
#endif /* Keep it for future use! */

/* eq [car [e]; CAR] -> car [eval [cadr [e]; a]] */
YLDEFNF(car, 1, 1) {
    return ylcaar(e);
} YLENDNF(car)

/* eq [car [e]; CDR] -> cdr [eval [cadr [e]; a]] */
YLDEFNF(cdr, 1, 1) {
    return ylcdar(e);
} YLENDNF(cdr)

YLDEFNF(setcar, 2, 2) {
    ylnfcheck_parameter(!yleis_atom(ylcar(e)));
    ylpsetcar(ylcar(e), ylcadr(e));
    return ylt();
} YLENDNF(setcar)

YLDEFNF(setcdr, 2, 2) {
    ylnfcheck_parameter(!yleis_atom(ylcar(e)));
    ylpsetcdr(ylcar(e), ylcadr(e));
    return ylt();
} YLENDNF(setcdr)

/* eq [car [e]; CONS] -> cons [eval [cadr [e]; a]; eval [caddr [e]; a]] */
YLDEFNF(cons, 2, 2) {
    return ylcons(ylcar(e), ylcadr(e));
} YLENDNF(cons)

YLDEFNF(null, 1, 1) {
    return ylnull(ylcar(e));
} YLENDNF(null)

static yle_t*
_list_cons(yle_t* e) {
    return yleis_nil(e)? ylnil(): ylcons (ylcar(e), _list_cons(ylcdr(e)));
}

YLDEFNF(list, 0, 9999) {
    return _list_cons(e);
} YLENDNF(list)

YLDEFNF(assert, 1, 1) {
    if(yleis_nil(ylcar(e))) {
        ylnflogE0("ASSERT FAILS\n");
        ylinterpret_undefined(YLErr_eval_assert);
        return NULL; /* to make compiler be happy. */
    } else {
        return ylt();
    }
} YLENDNF(assert)

YLDEFNF(exit, 0, 0) {
    ylinterpret_undefined(YLOk);
    return NULL; /* to make compiler be happy. */
} YLENDNF(exit)

YLDEFNF(print, 1, 9999) {
    ylnfcheck_parameter(!yleis_atom(e));
    do {
        ylprint(("%s", ylechain_print(ylcar(e)) ));
        e = ylcdr(e);
    } while(!yleis_nil(e));
    return ylt();
} YLENDNF(print)


YLDEFNF(printf, 1, 10) {

#define __cleanup()                             \
    do {                                        \
        for(i=0; i<(pcsz-1); i++) {             \
            if(s[i]) { ylfree(s[i]); }          \
        }                                       \
        if(s) { ylfree(s); }                    \
        yldynb_clean(&b);                       \
    }while(0)

    int          i;
    yldynb_t     b;
    const char*  fmt;
    char**       s = NULL;

    ylnfcheck_parameter(!yleis_atom(e));
    /*
     * allocate memory to save print string
     * First string is 'format string'. So we don't need to alloc memory for this.
     */
    s = (char**)ylmalloc(sizeof(char*) * (pcsz-1));
    if(!s) { goto bail; }
    memset(s, 0, sizeof(char*) * (pcsz-1));
    fmt = ylasym(ylcar(e)).sym;
    for(i=0, e=ylcdr(e); !yleis_nil(e); i++, e=ylcdr(e)) {
        const char* es = ylechain_print(ylcar(e));
        s[i] = ylmalloc(strlen(es) + 1);
        if(!s[i]) { goto bail; }
        strcpy(s[i], es);
    }

    { /* Just scope */
        int        ret;
        if(0 > yldynb_init(&b, 4096)) { goto bail; }  /* initial buffer size is 4096 */
        yldynb_reset(&b);
        while(1) {
            switch(pcsz) {
                /*
                 * Using variable string as an printf format string, can be dangerous potentially.
                 * But, using exsiting 'snprintf' is much easier.
                 * So, ... allow this warning "warning: format not a string literal and no format arguments"
                 */
                case 1: ret = snprintf((char*)yldynb_ptr(&b), yldynb_freesz(&b), fmt); break;
                case 2: ret = snprintf((char*)yldynb_ptr(&b), yldynb_freesz(&b), fmt, s[0]); break;
                case 3: ret = snprintf((char*)yldynb_ptr(&b), yldynb_freesz(&b), fmt, s[0], s[1]); break;
                case 4: ret = snprintf((char*)yldynb_ptr(&b), yldynb_freesz(&b), fmt, s[0], s[1], s[2]); break;
                case 5: ret = snprintf((char*)yldynb_ptr(&b), yldynb_freesz(&b), fmt, s[0], s[1], s[2], s[3]); break;
                case 6: ret = snprintf((char*)yldynb_ptr(&b), yldynb_freesz(&b), fmt, s[0], s[1], s[2], s[3], s[4]); break;
                case 7: ret = snprintf((char*)yldynb_ptr(&b), yldynb_freesz(&b), fmt, s[0], s[1], s[2], s[3], s[4], s[5]); break;
                case 8: ret = snprintf((char*)yldynb_ptr(&b), yldynb_freesz(&b), fmt, s[0], s[1], s[2], s[3], s[4], s[5], s[6]); break;
                case 9: ret = snprintf((char*)yldynb_ptr(&b), yldynb_freesz(&b), fmt, s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]); break;
                case 10:ret = snprintf((char*)yldynb_ptr(&b), yldynb_freesz(&b), fmt, s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8]); break;
                default: ylassert(0);
            }
            if(ret >= yldynb_limit(&b)) {
                yldynb_reset(&b);
                if(0 > yldynb_expand(&b)) { goto bail; }
            } else {
                break;
            }
        }
    }
    ylprint(("%s", (char*)yldynb_ptr(&b)));
    __cleanup();
    return ylt();

 bail:
    __cleanup();
    ylnflogW0("Not enough memory!\n");
    ylinterpret_undefined(YLErr_func_fail);

#undef __cleanup

    return NULL; /* to make compiler be happy. */
} YLENDNF(printf)


YLDEFNF(log, 2, 9999) {
    const char*  lvstr;
    int          loglv;
    ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym()));
    lvstr = ylasym(ylcar(e)).sym;
    if(0 == lvstr[0] || 0 != lvstr[1]) { goto invalid_loglv; }
    switch(lvstr[0]) {
        case 'v': loglv = YLLogV; break;
        case 'd': loglv = YLLogD; break;
        case 'i': loglv = YLLogI; break;
        case 'w': loglv = YLLogW; break;
        case 'e': loglv = YLLogE; break;
        default: goto invalid_loglv;
    }
    do {
        yllog((loglv, "%s", ylechain_print(ylcar(e)) ));
        e = ylcdr(e);
    } while(!yleis_nil(e));
    return ylt();

 invalid_loglv:
    ylnflogE1("Invalid loglevel: %s\n", lvstr);
    return ylnil();
} YLENDNF(log)

YLDEFNF(to_string, 1, 1) {
    unsigned int len;
    const char*  estr;
    char*        s;
    estr = ylechain_print(ylcar(e));
    ylassert(estr);
    len = strlen(estr);
    s = ylmalloc(len+1); /* +1 for trailing 0 */
    if(!s) {
        ylnflogE1("Fail to alloc memory for string : %d\n", len);
        return ylnil();
    }
    strcpy(s, estr);
    return ylacreate_sym(s);
} YLENDNF(to_string)

YLDEFNF(concat, 2, 9999) {
    char*            buf = NULL;
    unsigned int     len;
    yle_t*           pe;
    char*            p;

    /* check input parameter */
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

    /* calculate total string length */
    len = 0; pe = e;
    while(!yleis_nil(pe)) {
        len += strlen(ylasym(ylcar(pe)).sym);
        pe = ylcdr(pe);
    }

    /* alloc memory and start to copy */
    buf = ylmalloc(len*sizeof(char) +1); /* '+1' for trailing 0 */
    if(!buf) {
        ylnflogE1("Not enough memory: required [%d]\n", len);
        ylinterpret_undefined(YLErr_func_fail);
    }

    pe = e; p = buf;
    while(!yleis_nil(pe)) {
        strcpy(p, ylasym(ylcar(pe)).sym);
        p += strlen(ylasym(ylcar(pe)).sym);
        pe = ylcdr(pe);
    }
    *p = 0; /* trailing 0 */

    return ylacreate_sym(buf);

} YLENDNF(concat)

/*===========================
 * fundamental calculations
 *===========================*/
static inline double
_integer(double x) {
    return (double)((long long)x);
}

YLDEFNF(integer, 1, 1) {
    double r;
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    r = _integer(yladbl(ylcar(e)));
    return ylacreate_dbl(r);
} YLENDNF(integer)

YLDEFNF(fraction, 1, 1) {
    double r;
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    r = yladbl(ylcar(e)) - _integer(yladbl(ylcar(e)));
    return ylacreate_dbl(r);
} YLENDNF(fraction)

YLDEFNF(bit_and, 2, 9999) {
    long long   r = -1; /* to make r be 'ffff...fff' type value. - 2's complement */
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    while( !yleis_nil(e) ) {
        if( ((long long)yladbl(ylcar(e))) == yladbl(ylcar(e)) ) {
            r &= ((long long)yladbl(ylcar(e)));
            e = ylcdr(e);
        } else { goto bail; }
    }
    if(r != ((double)r)) { goto bail; }
    return ylacreate_dbl((double)r);

 bail:
    ylnflogE0("Parameter or return value cannot be cross-changable between long long and double!\n");
    ylinterpret_undefined(YLErr_func_invalid_param);
    return NULL; /* to make compiler be happy. */
} YLENDNF(bit_and)

YLDEFNF(bit_or, 2, 9999) {
    long long  r = 0;
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    while( !yleis_nil(e) ) {
        if( ((long long)yladbl(ylcar(e))) == yladbl(ylcar(e)) ) {
            r |= ((long long)yladbl(ylcar(e)));
            e = ylcdr(e);
        } else { goto bail; }
    }
    if(r != ((double)r)) { goto bail; }
    return ylacreate_dbl((double)r);

 bail:
    ylnflogE0("Parameter or return value cannot be cross-changable between long long and double!\n");
    ylinterpret_undefined(YLErr_func_invalid_param);
    return NULL; /* to make compiler be happy. */
} YLENDNF(bit_or)

YLDEFNF(bit_xor, 2, 9999) {
    long long  r = 0;
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    while( !yleis_nil(e) ) {
        if( ((long long)yladbl(ylcar(e))) == yladbl(ylcar(e)) ) {
            r ^= ((long long)yladbl(ylcar(e)));
            e = ylcdr(e);
        } else { goto bail; }
    }
    if(r != ((double)r)) { goto bail; }
    return ylacreate_dbl((double)r);

 bail:
    ylnflogE0("Parameter or return value cannot be cross-changable between long long and double!\n");
    ylinterpret_undefined(YLErr_func_invalid_param);

    return NULL; /* to make compiler be happy. */
} YLENDNF(bit_xor)


YLDEFNF(mod, 2, 2) {
    long long  r = 0;
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    r = ((long long)yladbl(ylcar(e))) % ((long long)yladbl(ylcadr(e)));
    return ylacreate_dbl(r);
} YLENDNF(add)


YLDEFNF(add, 2, 9999) {
    double   r = 0;
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    while( !yleis_nil(e) ) {
        r += yladbl(ylcar(e));
        e = ylcdr(e);
    }
    return ylacreate_dbl(r);
} YLENDNF(add)

YLDEFNF(mul, 2, 9999) {
    double   r = 1;
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    while( !yleis_nil(e) ) {
        r *= yladbl(ylcar(e));
        e = ylcdr(e);
    }
    return ylacreate_dbl(r);
} YLENDNF(mul)

YLDEFNF(sub, 2, 9999) {
    double   r = 0;
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    r = yladbl(ylcar(e));
    e = ylcdr(e);
    while( !yleis_nil(e) ) {
        r -= yladbl(ylcar(e));
        e = ylcdr(e);
    }
    return ylacreate_dbl(r);
} YLENDNF(sub)


YLDEFNF(div, 2, 9999) {
    double   r = 0;
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    r = yladbl(ylcar(e));
    e = ylcdr(e);
    while( !yleis_nil(e) ) {
        if(0 == yladbl(ylcar(e))) {
            ylnflogE0("divide by zero!!!\n");
            ylinterpret_undefined(YLErr_func_fail);
        }
        r /= yladbl(ylcar(e));
        e = ylcdr(e);
    }
    return ylacreate_dbl(r);
} YLENDNF(div)

YLDEFNF(gt, 2, 2) {
    yle_t *p1 = ylcar(e),
          *p2 = ylcadr(e);
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym())
                        || ylais_type_chain(e, ylaif_dbl()));
    if(ylaif_dbl() == ylaif(ylcar(e))) {
        return (yladbl(p1) > yladbl(p2))? ylt(): ylnil();
    } else {
        return (strcmp(ylasym(p1).sym, ylasym(p2).sym) > 0)? ylt(): ylnil();
    }
} YLENDNF(gt)

YLDEFNF(lt, 2, 2) {
    yle_t *p1 = ylcar(e),
          *p2 = ylcadr(e);
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym())
                        || ylais_type_chain(e, ylaif_dbl()));
    if(ylaif_dbl() == ylaif(ylcar(e))) {
        return (yladbl(p1) < yladbl(p2))? ylt(): ylnil();
    } else {
        return (strcmp(ylasym(p1).sym, ylasym(p2).sym) < 0)? ylt(): ylnil();
    }
} YLENDNF(lt)

