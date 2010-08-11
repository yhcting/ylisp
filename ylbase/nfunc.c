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
#include "ylsfunc.h"


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
            yllogE(("LET: incorrect argument syntax\n"));
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
        yllogE(("LET: incorrect argument syntax\n"));
        ylinterpret_undefined(YLErr_func_invalid_param);
    }

    ylpsetcar(ylcdar(e), yleval(ylcadar(e), *a));
    _update_assoc(a, ylcar(e));
    _evarg(ylcdr(e), a);
}


YLDEFNF(f_let, 2, 9999) {
    yle_t*  p;
    if(yleis_atom(ylcar(e)) && !yleis_nil(ylcar(e))) { 
        yllogE(("LET: incorrect argument syntax\n"));
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
            yllogE(("Loop count exceeded limits(%d)\n", __MAX_LOOP_COUNT));
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
        yllogE(("<!clone!> nil cannot be cloned!!\n"));
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
        yllogE(("<!setcar!> invalid parameter type\n"));
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
    ylpsetcar(ylcar(e), ylcadr(e));
    return ylt();
} YLENDNF(setcar)

YLDEFNF(setcdr, 2, 2) {
    if( yleis_atom(ylcar(e)) ) {
        yllogE(("<!setcar!> invalid parameter type\n"));
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
        yllogE(("<!assert!> ASSERT FAILS\n"));
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
    if(ylsysv()->loglv <= YLLog_output) {
        do {
            ylprint(("%s", yleprint(ylcar(e)) ));
            e = ylcdr(e);
        } while(!yleis_nil(e));
    }
} YLENDNF(print)

/*===========================
 * Simple calculations
 *===========================*/

YLDEFNF(mod, 2, 2) {
    long   r = 0;
    ylcheck_chain_atom_type1(add, e, YLADouble);
    r = ((long)yladbl(ylcar(e))) % ((long)yladbl(ylcadr(e)));
    e = ylmp_get_block();
    ylaassign_dbl(e, r);
    return e;
} YLENDNF(add)


YLDEFNF(add, 2, 9999) {
    double   r = 0;
    ylcheck_chain_atom_type1(add, e, YLADouble);
    while( !yleis_nil(e) ) {
        r += yladbl(ylcar(e));
        e = ylcdr(e);
    }
    e = ylmp_get_block();
    ylaassign_dbl(e, r);
    return e;
} YLENDNF(add)

YLDEFNF(mul, 2, 9999) {
    double   r = 0;
    ylcheck_chain_atom_type1(mul, e, YLADouble);
    while( !yleis_nil(e) ) {
        r *= yladbl(ylcar(e));
        e = ylcdr(e);
    }
    e = ylmp_get_block();
    ylaassign_dbl(e, r);
    return e;
} YLENDNF(mul)

YLDEFNF(sub, 2, 9999) {
    double   r = 0;
    ylcheck_chain_atom_type1(sub, e, YLADouble);
    r = yladbl(ylcar(e));
    e = ylcdr(e);
    while( !yleis_nil(e) ) {
        r -= yladbl(ylcar(e));
        e = ylcdr(e);
    }
    e = ylmp_get_block();
    ylaassign_dbl(e, r);
    return e;
} YLENDNF(sub)


YLDEFNF(div, 2, 9999) {
    double   r = 0;
    ylcheck_chain_atom_type1(div, e, YLADouble);
    r = yladbl(ylcar(e));
    e = ylcdr(e);
    while( yleis_nil(e) ) {
        if(0 == yladbl(ylcar(e))) {
            yllogE(("<!div!> divide by zero!!!\n"));
            ylinterpret_undefined(YLErr_func_fail);
        }
        r /= yladbl(ylcar(e));
        e = ylcdr(e);
    }
    e = ylmp_get_block();
    ylaassign_dbl(e, r);
    return e;
} YLENDNF(div)

YLDEFNF(gt, 2, 2) {
    yle_t *p1 = ylcar(e), 
          *p2 = ylcadr(e);
    ylcheck_chain_atom_type2(gt, e, YLADouble, YLASymbol);
    if(ylatype(ylcar(e)) != ylatype(ylcadr(e))) {
        yllogE(("<!gt!> Invalid paramter!\n"));
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
    switch(ylatype(ylcar(e))) {
        case YLADouble: {
            return (yladbl(p1) > yladbl(p2))? ylt(): ylnil();
        } break;
        case YLASymbol: {
            return (strcmp(ylasym(p1).sym, ylasym(p2).sym) > 0)? ylt(): ylnil();
        } break;
        default:
            yllogE(("<!gt!> Only number or string can be compared\n"));
    }
} YLENDNF(gt)

YLDEFNF(lt, 2, 2) {
    yle_t *p1 = ylcar(e), 
          *p2 = ylcadr(e);
    ylcheck_chain_atom_type2(gt, e, YLADouble, YLASymbol);
    if(ylatype(ylcar(e)) != ylatype(ylcadr(e))) {
        yllogE(("<!lt!> Invalid paramter!\n"));
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
    switch(ylatype(ylcar(e))) {
        case YLADouble: {
            return (yladbl(p1) < yladbl(p2))? ylt(): ylnil();
        } break;
        case YLASymbol: {
            return (strcmp(ylasym(p1).sym, ylasym(p2).sym) < 0)? ylt(): ylnil();
        } break;
        default:
            yllogE(("<!lt!> Only number or string can be compared\n"));
    }
} YLENDNF(lt)

