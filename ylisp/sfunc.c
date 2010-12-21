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



#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "lisp.h"

#ifdef CONFIG_DBG_EVAL
/*
 * this is for debugging perforce
 * Let's ignore Race Condition in MT evaluation.
 * It's just for debugging and not a big deal!
 */
static unsigned int _eval_id = 0;

/* -------------------------------
 * For debugging
 * -------------------------------*/
unsigned int
yleval_id() { return _eval_id; }

#endif /* CONFIG_DBG_EVAL */

/* -------------------------------
 * Utility Functions
 * -------------------------------*/

/*
 * Copy only pair information.
 * (This doesn't clone atom!)
 * It doesn't detect cycle due to performance reason.
 */
static inline yle_t*
_list_clone(yle_t* e) {
    if(yleis_atom(e)) { return e; } /* nothing to clone */
    else {
        return ylcons(_list_clone(ylpcar(e)), _list_clone(ylpcdr(e)));
    }
}



/*=================================
 * Recursive S-functions - START
 *=================================*/
/*
 * It doesn't detect cycle due to performance reason.
 */
const yle_t*
yleq(const yle_t* e1, const yle_t* e2) {
    if(e1 == e2) { return ylt(); } /* trivial case (even if e1 and e2 are pair-type.*/
    if(yleis_atom(e1) && yleis_atom(e2)) {
        if( ylaif(e1)->eq == ylaif(e2)->eq ) {
            if(ylaif(e1)->eq) {
                return ylaif(e1)->eq(e1, e2)? ylt(): ylnil();
            } else {
                yllogW0("There is an atom that doesn't support/allow EQ!\n");
                return ylnil(); /* default is 'not equal' */
            }
        } else {
            return ylnil();
        }
    } else if(!yleis_atom(e1) && !yleis_atom(e2)) {
        return ( yleis_nil(yleq(ylcar(e1), ylcar(e2)))
                 || yleis_nil(yleq(ylcdr(e1), ylcdr(e2))) )?
            ylnil(): ylt();

    } else { return ylnil(); }
}

/*=================================
 * Elementary S-functions - END
 *=================================*/



/*=================================
 * Recursive S-functions - START
 *=================================*/

/**
 * assumption : x is atomic symbol, y is a list form ((u1 v1) ...)
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


/*
 * helper function to simplify '_set' function.
 */
static inline int
_set_insert (yletcxt_t* cxt, const char* sym, int ty, yle_t* e) {
    if (styis (ty, STyper_thread)) return ylslu_insert (cxt->slut, sym, ty, e);
    else return ylgsym_insert (sym, ty, e);
}

static inline int
_set_description (yletcxt_t* cxt, const char* sym, int ty, const char* desc) {
    if (styis (ty, STyper_thread)) return ylslu_set_description (cxt->slut, sym, desc);
    else return  ylgsym_set_description (sym, desc);
}

/**
 * a is a yllist of the form ((u1 v1) ... (uN vN))
 * < additional constraints : x is atomic >
 * if x is one of u's, it changes the value of 'u'. If ylnot, it changes global lookup map.
 * @s:     key symbol
 * @val:   any S-expression - value
 * @a:     map list
 * @desc:  description of symbol
 * @attr:  symbol attribute.
 * @return: new value
 */
static yle_t*
_set(yletcxt_t* cxt, yle_t* s, yle_t* val, yle_t* a, const char* desc, int ty) {
    yle_t* r;
    if ( !ylais_type (s, ylaif_sym ())
        || ylnil () == s) {
        yllogE0 ("Only symbol can be set!\n");
        ylinterpret_undefined (YLErr_eval_undefined);
    }

    if (0 == *ylasym (s).sym) {
        yllogE0 ("empty symbol cannot be set!\n");
        ylinterpret_undefined (YLErr_eval_undefined);
    }

    ylassert (ylasym (s).sym);
    r = _list_find (s, a);
    if (r) {
        /* found it */
        ylassert (!yleis_atom (r));
        if (yleis_atom (ylcdr (r))) {
            yllogE0 ("unexpected set operation!\n");
            ylinterpret_undefined (YLErr_eval_undefined);
        } else  {
            ylassert (ylais_type (ylcar (r), ylaif_sym ()));
            ylasym (ylcar (r)).ty = ty;
            /* change value of local map yllist */
            ylpsetcdr(r, ylcons(val, ylnil()));
        }
        /* argument desc is ignored */
    } else {
        _set_insert (cxt, ylasym (s).sym, ty, val);
        /* NULL or strlen (desc) == 0 : only trailing 0 */
        if (!desc || !desc[0]) desc = NULL;
        _set_description (cxt, ylasym (s).sym, ty, desc);
    }
    return val;
}

yle_t*
ylset(yletcxt_t* cxt, yle_t* s, yle_t* val, yle_t* a, const char* desc, int ty) {
    return _set(cxt, s, val, a, desc, ty);
}

int
ylis_set (yletcxt_t* cxt, yle_t* a, const char* sym) {
    yle_t  etmp;
    int    temp;
    /* etmp is never cleaned. So, sym can be used directly here */
    ylaassign_sym(&etmp, (char*)sym);
    return NULL != _list_find (&etmp, a) ||
        NULL != ylslu_get (cxt->slut, &temp, sym) ||
        NULL != ylgsym_get (&temp, sym);
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
static const yle_t*
_assoc(yletcxt_t* cxt, int* ovty, yle_t* x, yle_t* y) {
    yle_t* r;
    if( !ylais_type(x, ylaif_sym()) ) {
        yllogE0("Only symbol can be associated!\n");
        ylinterpret_undefined(YLErr_eval_undefined);
    }

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
     *
     * !! IMPORTANT NOTE !!
     *    Current implementation DOESN"T clone any ATOM DATA! (see '_list_clone')
     *    So, chaning atom directly affects to global macro!!!
     *    (This is NOT BUG. It's implementation CONCEPT!)
     *
     */
    do {
        /*
         * Policy:
         *    Find in symbol hash first. If fails, try to evaluate as number!.
         *    (For fast symbol binding!)
         */
        r = _list_find(x, y);
        if (r) {
            /* Found! in local association list */
            ylassert (ylais_type (ylcar (r), ylaif_sym ()));
            *ovty = ylasym (ylcar (r)).ty;
            r = ylcadr (r);
            break;
        }

        /* Find in per-thread symbol table! */
        r = (yle_t*)ylslu_get(cxt->slut, ovty, ylasym(x).sym);
        if (r) break;

        /* At last find in global symbol table */
        r = (yle_t*)ylgsym_get(ovty, ylasym(x).sym);
    } while (0);

    if (r) return styis(*ovty, STymac)? _list_clone(r): r;

    /* check that this is numeric symbol */
    { /* Just Scope */
        /*
         * check whether this represents number or not
         * (string that representing number associated to 'double' type value )
         */
        char*   endp;
        double  d;
        d = strtod(ylasym(x).sym, &endp);
        if( 0 == *endp && ERANGE != errno ) {
            /* default is 0 */
            *ovty = 0;
            /* right coversion - let's assign double type atom*/
            return ylacreate_dbl(d);
        }

        yllogE1("symbol [%s] was not set!\n", ylasym(x).sym);
        ylinterpret_undefined(YLErr_eval_undefined);
    }

    return NULL; /* to make compiler happy */
}

/*=================================
 * Recursive S-functions - END
 *=================================*/

/*=================================
 * Universal S-functions - START
 *=================================*/


yle_t*
yleval(yletcxt_t* cxt, yle_t* e, yle_t* a) {
#define _cmp(x, e) !strcmp(#x, ylasym(e).sym)

    yle_t*       r = NULL;
    int          vty;
#ifdef CONFIG_DBG_EVAL
    unsigned int evid = _eval_id++; /* evaluation id for debugging */
#endif /* CONFIG_DBG_EVAL */

    /* Add evaluation stack -- for debugging */
    ylstk_push(cxt->evalstk, (void*)e);

    dbg_eval(yllogD2("[%d] eval In:\n"
                     "    %s\n", evid, ylechain_print(ylethread_buf(cxt), e));
             yllogD1("    =>%s\n", ylechain_print(ylethread_buf(cxt), a)););

    /*
     * 'e' and 'a' should be preserved during evaluation.
     * So, add reference count manually!
     * (especially, in case of a, some functions (ex let) may add new assoc. element
     *  in front of current 'a'.
     */
    ylmp_add_bb2(e, a);

    if( yleis_atom(e) ) {
        if(ylaif_sym() == ylaif(e)) {
            r = (yle_t*)_assoc(cxt, &vty, e, a);
            if (styis (vty, STymac)) r = yleval (cxt, r, a);
        } else {
            yllogE0("ERROR to evaluate atom(only symbol can be evaluated!.\n");
            ylinterpret_undefined(YLErr_eval_undefined);
        }
    } else if( yleis_atom(ylcar(e)) ) {
        yle_t*   car_e =ylcar(e);

        if(ylaif_sym() == ylaif(car_e) ) {
            r = (yle_t*)_assoc(cxt, &vty, car_e, a);
            if (styis (vty, STymac)) {
                /*
                 * This is macro symbol! replace target expression with symbol.
                 * And evaluate it with replaced value!
                 */
                r = yleval (cxt, ylcons (r, ylcdr (e)), a);
            } else {
                if (yleis_atom (r)) {
                    if (ylaif_sfunc () == ylaif (r)) {
                        /*
                         * This is 'sfunc' type symbol.
                         * parameters are not evaluated before being passed!
                         */
                        r = (*(ylanfunc (r).f)) (cxt, ylcdr (e), a);
                    } else if (ylaif_nfunc () == ylaif (r)) {
                        yle_t* param;
                        /*
                         * 'yleval' is called in 'ylevlis'.
                         * We should keep 'r' from GC!
                         */
                        ylmp_add_bb1 (r);
                        param = ylevlis (cxt, ylcdr (e), a);
                        ylmp_rm_bb1 (r);

                        /* preserving 'param' is prerequisite of calling native function! */
                        ylmp_add_bb1 (param);
                        r = (*(ylanfunc (r).f)) (cxt, param, a);
                        ylmp_rm_bb1 (param);
                    } else {
                        yllogE0 ("ERROR to evaluate. First element should be a function.\n");
                        ylinterpret_undefined (YLErr_eval_undefined);
                    }
                } else {
                    /* try to evaluate it again with evaluated expression! */
                    yle_t* param;
                    ylmp_add_bb1 (r);
                    param = ylevlis (cxt, ylcdr (e), a);
                    ylmp_rm_bb1 (r);
                    r = yleval (cxt, ylcons (r, param), a);
                }
            }
        } else {
            yllogE0("ERROR to evaluate. Only symbol and list can be evaluated!\n");
            ylinterpret_undefined(YLErr_eval_undefined);
        }
    } else {
        yle_t* ce = ylcaar(e);
        if (!ylais_type (ce, ylaif_sym ())) {
            yllogE0("Invalid expression to evaluate!\n"
                    "    (symbol ...) or ((<symbol> ...)) are allowed!!\n"
                    "    (((xxx))) is not allowed!\n");
            ylinterpret_undefined(YLErr_eval_undefined);
        }

        /*
         * Abbrev :
         *    - LAL : local association list
         *
         * Why 'flabel' is required
         * The main reason is 'performance to access global symbol'.
         * Script usually keeps some amount of function call stack during runtime.
         * So, Using inherited LAL in function means "size of LAL grows in proportion to depth of call-stack".
         * And this leads performance-inefficency to access global symbol (usually, function symbol, reserved words etc).
         * Functions from typical LISP - ex. label, lambda - uses inherited LAL.
         * So, special new symbol 'flabel' is adopted.
         * This is usually used to implement 'defun' function.
         * (Actually, 'defun' is not the one that is described in LISP's mathemetical model.
         *  But it is very popular and important.)
         */
        if (_cmp (flabel, ce)) {
            /*
             * exactly same with label except for new label is added to LAL as macro symbol (NOT normal one)
             */
            /*
             * (flabel name arg exp) p1 p2 ...
             * car     : flabel expression list
             * cdr     : parameter list
             *
             * In flabel expression list
             * cadr   : label name
             * caddr  : argument list
             * cadddr : expression
             */
            yle_t*    fl  = ylcar (e);    /* flabel */
            yle_t*    fln = ylcadr (fl);  /* flabel name */
            yle_t*    fla = ylcaddr (fl); /* flabel arguement list */
            yle_t*    fle = ylcadddr (fl);/* flabel expression */
            yle_t*    flp = ylcdr (e);    /* parameter of flabel */
            yle_t*    param_assoc;
            if (!ylais_type (fln, ylaif_sym ())) {
                yllogE0("flabel name should be symbol!\n");
                ylinterpret_undefined(YLErr_eval_undefined);
            }
            /* set symbol type as macro */
            styset (ylasym (fln).ty, STymac);
            param_assoc = ylpair (fla, ylevlis (cxt, flp, a));
            r = yleval (cxt, fle,
                        ylcons (yllist (fln, fl), param_assoc));
        } else if (_cmp (lambda, ce)) {
            /*
             * eq [caar [e] -> LAMBDA] -> eval [caddar [e]; append [pair [cadar [e]; evlis [cdr [e]; a]]; a]]
             * ylpair and ylappend don't call yleval in it. So, we don't need to preserve base blocks explicitly
             *
             * car     : lambda expression list
             * cdr     : parameter list
             * cadar   : arguement list
             * caddar  : lambda body
             */
            r = yleval(cxt, ylcaddar(e), ylappend(ylpair(ylcadar(e), ylevlis(cxt, ylcdr(e), a)), a));
        } else if(_cmp(mlambda, ce)) {
            if( yleis_nil(ylcadar(e)) && !yleis_nil(ylcaddar(e)) ) {
                /*
                 * !!! Special usage of mlambda !!!
                 * If parameter is nil, than, arguments are appended to the body!
                 */

                /*
                 * At below, we should change cdr value.
                 * In case of MT evaluation, this may cause racing condition.
                 * Use list-copied one!
                 */
                yle_t* exp = _list_clone(ylcaddar(e));
                yle_t* we = exp;
                while(!yleis_nil(ylcdr(we))) { we = ylcdr(we); }

                /* connect body with argument */
                ylpsetcdr(we, ylcdr(e));
                r = yleval(cxt, exp, a);
                /* we don't need to restore. it was just copied one. */
                /* ylpsetcdr(we, ylnil()); */
            } else {
                /*
                 * NOTE!!
                 *     expression itself may be changed in '_mreplace'.
                 *     To reserve original expression, we need to use cloned one.
                 *  --> Now expression is preserved!
                 */
                yle_t* exp = _list_clone(ylcaddar(e));
                if(0 > _mreplace(exp, ylappend(ylpair(ylcadar(e), ylcdr(e)), ylnil()))) {
                    yllogE0("Fail to mreplace!!\n");
                    ylinterpret_undefined(YLErr_eval_undefined);
                } else {
                    r = yleval(cxt, exp, a);
                }
            }
        } else if (_cmp(label, ce)) {
            /*
             * car     : label expression list
             * cdr     : parameter list
             * cadar   : label name
             * caddar  : lambda expression list
             */
            if (!ylais_type (ylcadar (e), ylaif_sym ())) {
                yllogE0("label name shoud be symbol!\n");
                ylinterpret_undefined(YLErr_eval_undefined);
            }
            /* eq [caar [e]; LABEL] -> eval [cons [caddar [e]; cdr [e]]; cons [list [cadar [e]; car [e]]; a]] */
            r = yleval(cxt, ylcons(ylcaddar(e), ylcdr(e)), ylcons(yllist(ylcadar(e), ylcar(e)), a));
        } else {
            /* my extention */
            r = yleval(cxt, ylcons(yleval(cxt, ylcar(e), a), ylcdr(e)), a);
            /* ylinterpret_undefined(YLErr_eval_undefined); */
        }
    }

#undef _cmp

    if(r) {

        dbg_eval(yllogD2("[%d] eval Out:\n"
                         "    %s\n", evid, ylechain_print(ylethread_buf(cxt), r)););
        ylmp_rm_bb2(e, a);

        /*
         * pop evaluation stack -- for debugging
         * (Push and Pop should be in one 'evaluatin-step' -- before unlock inteval_mutex)
         */
        ylstk_pop(cxt->evalstk);

        /* Returned value should be pretected from GC. */
        ylmp_add_bb1(r);

        /*
         * This is right place to interrupt evaluation!
         * - Evaulation is pushed to debugging stack.
         * - Parameter's are kept from GC.
         * Now interrupting is acceptable!
         * Let's notify memory module that evaluation thread is in safe state!
         * (I need to find more places to put this function!.
         *   -> make SW stable! And then find more places!
         *   [ex. Starting point of evaluation!])
         */
        ylmt_notify_safe(cxt);
        ylmt_notify_unsafe(cxt);

        /* Pass responsibility about preserving return value to the caller! */
        ylmp_rm_bb1(r);

        return r;
    } else {
        yllogE0("NULL return! is it possible!\n");
        ylinterpret_undefined(YLErr_eval_undefined);
    }
    ylassert(0);
    return NULL; /* to make compiler happy */
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
ylapply(yletcxt_t* cxt, yle_t* f, yle_t* args, yle_t* a) {
    return yleval(cxt, ylcons(f, _appq(args)), a);
}

/*=================================
 * Universal S-functions - END
 *=================================*/



ylerr_t
ylsfunc_init() {
    return YLOk;
}
