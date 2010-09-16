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



#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include "trie.h"
#include "lisp.h"

/* global error number */
extern int errno;


/* this is for debugging perforce */
static unsigned int _eval_id = 0;

/* -------------------------------
 * For debugging
 * -------------------------------*/
unsigned int
ylget_eval_id() { return _eval_id; }

/*=================================
 * Recursive S-functions - START
 *=================================*/

const yle_t*
yleq(const yle_t* e1, const yle_t* e2) {
    /* 
     * check trivial case first.
     */
    if(yleis_nil(e1) && yleis_nil(e2)) {
        return ylt();
    } else if( (!yleis_nil(e1) && yleis_nil(e2))
               || (yleis_nil(e1) && !yleis_nil(e2))) {
        return ylnil();
    }

    if(yleis_atom(e1) && yleis_atom(e2)) {
        if(ylatype(e1) == ylatype(e2)) {
            if(ylaif(e1)->eq) { 
                return ((*ylaif(e1)->eq)(e1, e2))? ylt(): ylnil(); 
            } else {
                yllogW0("There is an atom that doesn't support/allow CLEAN!\n");
                return ylnil(); /* default is 'not equal' */
            }
        } else {
            return ylnil();
        }
    } else {
        /* 
         * actually, 'yleq' is only defined only for ylatom-comparison.
         * But for easy-exception-handling,
         *    this returns ylnil() in case of ylnot-ylatom-exp.
         */
        return ylnil();
    }
}

/*=================================
 * Elementary S-functions - END
 *=================================*/



/*=================================
 * Recursive S-functions - START
 *=================================*/

/**
 * assumption : x is atomic symbol, y is a yllist form ((u1 v1) ...)
 * @return NULL if fail to find. container ylpair if found.
 */
static inline yle_t*
_list_find(yle_t* x, yle_t* y) {
    if( yleis_atom(y) || yleis_atom(ylcar(y)) ) { 
        /* check that y is empty list or not. NIL also atom! */
        return NULL;
    } else {
        if( yleis_true(yleq(ylcaar(y), x)) ) {
            return ylcar(y);
        } else {
            return _list_find(x, ylcdr(y));
        }
    }
}

/**
 * a is a yllist of the form ((u1 v1) ... (uN vN))
 * < additional constraints : x is atomic >
 * if x is one of u's, it changes the value of 'u'. If ylnot, it changes global lookup map.
 * @a: atomic symbol
 * @y: any S-expression
 * @a: map yllist
 * @return: new value
 */
yle_t*
ylset(yle_t* x, yle_t* y, yle_t* a, const char* desc) {
    yle_t* r;
    if( !ylais_type(x, YLASymbol)
        || ylnil() == x) {
        yllogE0("Only symbol can be set!\n");
        ylinterpret_undefined(YLErr_eval_undefined); 
    }

    ylassert(ylasym(x).sym);
    r = _list_find(x, a);
    if(r) {
        /* found it */
        ylassert(!yleis_atom(r));
        if(yleis_atom(ylcdr(r))) { 
            yllogE0("unexpected set operation!\n");
            ylinterpret_undefined(YLErr_eval_undefined); 
        } else  {
            /* change value of local map yllist */
            ylpsetcdr(r, ylcons(y, ylnil()));
        }
    } else {
        /* not found - set to global space */
        yltrie_insert(ylasym(x).sym, TRIE_VType_set, y);
        if(desc) {
            yltrie_set_description(ylasym(x).sym, desc);
        }
    }

    return y;
}

yle_t*
ylmset(yle_t* x, yle_t* y, const char* desc) {
    if( !ylais_type(x, YLASymbol)
        || ylnil() == x) {
        yllogE0("Only symbol can be set!\n");
        ylinterpret_undefined(YLErr_eval_undefined); 
    }
    ylassert(ylasym(x).sym);
    /* mset doesn't affect to local variable! */
    yltrie_insert(ylasym(x).sym, TRIE_VType_macro, y);
    if(desc) {
        yltrie_set_description(ylasym(x).sym, desc);
    }

    return y;
}

/*
 * assumption : 
 *    form of 'a' is ((u1 v1) (u2 v2) ...)
 *    e : not atom!
 */
static inline int
_mreplace(yle_t* e, yle_t* a) {
    int     r = 0;
    /* @e SHOULD NOT be an atom! */
    if(!e || yleis_atom(e) || !a) { 
        yllogE0("Wrong syntax of mlambda!\n");
        return -1; 
    }

    dbg_eval(yllogV1("---- macro replace -------\n"
                     "    From :%s\n", yleprint(e));
             yllogV1("    To   :%s\n"
                     "--------------------------\n", yleprint(a)););

    /*
     * 'e' may be in global space. So, changing 'e' may affects global state.
     * And this should not be happened.
     * But, in case of 'mlambda', there is always matching parameter for each argument.
     * So, parameter pointer of expression that is in global space, can be ignored.
     * (It always replaced with newly passed one. So, last value is not referenced anywhere)
     * That's why, it is allowed changing 'e' value, even if it's not doen by 'set' or 'unset'
     */
    if(yleis_atom(ylcar(e))) {
        yle_t* n;
        n = _list_find(ylcar(e), a);
        /*
         * _mreplace uses cloned expression. So, we can change 'e'.
         * set direct link to 'e' is OK I think.
         */
        if(n) { ylpsetcar(e, ylcadr(n)); }
    } else {
        r = _mreplace(ylcar(e), a);
    }

    /* check error */
    if(0 > r) { return -1; }
    if(yleis_atom(ylcdr(e))) {
        yle_t* n;
        n = _list_find(ylcdr(e), a);
        if(n) { ylpsetcdr(e, ylcadr(n)); }
    } else {
        r = _mreplace(ylcdr(e), a);
    }
    return r;
}

/**
 * y is a yllist of the form ((u1 v1) ... (uN vN)) yland x is one of u's then
 * assoc [x; y] = yleq [ylcaar [y]; x] -> ylcadr [y]; T -> assoc [x; ylcdr[y]]
 * 
 * < additional constraints : x is atomic >
 */
const yle_t*
_assoc(int* ovty, yle_t* x, yle_t* y) {
    yle_t* r;
    if( !ylais_type(x, YLASymbol) ) {
        yllogE0("Only symbol can be associated!\n");
        ylinterpret_undefined(YLErr_eval_undefined); 
    }

    /*
     * Policy:
     *    Find in symbol hash first. If fails, try to evaluate as number!.
     *    (For fast symbol binding!)
     */
    r = _list_find(x, y);
    if(r) { 
        /* Found! in local association list */
        *ovty = TRIE_VType_set;
        return ylcadr(r);
    } else {
        r = (yle_t*)yltrie_get(ovty, ylasym(x).sym);
        if(r) { 
            if(TRIE_VType_macro == *ovty) {
                /* 
                 * !! IMPORTANT NOTE !!
                 *    This SHOULD NOT BE THE ONE IN GLOBAL SPACE!!
                 *    Expression should be preserved!
                 *    Concept of macro(mset/mlambda) is different from 'set/lambda'.
                 *    Concept is NOT "get and use stored data".
                 *        - in this context, retrieved data can be changed!.
                 *    But, concept is "Replace it with pre-defined expression!"
                 *        - in this context, pre-defined expression SHOULD NOT be changed at any cases.
                 *    So, we should use cloned one!
                 */
                return yleclone_chain(r);
            } else {
                return r;
            }
        } else {
            /* 
             * check whether this represents number or not
             * (string that representing number associated to 'double' type value )
             */
            char*   endp;
            double  d;
            d = strtod(ylasym(x).sym, &endp);
            if( 0 == *endp && ERANGE != errno ) {
                /* default is "NOT macro". So set as 'TRIE_VType_set' */
                *ovty = TRIE_VType_set;
                /* right coversion - let's assign double type atom*/
                return ylacreate_dbl(d);
            }

            yllogE1("symbol [%s] was not set!\n", ylasym(x).sym);
            ylinterpret_undefined(YLErr_eval_undefined);
        }
    }
}

/*=================================
 * Recursive S-functions - END
 *=================================*/

/*=================================
 * Universal S-functions - START
 *=================================*/


yle_t*
yleval(yle_t* e, yle_t* a) {
#define _cmp(x, e) !strcmp(#x, ylasym(e).sym)

    yle_t*       r = NULL;
    int          vty;
    unsigned int evid = _eval_id++; /* evaluation id for debugging */

    /* This is right place to interrupt evaluation! */
    ylinteval_unlock();
    ylinteval_lock();

    dbg_eval(yllogV2("[%d] eval In:\n"
                     "    %s\n", evid, yleprint(e)););
    dbg_eval(yllogV1("    =>%s\n", yleprint(a)););
    dbg_mem(yllogV4("START eval:\n"
                    "    MP usage : %d\n"
                    "    refcnt : nil(%d), t(%d), q(%d)\n", 
                    ylmp_usage(), ylercnt(ylnil()), 
                    ylercnt(ylt()), ylercnt(ylq())););
    
    /*
     * 'e' and 'a' should be preserved during evaluation.
     * So, add reference count manually!
     * (especially, in case of a, some functions (ex let) may add new assoc. element
     *  in front of current 'a'. In case of this, without adding reference count,
     *  current assoc. list may be destroied due to this is also in chain!)
     */
    ylercnt(e)++; ylercnt(a)++;
    /*
     * Push(Save) that current memory pool status before evaluation.
     */
    ylmp_push();
    if( yleis_nil(e) ) {
        yllogE0("ERROR : nil cannot be evaluated!\n");
        ylinterpret_undefined(YLErr_eval_undefined);
    }

    if( yleis_atom(e) ) { 
        if(YLASymbol == ylatype(e)) {
            r = (yle_t*)_assoc(&vty, e, a);
            if(TRIE_VType_macro == vty) {
                /* this is macro!. evaluate it again */
                r = yleval(r, a);
            }
        } else {
            yllogE0("ERROR to evaluate atom(only symbol can be evaluated!.\n");
            ylinterpret_undefined(YLErr_eval_undefined);
        }
    } else if( yleis_atom(ylcar(e)) ) {
        yle_t*   car_e = ylcar(e);
        yle_t*   p;
        if(YLASymbol == ylatype(car_e) ) {
            r = (yle_t*)_assoc(&vty, car_e, a);
            if(TRIE_VType_macro == vty) {
                /* 
                 * This is macro symbol! replace target expression with symbol.
                 * And evaluate it with replaced value!
                 */
                r = yleval( ylcons(r, ylcdr(e)), a );
            } else {
                if(yleis_atom(r)) {
                    if(YLANfunc == ylatype(r)) {
                        r = (*(ylanfunc(r).f))(ylevlis(ylcdr(e), a), a);
                    } else if(YLASfunc == ylatype(r)) {
                        /* parameters are not evaluated before being passed! */
                        r = (*(ylanfunc(r).f))(ylcdr(e), a);
                    } else {
                        yllogE0("ERROR to evaluate. First element should be a function!\n");
                        ylinterpret_undefined(YLErr_eval_undefined);
                    }
                } else {
                    yllogE0("ERROR to evaluate. First element should be an atom that represets function!\n");
                    ylinterpret_undefined(YLErr_eval_undefined);
                }
            } 
        } else {
            yllogE0("ERROR to evaluate. Only symbol and list can be evaluated!\n");
            ylinterpret_undefined(YLErr_eval_undefined);
        }
    } else {
        yle_t* ce = ylcaar(e);
        if(_cmp(label, ce)) {
            /* eq [caar [e]; LABEL] -> eval [cons [caddar [e]; cdr [e]]; cons [list [cadar [e]; car [e]]; a]] */
            r = yleval(ylcons(ylcaddar(e), ylcdr(e)), ylcons(yllist(ylcadar(e), ylcar(e)), a));
        } else if(_cmp(lambda, ce)) {
            /* eq [caar [e] -> LAMBDA] -> eval [caddar [e]; append [pair [cadar [e]; evlis [cdr [e]; a]]; a]] */
            r = yleval(ylcaddar(e), ylappend(ylpair(ylcadar(e), ylevlis(ylcdr(e), a)), a));
        } else if(_cmp(mlambda, ce)) {
            if( yleis_nil(ylcadar(e)) && !yleis_nil(ylcaddar(e)) ) {
                /* 
                 * !!! Special usage of mlambda !!!
                 * If parameter is nil, than, arguments are appended to the body!
                 */
                yle_t* we = ylcaddar(e);
                while(!yleis_nil(ylcdr(we))) { we = ylcdr(we); }
                /* 
                 * Expression itself is preserved at any case.
                 * So, we don't need to care about replacement of expression.
                 * (restoring link is enough!)
                 */
                /* connect body with argument */
                ylpsetcdr(we, ylcdr(e));
                r = yleval(ylcaddar(e), a);
                /* restore to original value - nil */
                ylpsetcdr(we, ylnil());
            } else {
                /* 
                 * NOTE!!
                 *     expression itself may be changed in '_mreplace'.
                 *     To reserve original expression, we need to use cloned one. 
                 *  --> Now expression is preserved!
                 */
                yle_t* exp = yleclone_chain(ylcaddar(e));
                if(0 > _mreplace(exp, ylappend(ylpair(ylcadar(e), ylcdr(e)), ylnil()))) {
                    yllogE0("Fail to mreplace!!\n");
                    ylinterpret_undefined(YLErr_eval_undefined);
                } else {
                    r = yleval(exp, a);
                }
            }            
        } else {
            yle_t *p1, *p2;
            /* my extention */
            r = yleval(ylcons(yleval(ylcar(e), a), ylcdr(e)), a);
            /* ylinterpret_undefined(YLErr_eval_undefined); */
        }
    }

#undef _cmp

    if(r) {
        
        dbg_eval(yllogV2("[%d] eval Out:\n"
                         "    %s\n", evid, yleprint(r)););


        /*
         * GC dangling blocks that are newly used during this evaluation.
         * Exceptions are blocks for return values and association lists.
         */
        /*
         * Returned value should be pretected from GC.
         * So, refer it manually
         * To prevent from freeing when reference cnt becomes 0,
         *  reference count of r is modifed directly!!
         * Actually, this is very dangerous.
         * So, BE CAREFUL WHEN MODIFY THIS!
         */
        ylercnt(r)++;
        ylercnt(e)--; ylercnt(a)--;

        ylmp_pop();
        ylercnt(r)--;


        dbg_mem(yllogV4("END eval :\n"
                        "    MP usage : %d\n"
                        "    refcnt : nil(%d), t(%d), q(%d)\n", 
                        ylmp_usage(), ylercnt(ylnil()), 
                        ylercnt(ylt()), ylercnt(ylq())););
        return r;
    } else {
        yllogE0("NULL return! is it possible!\n");
        ylinterpret_undefined(YLErr_eval_undefined);
    }
}


/**
 * appq [m] = [null [m] -> NIL; T -> cons [list [QUOTE; car [m]]; appq[ cdr [m]]]]
 */
static inline yle_t*
_appq(yle_t* m) {
    if( yleis_true(ylnull(m)) ) {
        return ylnil();
    } else {
        return ylcons(yllist(ylq(), ylcar(m)), _appq(ylcdr(m)));
    }
}

/**
 * apply [f; args] = eval [cons [f; appq [args]]; NIL]
 */
yle_t*
ylapply(yle_t* f, yle_t* args, yle_t* a) {
    return yleval(ylcons(f, _appq(args)), a);
}

/*=================================
 * Universal S-functions - END
 *=================================*/



ylerr_t
ylsfunc_init() {
    return YLOk;
}
