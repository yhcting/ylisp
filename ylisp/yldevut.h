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
#define yleatom(e)      ((e)->u.a)
#define ylasym(e)       ((e)->u.a.u.sym)
#define ylanfunc(e)     ((e)->u.a.u.nfunc)
#define yladbl(e)       ((e)->u.a.u.dbl)
#define ylabin(e)       ((e)->u.a.u.bin)

#define ylpcar(e)       ((e)->u.p.car)
#define ylpcdr(e)       ((e)->u.p.cdr)

#define yleis_nil(e)    (ylnil() == (e))

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
#define yltrue()          ylt()
#define ylfalse()         ylnil()

#define yleis_true(e)     (ylfalse() != (e))
#define yleis_false(e)    (ylfalse() == (e))
/**
 * notify that ylisp reached to the ylinterpret_undefined state.
 * @r: reason
 */
extern void ylinterpret_undefined(int reason);


/* ------------------------------
 * Macros for Log -- START
 * ------------------------------*/
#define yllogV0(fmt)                     yllog((YLLogV, fmt))
#define yllogV1(fmt, p0)                 yllog((YLLogV, fmt, p0))
#define yllogV2(fmt, p0, p1)             yllog((YLLogV, fmt, p0, p1))
#define yllogV3(fmt, p0, p1, p2)         yllog((YLLogV, fmt, p0, p1, p2))
#define yllogV4(fmt, p0, p1, p2, p3)     yllog((YLLogV, fmt, p0, p1, p2, p3))
#define yllogV5(fmt, p0, p1, p2, p3, p4) yllog((YLLogV, fmt, p0, p1, p2, p3, p4))
#define yllogV6(fmt, p0, p1, p2, p3, p4, p5)            \
    yllog((YLLogV, fmt, p0, p1, p2, p3, p4, p5))
#define yllogV7(fmt, p0, p1, p2, p3, p4, p5, p6)        \
    yllog((YLLogV, fmt, p0, p1, p2, p3, p4, p5, p6))
#define yllogV8(fmt, p0, p1, p2, p3, p4, p5, p6, p7)            \
    yllog((YLLogV, fmt, p0, p1, p2, p3, p4, p5, p6, p7))
#define yllogV9(fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8)        \
    yllog((YLLogV, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8))

#define yllogD0(fmt)                     yllog((YLLogD, fmt))
#define yllogD1(fmt, p0)                 yllog((YLLogD, fmt, p0))
#define yllogD2(fmt, p0, p1)             yllog((YLLogD, fmt, p0, p1))
#define yllogD3(fmt, p0, p1, p2)         yllog((YLLogD, fmt, p0, p1, p2))
#define yllogD4(fmt, p0, p1, p2, p3)     yllog((YLLogD, fmt, p0, p1, p2, p3))
#define yllogD5(fmt, p0, p1, p2, p3, p4) yllog((YLLogD, fmt, p0, p1, p2, p3, p4))
#define yllogD6(fmt, p0, p1, p2, p3, p4, p5)            \
    yllog((YLLogD, fmt, p0, p1, p2, p3, p4, p5))
#define yllogD7(fmt, p0, p1, p2, p3, p4, p5, p6)        \
    yllog((YLLogD, fmt, p0, p1, p2, p3, p4, p5, p6))
#define yllogD8(fmt, p0, p1, p2, p3, p4, p5, p6, p7)            \
    yllog((YLLogD, fmt, p0, p1, p2, p3, p4, p5, p6, p7))
#define yllogD9(fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8)        \
    yllog((YLLogD, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8))

#define yllogI0(fmt)                     yllog((YLLogI, fmt))
#define yllogI1(fmt, p0)                 yllog((YLLogI, fmt, p0))
#define yllogI2(fmt, p0, p1)             yllog((YLLogI, fmt, p0, p1))
#define yllogI3(fmt, p0, p1, p2)         yllog((YLLogI, fmt, p0, p1, p2))
#define yllogI4(fmt, p0, p1, p2, p3)     yllog((YLLogI, fmt, p0, p1, p2, p3))
#define yllogI5(fmt, p0, p1, p2, p3, p4) yllog((YLLogI, fmt, p0, p1, p2, p3, p4))
#define yllogI6(fmt, p0, p1, p2, p3, p4, p5)            \
    yllog((YLLogI, fmt, p0, p1, p2, p3, p4, p5))
#define yllogI7(fmt, p0, p1, p2, p3, p4, p5, p6)        \
    yllog((YLLogI, fmt, p0, p1, p2, p3, p4, p5, p6))
#define yllogI8(fmt, p0, p1, p2, p3, p4, p5, p6, p7)            \
    yllog((YLLogI, fmt, p0, p1, p2, p3, p4, p5, p6, p7))
#define yllogI9(fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8)        \
    yllog((YLLogI, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8))

#define yllogW0(fmt)                     yllog((YLLogW, fmt))
#define yllogW1(fmt, p0)                 yllog((YLLogW, fmt, p0))
#define yllogW2(fmt, p0, p1)             yllog((YLLogW, fmt, p0, p1))
#define yllogW3(fmt, p0, p1, p2)         yllog((YLLogW, fmt, p0, p1, p2))
#define yllogW4(fmt, p0, p1, p2, p3)     yllog((YLLogW, fmt, p0, p1, p2, p3))
#define yllogW5(fmt, p0, p1, p2, p3, p4) yllog((YLLogW, fmt, p0, p1, p2, p3, p4))
#define yllogW6(fmt, p0, p1, p2, p3, p4, p5)            \
    yllog((YLLogW, fmt, p0, p1, p2, p3, p4, p5))
#define yllogW7(fmt, p0, p1, p2, p3, p4, p5, p6)        \
    yllog((YLLogW, fmt, p0, p1, p2, p3, p4, p5, p6))
#define yllogW8(fmt, p0, p1, p2, p3, p4, p5, p6, p7)            \
    yllog((YLLogW, fmt, p0, p1, p2, p3, p4, p5, p6, p7))
#define yllogW9(fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8)        \
    yllog((YLLogW, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8))

#define yllogE0(fmt)                     yllog((YLLogE, fmt))
#define yllogE1(fmt, p0)                 yllog((YLLogE, fmt, p0))
#define yllogE2(fmt, p0, p1)             yllog((YLLogE, fmt, p0, p1))
#define yllogE3(fmt, p0, p1, p2)         yllog((YLLogE, fmt, p0, p1, p2))
#define yllogE4(fmt, p0, p1, p2, p3)     yllog((YLLogE, fmt, p0, p1, p2, p3))
#define yllogE5(fmt, p0, p1, p2, p3, p4) yllog((YLLogE, fmt, p0, p1, p2, p3, p4))
#define yllogE6(fmt, p0, p1, p2, p3, p4, p5)            \
    yllog((YLLogE, fmt, p0, p1, p2, p3, p4, p5))
#define yllogE7(fmt, p0, p1, p2, p3, p4, p5, p6)        \
    yllog((YLLogE, fmt, p0, p1, p2, p3, p4, p5, p6))
#define yllogE8(fmt, p0, p1, p2, p3, p4, p5, p6, p7)            \
    yllog((YLLogE, fmt, p0, p1, p2, p3, p4, p5, p6, p7))
#define yllogE9(fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8)        \
    yllog((YLLogE, fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8))

/* ------------------------------
 * Macros for Log -- END
 * ------------------------------*/

/* ------------------------------
 * Macros for NF Log -- START
 *    'nfname' is predefined string variable of native function name.
 *    This log macros automatically add 'nfname' as a prefix of log!
 * ------------------------------*/

#define ylnflogV0(fmt)                     yllog((YLLogV, "<!%s!> " fmt, __nFNAME))
#define ylnflogV1(fmt, p0)                 yllog((YLLogV, "<!%s!> " fmt, __nFNAME, p0))
#define ylnflogV2(fmt, p0, p1)             yllog((YLLogV, "<!%s!> " fmt, __nFNAME, p0, p1))
#define ylnflogV3(fmt, p0, p1, p2)         yllog((YLLogV, "<!%s!> " fmt, __nFNAME, p0, p1, p2))
#define ylnflogV4(fmt, p0, p1, p2, p3)     yllog((YLLogV, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3))
#define ylnflogV5(fmt, p0, p1, p2, p3, p4) yllog((YLLogV, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4))
#define ylnflogV6(fmt, p0, p1, p2, p3, p4, p5)            \
    yllog((YLLogV, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5))
#define ylnflogV7(fmt, p0, p1, p2, p3, p4, p5, p6)        \
    yllog((YLLogV, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6))
#define ylnflogV8(fmt, p0, p1, p2, p3, p4, p5, p6, p7)            \
    yllog((YLLogV, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6, p7))
#define ylnflogV9(fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8)        \
    yllog((YLLogV, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6, p7, p8))

#define ylnflogD0(fmt)                     yllog((YLLogD, "<!%s!> " fmt, __nFNAME))
#define ylnflogD1(fmt, p0)                 yllog((YLLogD, "<!%s!> " fmt, __nFNAME, p0))
#define ylnflogD2(fmt, p0, p1)             yllog((YLLogD, "<!%s!> " fmt, __nFNAME, p0, p1))
#define ylnflogD3(fmt, p0, p1, p2)         yllog((YLLogD, "<!%s!> " fmt, __nFNAME, p0, p1, p2))
#define ylnflogD4(fmt, p0, p1, p2, p3)     yllog((YLLogD, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3))
#define ylnflogD5(fmt, p0, p1, p2, p3, p4) yllog((YLLogD, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4))
#define ylnflogD6(fmt, p0, p1, p2, p3, p4, p5)            \
    yllog((YLLogD, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5))
#define ylnflogD7(fmt, p0, p1, p2, p3, p4, p5, p6)        \
    yllog((YLLogD, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6))
#define ylnflogD8(fmt, p0, p1, p2, p3, p4, p5, p6, p7)            \
    yllog((YLLogD, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6, p7))
#define ylnflogD9(fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8)        \
    yllog((YLLogD, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6, p7, p8))

#define ylnflogI0(fmt)                     yllog((YLLogI, "<!%s!> " fmt, __nFNAME))
#define ylnflogI1(fmt, p0)                 yllog((YLLogI, "<!%s!> " fmt, __nFNAME, p0))
#define ylnflogI2(fmt, p0, p1)             yllog((YLLogI, "<!%s!> " fmt, __nFNAME, p0, p1))
#define ylnflogI3(fmt, p0, p1, p2)         yllog((YLLogI, "<!%s!> " fmt, __nFNAME, p0, p1, p2))
#define ylnflogI4(fmt, p0, p1, p2, p3)     yllog((YLLogI, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3))
#define ylnflogI5(fmt, p0, p1, p2, p3, p4) yllog((YLLogI, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4))
#define ylnflogI6(fmt, p0, p1, p2, p3, p4, p5)            \
    yllog((YLLogI, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5))
#define ylnflogI7(fmt, p0, p1, p2, p3, p4, p5, p6)        \
    yllog((YLLogI, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6))
#define ylnflogI8(fmt, p0, p1, p2, p3, p4, p5, p6, p7)            \
    yllog((YLLogI, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6, p7))
#define ylnflogI9(fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8)        \
    yllog((YLLogI, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6, p7, p8))

#define ylnflogW0(fmt)                     yllog((YLLogW, "<!%s!> " fmt, __nFNAME))
#define ylnflogW1(fmt, p0)                 yllog((YLLogW, "<!%s!> " fmt, __nFNAME, p0))
#define ylnflogW2(fmt, p0, p1)             yllog((YLLogW, "<!%s!> " fmt, __nFNAME, p0, p1))
#define ylnflogW3(fmt, p0, p1, p2)         yllog((YLLogW, "<!%s!> " fmt, __nFNAME, p0, p1, p2))
#define ylnflogW4(fmt, p0, p1, p2, p3)     yllog((YLLogW, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3))
#define ylnflogW5(fmt, p0, p1, p2, p3, p4) yllog((YLLogW, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4))
#define ylnflogW6(fmt, p0, p1, p2, p3, p4, p5)            \
    yllog((YLLogW, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5))
#define ylnflogW7(fmt, p0, p1, p2, p3, p4, p5, p6)        \
    yllog((YLLogW, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6))
#define ylnflogW8(fmt, p0, p1, p2, p3, p4, p5, p6, p7)            \
    yllog((YLLogW, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6, p7))
#define ylnflogW9(fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8)        \
    yllog((YLLogW, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6, p7, p8))

#define ylnflogE0(fmt)                     yllog((YLLogE, "<!%s!> " fmt, __nFNAME))
#define ylnflogE1(fmt, p0)                 yllog((YLLogE, "<!%s!> " fmt, __nFNAME, p0))
#define ylnflogE2(fmt, p0, p1)             yllog((YLLogE, "<!%s!> " fmt, __nFNAME, p0, p1))
#define ylnflogE3(fmt, p0, p1, p2)         yllog((YLLogE, "<!%s!> " fmt, __nFNAME, p0, p1, p2))
#define ylnflogE4(fmt, p0, p1, p2, p3)     yllog((YLLogE, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3))
#define ylnflogE5(fmt, p0, p1, p2, p3, p4) yllog((YLLogE, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4))
#define ylnflogE6(fmt, p0, p1, p2, p3, p4, p5)            \
    yllog((YLLogE, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5))
#define ylnflogE7(fmt, p0, p1, p2, p3, p4, p5, p6)        \
    yllog((YLLogE, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6))
#define ylnflogE8(fmt, p0, p1, p2, p3, p4, p5, p6, p7)            \
    yllog((YLLogE, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6, p7))
#define ylnflogE9(fmt, p0, p1, p2, p3, p4, p5, p6, p7, p8)        \
    yllog((YLLogE, "<!%s!> " fmt, __nFNAME, p0, p1, p2, p3, p4, p5, p6, p7, p8))


/* ------------------------------
 * Macros for NF Log -- END
 * ------------------------------*/


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
    int             pcsz; /* Parameter Chain SiZe */                    \
    const char*     __nFNAME = #n;                                      \
    { /* jusg scope */                                                  \
        pcsz = ylchain_size(e);                                         \
        if((minp) > pcsz || pcsz > (maxp)) {                            \
            ylnflogE0("invalid number of parameter\n");                 \
            ylinterpret_undefined(YLErr_func_invalid_param);            \
        }                                                               \
    }                                                                   \
    do

/* This should be ylpair with YLDEFNF */                 
#define YLENDNF(n) while(0); }


#define ylnfcheck_atype_chain1(eXP, tY)                                 \
    do {                                                                \
        yle_t* _E = (eXP);                                              \
        while( !yleis_nil(_E) ) {                                       \
            if(!ylais_type(ylcar(_E), tY)) {                            \
                ylnflogE0("invalid parameter type\n");                  \
                ylinterpret_undefined(YLErr_func_invalid_param);        \
            }                                                           \
            _E = ylcdr(_E);                                             \
        }                                                               \
    } while(0)


/*
 * Type of all parameters should be same. And it should be ty0 or ty1.
 */
#define ylnfcheck_atype_chain2(eXP, tY0, tY1)                           \
    do {                                                                \
        yle_t* _E = (eXP);                                              \
        int _aty;                                                       \
        if(!yleis_nil(_E)) {                                            \
            int bok = TRUE;                                             \
            _aty = ylatype(ylcar(_E));                                  \
            if( tY0 != _aty && tY1 != _aty ) { bok = FALSE; }           \
            else {                                                      \
                _E = ylcdr(_E);                                         \
                while( !yleis_nil(_E) ) {                               \
                    if( !ylais_type(ylcar(_E), _aty) ) {                \
                        bok = FALSE; break;                             \
                    }                                                   \
                    _E = ylcdr(_E);                                     \
                }                                                       \
            }                                                           \
            if(!bok) {                                                  \
                ylnflogE0("invalid parameter type\n");                  \
                ylinterpret_undefined(YLErr_func_invalid_param);        \
            }                                                           \
        }                                                               \
    } while(0)


#define ylnfcheck_atype1(eXP, tY)                               \
    do {                                                        \
        yle_t* _E = (eXP);                                      \
        if(!ylais_type(_E, tY)) {                               \
            ylnflogE0("invalid parameter type\n");              \
            ylinterpret_undefined(YLErr_func_invalid_param);    \
        }                                                       \
    } while(0)

#define ylnfcheck_atype2(eXP, tY1, tY2)                         \
    do {                                                        \
        yle_t* _E = (eXP);                                      \
        if( !(ylais_type(_E, tY1)                               \
              || ylais_type(_E, tY2)) ) {                       \
            ylnflogE0("invalid parameter type\n");              \
            ylinterpret_undefined(YLErr_func_invalid_param);    \
        }                                                       \
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
 * EXTREANLY SENSITIVE FUNCTION
 * WARNING! : If you don't know what you are doing, exactly, DO NOT USE THIS!
 * Store current memory pool state.
 */
extern void
ylmp_push();

/**
 * EXTREANLY SENSITIVE FUNCTION
 * WARNING! : If you don't know what you are doing, exactly, DO NOT USE THIS!
 * GC if needed. <= Very SENSITIVE!
 */
extern void
ylmp_pop();

/**
 * return stack size (generated by ylmp_push/ylmp_pop)
 */
extern unsigned int
ylmp_stack_size(); 


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
