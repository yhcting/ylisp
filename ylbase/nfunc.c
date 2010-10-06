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
#include "ylut.h"

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
    return yleis_true(yleval(ylcaar(c), a))? yleval(ylcadar(c), a): _evcon(ylcdr(c), a);
}

YLDEFNF(f_cond, 1, 9999) {
    /* yleq [ylcar [e]; COND] -> evcon [ylcdr [e]; a]; */
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

static void
_evarg(yle_t* e, yle_t** a) {
    yle_t*    r;
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
     */
    _update_assoc(a, yllist(ylcaar(e), yleval(ylcadar(e), *a)));
    _evarg(ylcdr(e), a);
}


YLDEFNF(f_let, 2, 9999) {
    yle_t*  p;
    if(yleis_atom(ylcar(e)) && !yleis_nil(ylcar(e))) { 
        ylnflogE0("incorrect argument syntax\n");
        ylinterpret_undefined(YLErr_func_invalid_param); 
    }
    _evarg(ylcar(e), &a);
    
    e = ylcdr(e);
    while(!yleis_nil(e)) {
        p = yleval(ylcar(e), a);
        e = ylcdr(e);
    }
    return p;
} YLENDNF(f_let)

YLDEFNF(f_while, 2, 9999) {
    static const int __MAX_LOOP_COUNT = 1000000;
    yle_t *cond, *exp;
    int   cnt = 0;
    cond = ylcar(e);
    while( yleis_true(yleval(cond, a)) ) {
        if(cnt < __MAX_LOOP_COUNT) {
            /*
             * Due to return value, memory pool may be over-used.
             * So, try to GC at the end of each loop!
             * (ylmp_push() / ylmp_pop())
             */
            ylmp_push();
            exp = ylcdr(e);
            while(!yleis_nil(exp)) {
                yleval(ylcar(exp), a);
                exp = ylcdr(exp);
            }
            ylmp_pop();

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

YLDEFNF(clone, 1, 1) {
    if(yleis_nil(e)) {
        ylnflogE0("nil cannot be cloned!!\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
    return yleclone_chain(ylcar(e));
} YLENDNF(clone)

/* eq [car [e]; CAR] -> car [eval [cadr [e]; a]] */
YLDEFNF(car, 1, 1) {
    return ylcaar(e);
} YLENDNF(car)

/* eq [car [e]; CDR] -> cdr [eval [cadr [e]; a]] */
YLDEFNF(cdr, 1, 1) {
    return ylcdar(e);
} YLENDNF(cdr)

YLDEFNF(setcar, 2, 2) {
    if( yleis_atom(ylcar(e)) ) {
        ylnflogE0("invalid parameter type\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
    ylpsetcar(ylcar(e), ylcadr(e));
    return ylt();
} YLENDNF(setcar)

YLDEFNF(setcdr, 2, 2) {
    if( yleis_atom(ylcar(e)) ) {
        ylnflogE0("invalid parameter type\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
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
    } else {
        return ylt();
    }
} YLENDNF(assert)

YLDEFNF(exit, 0, 0) {
    ylinterpret_undefined(YLOk);
} YLENDNF(exit)

YLDEFNF(print, 1, 9999) {
    if(yleis_atom(e)) { ylinterpret_undefined(YLErr_func_invalid_param); }
    do {
        ylprint(("%s", yleprint(ylcar(e)) ));
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
        ylutdynb_clean(&b);                     \
    }while(0)

    int          i;
    ylutdynb_t   b;
    const char*  fmt;
    char**       s = NULL;
    if(yleis_atom(e)) { ylinterpret_undefined(YLErr_func_invalid_param); }
    /*
     * allocate memory to save print string
     * First string is 'format string'. So we don't need to alloc memory for this.
     */
    s = (char**)ylmalloc(sizeof(char*) * (pcsz-1));
    if(!s) { goto bail; }
    memset(s, 0, sizeof(char*) * (pcsz-1));
    fmt = ylasym(ylcar(e)).sym;
    for(i=0, e=ylcdr(e); !yleis_nil(e); i++, e=ylcdr(e)) {
        const char* es = yleprint(ylcar(e));
        s[i] = ylmalloc(strlen(es) + 1);
        if(!s[i]) { goto bail; }
        strcpy(s[i], es);
    }

    { /* Just scope */
        int        ret;
        if(0 > ylutdynb_init(&b, 4096)) { goto bail; }  /* initial buffer size is 4096 */
        ylutdynb_reset(&b);
        while(1) {
            switch(pcsz) {
                /*
                 * Using variable string as an printf format string, can be dangerous potentially.
                 * But, using exsiting 'snprintf' is much easier.
                 * So, ... allow this warning "warning: format not a string literal and no format arguments"
                 */
                case 1: ret = snprintf(ylutdynb_ptr(&b), ylutdynb_freesz(&b), fmt); break;
                case 2: ret = snprintf(ylutdynb_ptr(&b), ylutdynb_freesz(&b), fmt, s[0]); break;
                case 3: ret = snprintf(ylutdynb_ptr(&b), ylutdynb_freesz(&b), fmt, s[0], s[1]); break;
                case 4: ret = snprintf(ylutdynb_ptr(&b), ylutdynb_freesz(&b), fmt, s[0], s[1], s[2]); break;
                case 5: ret = snprintf(ylutdynb_ptr(&b), ylutdynb_freesz(&b), fmt, s[0], s[1], s[2], s[3]); break;
                case 6: ret = snprintf(ylutdynb_ptr(&b), ylutdynb_freesz(&b), fmt, s[0], s[1], s[2], s[3], s[4]); break;
                case 7: ret = snprintf(ylutdynb_ptr(&b), ylutdynb_freesz(&b), fmt, s[0], s[1], s[2], s[3], s[4], s[5]); break;
                case 8: ret = snprintf(ylutdynb_ptr(&b), ylutdynb_freesz(&b), fmt, s[0], s[1], s[2], s[3], s[4], s[5], s[6]); break;
                case 9: ret = snprintf(ylutdynb_ptr(&b), ylutdynb_freesz(&b), fmt, s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]); break;
                case 10:ret = snprintf(ylutdynb_ptr(&b), ylutdynb_freesz(&b), fmt, s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8]); break;
                default: ylassert(0);
            }
            if(ret >= ylutdynb_limit(&b)) {
                ylutdynb_reset(&b);
                if(0 > ylutdynb_expand(&b)) { goto bail; }
            } else {
                break;
            }
        }
    }
    ylprint(("%s", ylutdynb_ptr(&b)));
    __cleanup();
    return ylt();

 bail:
    __cleanup();
    ylnflogW0("Not enough memory!\n");
    ylinterpret_undefined(YLErr_func_fail);

#undef __cleanup

} YLENDNF(printf)


YLDEFNF(log, 2, 9999) {
    const char*  lvstr;
    int          loglv;
    ylnfcheck_atype1(ylcar(e), YLASymbol);
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
        yllog((loglv, "%s", yleprint(ylcar(e)) ));
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
    estr = yleprint(ylcar(e));
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

/*===========================
 * Simple calculations
 *===========================*/

YLDEFNF(bit_and, 2, 9999) {
    long   r = -1; /* to make r be 'ffff...fff' type value. - 2's complement */
    ylnfcheck_atype_chain1(e, YLADouble);
    while( !yleis_nil(e) ) {
        if( ((long)yladbl(ylcar(e))) == yladbl(ylcar(e)) ) {
            r &= ((long)yladbl(ylcar(e)));
            e = ylcdr(e);
        } else { goto bail; }
    }
    if(r != ((double)r)) { goto bail; }
    return ylacreate_dbl((double)r);

 bail:
    ylnflogE0("Parameter or return value cannot be cross-changable between long and double!\n");
    ylinterpret_undefined(YLErr_func_invalid_param);
} YLENDNF(bit_and)

YLDEFNF(bit_or, 2, 9999) {
    long   r = 0;
    ylnfcheck_atype_chain1(e, YLADouble);
    while( !yleis_nil(e) ) {
        if( ((long)yladbl(ylcar(e))) == yladbl(ylcar(e)) ) {
            r |= ((long)yladbl(ylcar(e)));
            e = ylcdr(e);
        } else { goto bail; }
    }
    if(r != ((double)r)) { goto bail; }
    return ylacreate_dbl((double)r);

 bail:
    ylnflogE0("Parameter or return value cannot be cross-changable between long and double!\n");
    ylinterpret_undefined(YLErr_func_invalid_param);
} YLENDNF(bit_or)

YLDEFNF(bit_xor, 2, 9999) {
    long   r = 0;
    ylnfcheck_atype_chain1(e, YLADouble);
    while( !yleis_nil(e) ) {
        if( ((long)yladbl(ylcar(e))) == yladbl(ylcar(e)) ) {
            r ^= ((long)yladbl(ylcar(e)));
            e = ylcdr(e);
        } else { goto bail; }
    }
    if(r != ((double)r)) { goto bail; }
    return ylacreate_dbl((double)r);

 bail:
    ylnflogE0("Parameter or return value cannot be cross-changable between long and double!\n");
    ylinterpret_undefined(YLErr_func_invalid_param);
} YLENDNF(bit_xor)


YLDEFNF(mod, 2, 2) {
    long   r = 0;
    ylnfcheck_atype_chain1(e, YLADouble);
    r = ((long)yladbl(ylcar(e))) % ((long)yladbl(ylcadr(e)));
    return ylacreate_dbl(r);
} YLENDNF(add)


YLDEFNF(add, 2, 9999) {
    double   r = 0;
    ylnfcheck_atype_chain1(e, YLADouble);
    while( !yleis_nil(e) ) {
        r += yladbl(ylcar(e));
        e = ylcdr(e);
    }
    return ylacreate_dbl(r);
} YLENDNF(add)

YLDEFNF(mul, 2, 9999) {
    double   r = 0;
    ylnfcheck_atype_chain1(e, YLADouble);
    while( !yleis_nil(e) ) {
        r *= yladbl(ylcar(e));
        e = ylcdr(e);
    }
    return ylacreate_dbl(r);
} YLENDNF(mul)

YLDEFNF(sub, 2, 9999) {
    double   r = 0;
    ylnfcheck_atype_chain1(e, YLADouble);
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
    ylnfcheck_atype_chain1(e, YLADouble);
    r = yladbl(ylcar(e));
    e = ylcdr(e);
    while( yleis_nil(e) ) {
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
    ylnfcheck_atype_chain2(e, YLADouble, YLASymbol);
    switch(ylatype(ylcar(e))) {
        case YLADouble: {
            return (yladbl(p1) > yladbl(p2))? ylt(): ylnil();
        } break;
        case YLASymbol: {
            return (strcmp(ylasym(p1).sym, ylasym(p2).sym) > 0)? ylt(): ylnil();
        } break;
    }
    ylassert(0);
} YLENDNF(gt)

YLDEFNF(lt, 2, 2) {
    yle_t *p1 = ylcar(e), 
          *p2 = ylcadr(e);
    ylnfcheck_atype_chain2(e, YLADouble, YLASymbol);
    switch(ylatype(ylcar(e))) {
        case YLADouble: {
            return (yladbl(p1) < yladbl(p2))? ylt(): ylnil();
        } break;
        case YLASymbol: {
            return (strcmp(ylasym(p1).sym, ylasym(p2).sym) < 0)? ylt(): ylnil();
        } break;
    }
    ylassert(0);
} YLENDNF(lt)

