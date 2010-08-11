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
 * Utilities for CNF Dev.
 * This is optional.
 * To avoid symbol conflict, this file includes 'static function' and 'macro' only.
 */

#ifndef ___YLDEVUt_h___
#define ___YLDEVUt_h___

#include "yldev.h"

#ifndef NULL
#   define NULL ((void*)0)
#endif

#ifndef TRUE
#   define TRUE 1
#endif

#ifndef FALSE
#   define FALSE 0
#endif

#define yleset_type(e, ty)      do{ (e)->t = (ty); } while(0)
#define yletype(e)              ((e)->t)
#define ylercnt(e)              ((e)->r)  /* reference count */
#define yleis_atom(e)           ((e)->t & YLEAtom)
#define ylatype(e)              ((e)->t & YLAtype_mask)

#define yleset_reachable(e)     ((e)->t |= YLEReachable)
#define yleclear_reachable(e)   ((e)->t &= ~YLEReachable)
#define yleis_reachable(e)      ((e)->t & YLEReachable)

/* macros for easy accessing */
#define yleatom(e)  ((e)->u.a)
#define ylasym(e)   ((e)->u.a.u.sym)
#define ylanfunc(e) ((e)->u.a.u.nfunc)
#define yladbl(e)   ((e)->u.a.u.dbl)
#define ylabin(e)   ((e)->u.a.u.bin)

#define ylpcar(e)   ((e)->u.p.car)
#define ylpcdr(e)   ((e)->u.p.cdr)

#define yleis_nil(e)             (ylnil() == (e))

/*
 * This function SHOULD NOT BE CALLED DIRECTLY
 */
extern void
__ylmp_release_block(yle_t* e);

static inline void
yleunref(yle_t* e) {
    ylercnt(e)--;
    if(0 == ylercnt(e)) {
        __ylmp_release_block(e);
    }
}

static inline void
yleref(yle_t* e) {
    ylercnt(e)++;
}

static inline void
ylpsetcar(yle_t* e, yle_t* exp) {
    yle_t*  sv = e->u.p.car;
    e->u.p.car = exp;
    yleref(exp);
    if(sv) { yleunref(sv); }
}

static inline void
ylpsetcdr(yle_t* e, yle_t* exp) {
    yle_t*  sv = e->u.p.cdr;
    e->u.p.cdr = exp;
    yleref(exp);
    if(sv) { yleunref(sv); }
}

/**
 * This is used very often!
 */
static inline int
ylais_type(const yle_t* e, char ty) {
    return (yleis_atom(e) && ylatype(e) == ty );
}

/*========================
 * Default value of all attributes SHOULD BE ZERO.
 *========================*/
/* 
 * This should be used to represent TRUE boolean value. 
 * Actually, "not nil" is true. But, T need to be shown for true!
 * (To improve performance, we may use global variable instead of function)
 * This is referened very frequently!
 * ylt() : TRUE sexp - be careful to using this
 * ylnil() : NIL sexp
 */
extern const yle_t* const ylg_predefined_true;
extern const yle_t* const ylg_predefined_nil;
extern const yle_t* const ylg_predefined_quote;
#define ylt()    ((yle_t*)ylg_predefined_true)
#define ylnil()  ((yle_t*)ylg_predefined_nil)
#define ylq()    ((yle_t*)ylg_predefined_quote)

/**
 * DO NOT USE "yltrue() == xxxx" if you don't know what you are doing!
 * Instead of this, use "ylfalse() != xxxx"
 */
#define yltrue()     ylt()
#define ylfalse()    ylnil()

#define yleis_true(e)     (ylfalse() != (e))
#define yleis_false(e)    (ylfalse() == (e))
/**
 * notify that ylisp reached to the ylinterpret_undefined state.
 * @r: reason
 */
extern void ylinterpret_undefined(int reason);

#define __yllogp(lv, pre, x)                    \
    do {                                        \
        if(ylsysv()->loglv <= (lv)) {           \
            ylprint((pre));                     \
            ylprint(x);                         \
        }                                       \
    } while(0)

#define yllogp(lv, x) __yllogp(lv, "", x)

#define yllogV(x) __yllogp(YLLog_verb,     "[V]: ", x);
#define yllogD(x) __yllogp(YLLog_dev,      "[D]: ", x);
#define yllogI(x) __yllogp(YLLog_info,     "[I]: ", x);
#define yllogW(x) __yllogp(YLLog_warn,     "[W]: ", x);
#define yllogO(x) __yllogp(YLLog_output,   "[O]: ", x);
#define yllogE(x) __yllogp(YLLog_err,      "[E]: ", x);


/*========================
 * To define native functions.
 *========================*/
/* make native function name : lnf (Lisp Native Funcion) */
#define YLNFN(n)    __LNF__##n##__

#define YLDECLNF(n) yle_t* YLNFN(n)(yle_t* e, yle_t* a)
/*
 * Native Function EXPORT 
 * @p: number of parameter that this function expected.
 *     <0 means variable number.
 */
#define YLDEFNF(n, minp, maxp)                                          \
    YLDECLNF(n) {                                                       \
    int pcsz; /* Parameter Chain SiZe */                                \
    { /* jusg scope */                                                  \
        pcsz = ylchain_size(e);                                         \
        if((minp) > pcsz || pcsz > (maxp)) {                            \
            yllogE(("<!%s!> invalid number of parameter\n", #n));       \
            ylinterpret_undefined(YLErr_func_invalid_param);            \
        }                                                               \
    }                                                                   \
    do

/* This should be ylpair with YLDEFNF */                 
#define YLENDNF(n) while(0); }

#define ylcheck_chain_atom_type1(nAME, eXP, tY)                         \
    do {                                                                \
        yle_t* _E = (eXP);                                              \
        while( !yleis_nil(_E) ) {                                       \
            if(!ylais_type(ylcar(_E), tY)) {                            \
                yllogE(("<!%s!> invalid parameter type\n", #nAME));     \
                ylinterpret_undefined(YLErr_func_invalid_param);        \
            }                                                           \
            _E = ylcdr(_E);                                             \
        }                                                               \
    } while(0)

#define ylcheck_chain_atom_type2(nAME, eXP, tY1, tY2)                   \
    do {                                                                \
        int _aty;                                                       \
        yle_t* _E = (eXP);                                              \
        if(!yleis_nil(_E)) {                                            \
            _aty = ylatype(ylcar(_E));                                  \
            if( tY1 != _aty && tY2 != _aty ) {                          \
                yllogE(("<!%s!> invalid parameter type\n", #nAME));     \
                ylinterpret_undefined(YLErr_func_invalid_param);        \
            }                                                           \
            _E = ylcdr(_E);                                             \
            while( !yleis_nil(_E) ) {                                   \
                if( !ylais_type(ylcar(_E), _aty) ) {                    \
                    yllogE(("<!%s!> invalid parameter type\n", #nAME)); \
                    ylinterpret_undefined(YLErr_func_invalid_param);    \
                }                                                       \
                _E = ylcdr(_E);                                         \
            }                                                           \
        }                                                               \
    } while(0)



static inline void
ylpassign(yle_t* e, yle_t* car, yle_t* cdr) {
    yleset_type(e, YLEPair);
    ylpsetcar(e, car);
    ylpsetcdr(e, cdr);
}

/**
 * @sym: [in] responsibility for memory handling is passed to @se.
 */
static inline void
ylaassign_sym(yle_t* e, char* sym) {
    yleset_type(e, YLEAtom | YLASymbol);
    ylasym(e).sym = sym;
}

static inline void
ylaassign_dbl(yle_t* e, double d) {
    yleset_type(e, YLEAtom | YLADouble);
    yladbl(e) = d;
}

static inline void
ylaassign_bin(yle_t* e, char* data, unsigned int len) {
    yleset_type(e, YLEAtom | YLABinary);
    ylabin(e).d = data;
    ylabin(e).sz = len;
}

/*=====================================
 * Useful S-Functions
 *=====================================*/









/*=====================================
 * Additional Extern functions for using utility functions
 *=====================================*/

/**
 * NOTE! : If you don't know what you are doing, DO NOT USE THIS!
 * Store current memory pool state.
 */
extern void
ylmp_push();

/**
 * NOTE! : If you don't know what you are doing, DO NOT USE THIS!
 * GC mem-blocks that are newly allocated from last 'push'
 */
extern void
ylmp_pop();


extern yle_t*
yleclone(const yle_t* e);

extern yle_t*
yleclone_chain(const yle_t* e);


extern int
ylchain_size(const yle_t* e);

/* 
 * get print string.
 * returned memory SHOULD NOT be freed.
 * (This function uses static buffer, So please keep this in mind!)
 * -> Wrong usage : printf("%s / %s", yleprint(e1), yleprint(e2));
 */
extern const char*
yleprint(const yle_t* e);


#endif /* ___YLDEVUt_h___ */
