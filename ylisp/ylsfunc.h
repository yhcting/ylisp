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



/**********************************************************
 * NOTE!
 *    S-Functions that are not suitable for static function
 *      are not included. (ex. 'eq', 'set', 'assoc' and 'eval')
 *    (Static function in header means "importing directly outside of module".)
 *
 *    The reasons is,
 *      - These have high possibility to be changed or improved
 *          in future as atom type is extended.
 *        So, importing these directly outside of module may cause future maintenance issue.
 **********************************************************/

#ifndef ___YLSFUNc_h___
#define ___YLSFUNc_h___

#include "yldev.h"

extern int
ylis_set (yletcxt_t* cxt, yle_t* a, const char* sym);

/* Non-static S-Functions */
extern const yle_t*
yleq(const yle_t* e1, const yle_t* e2);

/**
 * In this function, GC may be triggered!
 * So, when use this function,
 *  caller SHOULD PRESERVE base blocks that should be protected from GC!
 *  by using 'ylmp_add_bbN(...)'.
 * This function is SENSITIVE AND DANGEROUS.
 * Take your attention for using this function!
 *
 * GC Protection required to caller
 *
 * *** NOTE ***
 * Return value of 'yleval' is not protected from GC.
 * So, if return value should be preserved, it is explicitly protected from GC.
 * Here is a example case.
 *    func(yleval(x, a), yleval(y, b)); <= bad example.
 * First arguement - return value of 'yleval(x, a)'- may be GCed at 'yleval(y, b)'!!
 * So, in this case, return value of 'yleval(x, a)' should be protected explicitly!
 *
 */
extern yle_t*
yleval(yletcxt_t* cxt, yle_t* e, yle_t* a);

/*=================================
 * Elementary S-functions - START
 *=================================*/
static inline yle_t*
ylcar(const yle_t* e) {
    if(!yleis_atom(e)) {
        return ylpcar(e);
    }
    yllog((YLLogE, "'car' is available only on pair\n"));
    ylinterpret_undefined(YLErr_eval_undefined);
    return NULL; /* to make compiler happy */
}

static inline yle_t*
ylcdr(const yle_t* e) {
    if(!yleis_atom(e)) {
        return ylpcdr(e);
    }
    yllog((YLLogE, "'cdr' is available only on pair\n"));
    ylinterpret_undefined(YLErr_eval_undefined);
    return NULL; /* to make compiler happy */
}

#define ylcaar(e) ylcar(ylcar(e))
#define ylcadr(e) ylcar(ylcdr(e))
#define ylcdar(e) ylcdr(ylcar(e))
#define ylcddr(e) ylcdr(ylcdr(e))

#define ylcaaar(e) ylcar(ylcaar(e))
#define ylcaadr(e) ylcar(ylcadr(e))
#define ylcadar(e) ylcar(ylcdar(e))
#define ylcaddr(e) ylcar(ylcddr(e))
#define ylcdaar(e) ylcdr(ylcaar(e))
#define ylcdadr(e) ylcdr(ylcadr(e))
#define ylcddar(e) ylcdr(ylcdar(e))
#define ylcdddr(e) ylcdr(ylcddr(e))

#define ylcaaaar(e) ylcar(ylcaaar(e))
#define ylcaaadr(e) ylcar(ylcaadr(e))
#define ylcaadar(e) ylcar(ylcadar(e))
#define ylcaaddr(e) ylcar(ylcaddr(e))
#define ylcadaar(e) ylcar(ylcdaar(e))
#define ylcadadr(e) ylcar(ylcdadr(e))
#define ylcaddar(e) ylcar(ylcddar(e))
#define ylcadddr(e) ylcar(ylcdddr(e))
#define ylcdaaar(e) ylcdr(ylcaaar(e))
#define ylcdaadr(e) ylcdr(ylcaadr(e))
#define ylcdadar(e) ylcdr(ylcadar(e))
#define ylcdaddr(e) ylcdr(ylcaddr(e))
#define ylcddaar(e) ylcdr(ylcdaar(e))
#define ylcddadr(e) ylcdr(ylcdadr(e))
#define ylcdddar(e) ylcdr(ylcddar(e))
#define ylcddddr(e) ylcdr(ylcdddr(e))



static inline yle_t*
ylcons(yle_t* car, yle_t* cdr) {
    return ylpcreate(car, cdr);
}

static inline const yle_t*
ylatom(const yle_t* e) {
    /*NIL is ylatom too. */
    return yleis_atom(e)? ylt(): ylnil();
}


static inline yle_t*
yland(const yle_t* e1, const yle_t* e2) {
    return (!yleis_nil(e1) && !yleis_nil(e2))? ylt(): ylnil();
}

static inline yle_t*
ylor(const yle_t* e1, const yle_t* e2) {
    return (yleis_nil(e1) && yleis_nil(e2))? ylnil(): ylt();
}

static inline yle_t*
ylnot(const yle_t* e1) {
    return yleis_nil(e1)? ylt(): ylnil();
}


/*=================================
 * Elementary S-functions - END
 *=================================*/

/*=================================
 * Recursive S-functions - START
 *=================================*/
static inline yle_t*
yllist(yle_t* e1, yle_t* e2)  {
    return ylcons(e1, ylcons(e2, ylnil()));
}

/**
 * null[x] = atom[x] AND eq[x;NIL]
 */
static inline yle_t*
ylnull(yle_t* e) {
    return (yleis_atom(e) && yleis_nil(e))? ylt(): ylnil();
}

/**
 * ff[x] = [atom [x] -> x; T -> ff [car [x]]]
 */
static inline yle_t*
ylff(yle_t* e) {
    return yleis_atom(e)? e: ylff(ylcar(e));
}

/**
 * subst [x; y; z] = [atom [z] -> [eq [z; y] -> x; T -> z];
 *    T -> cons[ subst[x; y; car [z]]; subst [x; y; cdr [z]]]]
 */
static inline yle_t*
ylsubst(yle_t* x, yle_t* y, yle_t* z) {
    if(yleis_atom(y)) {
        if(yleis_atom(z)) {
            if( yleis_true(yleq(z, y)) ) { return x; }
            else { return z; }
        } else {
            return ylcons(ylsubst(x, y, ylcar(z)), ylsubst(x, y, ylcdr(z)));
        }
    }
    yllogE0("subst : Should not reach here!\n");
    ylinterpret_undefined(YLErr_eval_undefined);
    return NULL; /* to make compiler be happy */
}


#if 0 /* 'yleq' is applicable to pair, too. So, yleq is just same with ylequal */
/**
 * equal [x; y] = [atom [x] && atom [y] && eq[x; y]]
 *                    || [!atom[x] && !atom[y] && equal [car [x]; car [y]] && equal [cdr [x]; cdr [y]]]
 */
static inline yle_t*
ylequal(yle_t* x, yle_t* y) {
    return
    ylb2e( ( yle2b(ylatom(x)) && yle2b(ylatom(y)) && yle2b(yleq(x,y)) )
           || ( !yle2b(ylatom(x))
                && !yle2b(ylatom(y))
                && yle2b(ylequal(ylcar(x), ylcar(y)))
                && yle2b(ylequal(ylcdr(x), ylcdr(y))) ) );

}
#endif

/**
 * append[x; y] = [null [x] -> y; T -> cons [car [x]; append [cdr [x]; y]]
 */
static inline yle_t*
ylappend(yle_t* x, yle_t* y) {
    return (yleis_nil(x))? y: ylcons(ylcar(x), ylappend(ylcdr(x), y));
}

/**
 * among[x; y] = !null(y) && [equal [x; car[y]] || among [x; cdr[y]]]
 */
static inline yle_t*
ylamong(yle_t* x, yle_t* y) {
    return ylb2e( !yle2b(ylnull(y)) && ( yle2b(yleq(x, ylcar(y))) || yle2b(ylamong(x, ylcdr(y))) ) );
}

/**
 * pair [x; y] = [null [x] && null [y] -> NIL;
 *               !atom [x] && !atom [y] -> cons [list [car [x]; car[y]]; pair [cdr [x]; cdr [y]]]
 */
static inline yle_t*
ylpair(yle_t* x, yle_t* y) {
    if( yleis_nil(x) && yleis_nil(y) ) {
        return ylnil();
    } else if( !yleis_atom(x) && !yleis_atom(y) ) {
        return ylcons(yllist(ylcar(x), ylcar(y)), ylpair(ylcdr(x), ylcdr(y)));
    } else {
        yllogE0("Fail to map parameter!\n");
        ylinterpret_undefined(YLErr_eval_undefined);
    }
    return NULL; /* to make compiler happy */
}


/**
 * : __sub2 / sublis
 * x is assumed to have the form of a list of pairs ((u1 v1) ... (uN vN)), where u's are atomic.
 */

/**
 * sub2 [x; z] = [null [x] -> z; eq [caar [x]; z] -> cadar [x]; T -> sub2 [cdr [x]; z]]
 */
static inline yle_t*
__ylsub2(yle_t* x, yle_t* z) {
    if( yleis_nil(x) ) {
        return z;
    } else if( yleis_true(yleq(ylcaar(x), z)) ) {
        return ylcadar(x);
    } else {
        return __ylsub2(ylcdr(x), z);
    }
}

/**
 * sublis [x; y] = [atom [y] -> sub2 [x; y]; T -> cons [sublis [x; car [y]]; sublis [x; cdr [y]]]
 */
static inline yle_t*
ylsublis(yle_t* x, yle_t* y) {
    return yleis_atom(y)? __ylsub2(x, y):
                ylcons(ylsublis(x, ylcar(y)), ylsublis(x, ylcdr(y)));
}

/*
 * GC Protection required to caller.
 */
static inline yle_t*
ylevlis(yletcxt_t* cxt, yle_t* m, yle_t* a) {
    if(yleis_nil(m)) { return ylnil(); }
    else {
        yle_t* r; /* return value */
        yle_t* p = yleval(cxt, ylcar(m), a);
        /* p should be preserved from GC */
        ylmp_add_bb1(p);
        r = ylcons(p, ylevlis(cxt, ylcdr(m), a));
        /* Now p is not base block anymore */
        ylmp_rm_bb1(p);
        return r;
    }
}


/*=================================
 * Recursive S-functions - END
 *=================================*/

#endif /* ___YLSFUNc_h___ */
