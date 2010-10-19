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
 * Header for custom-native-function(henceforth CNF) developer
 * Essential stuffs for CNF development.
 */
#ifndef ___YLDEv_h___
#define ___YLDEv_h___

#include "ylisp.h"
#include "yldef.h"


/*===================================
 *
 * Constants
 *
 *===================================*/


/**
 * !!NOTE!!
 *     "Difference in major version" means "incompatible"
 *     Version-Up SHOULD KEEP IT'S BACKWARD COMPATIBILITY!
 */
#define YLDEV_VERSION 0x00010000

#define yldev_ver_major(v) (((v)&0xffff0000)>>16)
#define yldev_ver_minor(v) ((v)&0x0000ffff)

/* --------------------------
 * YLE types
 * --------------------------*/
#define YLEPair            0           
#define YLEAtom            0x80000000  /**< 0 means ylpair */ 
#define YLEGCMark          0x40000000  /**< Mark used only for GC */
#define YLEMark            0x20000000  /**< Bit for Mark. This is used for several purpose! */

/* --------------------------
 * YLASymbol attributes
 * --------------------------*/
/* 0 is default symbol attribute */
#define YLASymbol_macro    0x80000000  /**< symbol represets macro */

/*===================================
 *
 * Types
 *
 *===================================*/

struct yle;

/* -------------------------
 * Structures for atom data type
 * -------------------------*/

/*
 * +Interfaces for atom operation+
 *    Each function pointer can be NULL.
 *    And this means, 'this operation is NOT supported or allowed'
 * 
 * !NOTE & WARNING!
 *    clone : [IMPORTANT]
 *        'clone' is ONLY USED for 'mlambda' and 'mset'.
 *        So, custom atom that doesn't allow/suport cloning, 
 *            SHOULD NOT be used at mlambda and mset!.
 *        Using atom which have NULL-clone-value at 'mlambda' or 'mset',
 *            cause 'ylinterpret_undefined' and interpreting is stopped with error.
 *
 *    eq :
 *        if NULL, yleq S-function always returns 'nil' - FALSE - with warning log.
 *
 *    to_string :
 *        if NULL, yleprint always returns special string - "!X!" - with warning log.
 *
 *    clean :
 *        if NULL, yleclean does nothing except for warning log.
 */
typedef struct {
    /*
     * @return : 1 if equal. Otherwise 0
     */
    int           (*eq)(const struct yle*, const struct yle*);
    /*
     * Deep copier
     * Values  will be shallow-copied before calling this function.
     * So, 'copy' SHOULD DO DEEP COPY if needed.
     * This can be NULL.(Nothing to deep-copy.
     * @map : 
     *    map of [orignal block - cloned block]
     *    Usually Trie is used as a data structure. (Hash is also good choice)
     * @return : <0 if error.
     */
    int           (*copy)(void* map, struct yle*, const struct yle*);
    /*
     * @return : static buffer. NULL if fails (ex. OOM)
     */
    const char*   (*to_string)(const struct yle*);
    /*
     * To visit referenced element of given atom.
     * (This may used to implement special atom type - ex. array, struture, class etc. if required)
     * return : 0: stop by user, 1: complete visiting.
     */
    int           (*visit)(struct yle*, void* user, 
                           /* 1: keep visiting / 0:stop visiting */
                           int(*)(void*/*user*/, 
                                  struct yle*/*referred element*/));
    void          (*clean)(struct yle*);
} ylatomif_t; /* atom inteface

/* nfunc : Native FUNCtion */
typedef struct yle* (*ylnfunc_t)(struct yle*, struct yle*);

typedef struct yle {
    int               t; /**< type */
    union {
        struct {
            /*
             * Interfaces to handle atom.
             * This value also can be "ID" of each atom type.
             * (Interface SHOULD BE DIFFERENT if atom TYPE is DIFFERENT.)
             */
            const ylatomif_t*    aif;

            /*
             * atom data
             */
            union {
                struct {
                    int            ty; /**< symbol type - 0 is default value */
                    char*          sym;
                } sym; /**< for YLASymbol */

                struct {
                    const char*    name;  /**< human readable function name */
                    ylnfunc_t      f;
                } nfunc; /**< for YLANfunc/YLASfunc */

                double     dbl; /**< for YLADouble */

                struct {
                    unsigned int sz;  /**< data size */
                    char*        d;   /**< data */
                } bin; /**< for YLABinary */

                void*      cd; /**< any data for custom atom(YLACustom) - Custom Data*/
            } u;
        } a; /**< atom */

        struct {
            struct yle  *car, *cdr;
        } p; /**< pair */
    } u;

    /*
     * members for debugging SHOULD BE put at the END of STRUCTURE!.
     * If not, whever debug configuration is changed, we should re-compile all other plug-in modules!!!
     */
#ifdef CONFIG_DBG_MEM
    /**< evaluation id that take this block from the pool. */
    unsigned int      evid; 
#endif /* CONFIG_DBG_MEM */
} yle_t;




/*===================================
 *
 * Macros
 *
 *===================================*/
#define not_used(e) do { (e)=(e); } while(0)

#define ylunroll16( expr, count, cond)              \
    switch( (count) & 0xf ) {                       \
        case 0: while (cond){                       \
            expr;                                   \
        case 15: expr;                              \
        case 14: expr;                              \
        case 13: expr;                              \
        case 12: expr;                              \
        case 11: expr;                              \
        case 10: expr;                              \
        case 9: expr;                               \
        case 8: expr;                               \
        case 7: expr;                               \
        case 6: expr;                               \
        case 5: expr;                               \
        case 4: expr;                               \
        case 3: expr;                               \
        case 2: expr;                               \
        case 1: expr;                               \
        }                                           \
    }

#define yleset_type(e, ty)      do{ (e)->t = (ty); } while(0)
#define yletype(e)              ((e)->t)
#define yleis_atom(e)           ((e)->t & YLEAtom)

/* macros for easy accessing */
#define yleatom(e)              ((e)->u.a)
#define ylaif(e)                ((e)->u.a.aif)
#define ylasym(e)               ((e)->u.a.u.sym)
#define ylanfunc(e)             ((e)->u.a.u.nfunc)
#define yladbl(e)               ((e)->u.a.u.dbl)
#define ylabin(e)               ((e)->u.a.u.bin)
#define ylacd(e)                ((e)->u.a.u.cd)

#define ylpcar(e)               ((e)->u.p.car)
#define ylpcdr(e)               ((e)->u.p.cdr)

/* 
 * each type-specific macros 
 */

/* -- common -- */
#define yleis_nil(e)            (ylnil() == (e))

#define yleset_mark(e)     ((e)->t |= YLEMark)
#define yleclear_mark(e)   ((e)->t &= ~YLEMark)
#define yleis_mark(e)      ((e)->t & YLEMark)

#define yleset_gcmark(e)     ((e)->t |= YLEGCMark)
#define yleclear_gcmark(e)   ((e)->t &= ~YLEGCMark)
#define yleis_gcmark(e)      ((e)->t & YLEGCMark)

/* -- symbol -- */
#define ylasymis_macro(ty)      ((ty) & YLASymbol_macro)
#define ylasymset_macro(ty)     ((ty) |= YLASymbol_macro)
#define ylasymclear_macro(ty)   ((ty) &= ~YLASymbol_macro)


/*
 * Variable number of argument in macro, is not used for the compatibility reason.
 * (GCC supports this. But some others don't)
 */
#define ylmalloc(x)     ylsysv()->malloc(x)
#define ylfree(x)       ylsysv()->free(x)

#ifdef CONFIG_ASSERT
#   define ylassert(x)  do { ylsysv()->assert((int)(x)); } while(0)
#else /* CONFIG_ASSERT */
#   define ylassert(x)  ((void*)0)
#endif /* CONFIG_ASSERT */

#ifdef CONFIG_LOG
#   define yllog(x)     do { ylsysv()->log x; } while(0)
#else /* CONFIG_LOG */
#   define yllog(x)     ((void*)0)
#endif /* CONFIG_LOG */

#define ylprint(x)      do { ylsysv()->print x; } while(0)
#define ylmpsz(x)       (ylsysv()->mpsz)
/* 
 * ! Predefined atoms !
 * To improve performance, we may use global variable instead of function.
 * This is referened very frequently!
 */
extern const yle_t* const ylg_predefined_true;
extern const yle_t* const ylg_predefined_nil;
extern const yle_t* const ylg_predefined_quote;
#define ylt()    ((yle_t*)ylg_predefined_true)
#define ylnil()  ((yle_t*)ylg_predefined_nil)
#define ylq()    ((yle_t*)ylg_predefined_quote)

#define yleis_predefined(e) ((e)==ylt() || (e)==ylnil() || (e)==ylq())

/* -------------------------------
 * Interface to get predefined aif(Atom InterFace)
 * -------------------------------*/
extern const ylatomif_t* const ylg_predefined_aif_sym;
extern const ylatomif_t* const ylg_predefined_aif_sfunc;
extern const ylatomif_t* const ylg_predefined_aif_nfunc;
extern const ylatomif_t* const ylg_predefined_aif_dbl;
extern const ylatomif_t* const ylg_predefined_aif_bin;
extern const ylatomif_t* const ylg_predefined_aif_nil;
#define ylaif_sym()     ylg_predefined_aif_sym
#define ylaif_sfunc()   ylg_predefined_aif_sfunc
#define ylaif_nfunc()   ylg_predefined_aif_nfunc
#define ylaif_dbl()     ylg_predefined_aif_dbl
#define ylaif_bin()     ylg_predefined_aif_bin
#define ylaif_nil()     ylg_predefined_aif_nil

/**
 * DO NOT USE "yltrue() == xxxx" if you don't know what you are doing!
 * Instead of this, use "ylfalse() != xxxx"
 * TRUE in ylisp means "Not NIL"!
 * FALSE in ylisp means "NIL"
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
        pcsz = ylelist_size(e);                                         \
        if((minp) > pcsz || pcsz > (maxp)) {                            \
            ylnflogE0("invalid number of parameter\n");                 \
            ylinterpret_undefined(YLErr_func_invalid_param);            \
        }                                                               \
    }                                                                   \
    do

/* This should be ylpair with YLDEFNF */                 
#define YLENDNF(n) while(0); }


#define ylnfcheck_atype_chain1(eXP, aIF)                                \
    do {                                                                \
        yle_t* _E = (eXP);                                              \
        while( !yleis_nil(_E) ) {                                       \
            if(!ylais_type(ylcar(_E), aIF)) {                           \
                ylnflogE0("invalid parameter type\n");                  \
                ylinterpret_undefined(YLErr_func_invalid_param);        \
            }                                                           \
            _E = ylcdr(_E);                                             \
        }                                                               \
    } while(0)


/*
 * Type of all parameters should be same. And it should be ty0 or ty1.
 */
#define ylnfcheck_atype_chain2(eXP, aIF0, aIF1)                         \
    do {                                                                \
        yle_t*              _E = (eXP);                                 \
        const ylatomif_t*   _aif;                                       \
        if(!yleis_nil(_E)) {                                            \
            int bok = TRUE;                                             \
            _aif = ylaif(ylcar(_E));                                    \
            if( aIF0 != _aif && aIF1 != _aif ) { bok = FALSE; }         \
            else {                                                      \
                _E = ylcdr(_E);                                         \
                while( !yleis_nil(_E) ) {                               \
                    if(!ylais_type(ylcar(_E), _aif) ) {                 \
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


#define ylnfcheck_atype1(eXP, aIF)                              \
    do {                                                        \
        yle_t* _E = (eXP);                                      \
        if(!ylais_type(_E, aIF)) {                              \
            ylnflogE0("invalid parameter type\n");              \
            ylinterpret_undefined(YLErr_func_invalid_param);    \
        }                                                       \
    } while(0)

#define ylnfcheck_atype2(eXP, aIF1, aIF2)                       \
    do {                                                        \
        yle_t* _E = (eXP);                                      \
        if( !(ylais_type(_E, aIF1)                              \
             || ylais_type(_E, aIF2)) ) {                       \
            ylnflogE0("invalid parameter type\n");              \
            ylinterpret_undefined(YLErr_func_invalid_param);    \
        }                                                       \
    } while(0)


/*===================================
 *
 * Non-static Functions/Symbols
 *
 *===================================*/
/* get system value */
extern const ylsys_t*
ylsysv();

/* -------------------------------
 * Interface to (un)register native functions
 * -------------------------------*/
extern ylerr_t
ylregister_nfunc(unsigned int version,
                 const char* sym, ylnfunc_t nfunc, 
                 const ylatomif_t* aif, const char* desc);

extern void
ylunregister_nfunc(const char* sym);

/* -------------------------------
 * Interface to memory pool
 * -------------------------------*/

/*
 * mp : Memory Pool
 * get yle_t block 
 */
extern yle_t*
ylmp_block();

/*
 * push base block
 */
extern void
ylmp_push(yle_t* e);

#define ylmp_push1(e0)                     do{ ylmp_push(e0); } while(0)
#define ylmp_push2(e0, e1)                 do{ ylmp_push1(e0); ylmp_push1(e1); } while(0)
#define ylmp_push3(e0, e1, e2)             do{ ylmp_push1(e0); ylmp_push2(e1, e2); } while(0)
#define ylmp_push4(e0, e1, e2, e3)         do{ ylmp_push1(e0); ylmp_push3(e1, e2, e3); } while(0)
#define ylmp_push5(e0, e1, e2, e3, e4)     do{ ylmp_push1(e0); ylmp_push4(e1, e2, e3, e4); } while(0)
#define ylmp_push6(e0, e1, e2, e3, e4, e5) do{ ylmp_push1(e0); ylmp_push5(e1, e2, e3, e4, e5); } while(0)
#define ylmp_push7(e0, e1, e2, e3, e4, e5, e6)                          \
    do{ ylmp_push1(e0); ylmp_push6(e1, e2, e3, e4, e5, e6); } while(0)
#define ylmp_push8(e0, e1, e2, e3, e4, e5, e6, e7)                      \
    do{ ylmp_push1(e0); ylmp_push7(e1, e2, e3, e4, e5, e6, e7); } while(0)
#define ylmp_push9(e0, e1, e2, e3, e4, e5, e6, e7, e8)                  \
    do{ ylmp_push1(e0); ylmp_push8(e1, e2, e3, e4, e5, e6, e7, e8); } while(0)

/*
 * pop base block
 */
extern void
ylmp_pop();

#define ylmp_pop1()  do{ ylmp_pop(); } while(0)
#define ylmp_pop2()  do{ ylmp_pop1(); ylmp_pop1(); } while(0)
#define ylmp_pop3()  do{ ylmp_pop1(); ylmp_pop2(); } while(0)
#define ylmp_pop4()  do{ ylmp_pop1(); ylmp_pop3(); } while(0)
#define ylmp_pop5()  do{ ylmp_pop1(); ylmp_pop4(); } while(0)
#define ylmp_pop6()  do{ ylmp_pop1(); ylmp_pop5(); } while(0)
#define ylmp_pop7()  do{ ylmp_pop1(); ylmp_pop6(); } while(0)
#define ylmp_pop8()  do{ ylmp_pop1(); ylmp_pop7(); } while(0)
#define ylmp_pop9()  do{ ylmp_pop1(); ylmp_pop8(); } while(0)

/*
 * clean base block stack
 */
extern void
ylmp_clean_stack();

extern void
ylmp_gc_if_needed();

/* -------------------------------
 * Interface to handle child process
 * -------------------------------*/

/*
 * Following 2 ylchild_proc_xxxx are for handling child process that ylisp waits for.
 */
/*
 * Actually, 'pid_t' should be used.
 * But we don't want to include "sys/types.h" here..
 * So, long is used instead of 'pid_t'
 */
extern int
ylchild_proc_set(long pid);

extern void
ylchild_proc_unset();


/* -------------------------------
 * Interface to handle ylisp element.
 *
 * !!CONVENTION!!
 * ylechain_xxx :
 *    function accesses element and those that it refers recursively.
 * ylexxx
 *    function accesses only given element.
 * -------------------------------*/

/*
 * clone given 'yle_t*' element and it's chain too.
 */
extern yle_t*
ylechain_clone(const yle_t* e);

extern int
ylelist_size(const yle_t* e);

/* 
 * get print string.
 * returned memory SHOULD NOT be freed.
 * (This function uses static buffer, So please keep this in mind!)
 * -> Wrong usage : printf("%s / %s", yleprint(e1), yleprint(e2));
 */
extern const char*
ylechain_print(const yle_t* e);

/*===================================
 *
 * static Functions/Symbols
 *
 *===================================*/
/*
 * visit full node. return value is resolve til now.
 */
#define _DEF_VISIT_FUNC(fTYPE, nAME, pREeXP, cOND, eXP)                 \
    fTYPE int                                                           \
    nAME(void* user, yle_t* e) {                                        \
        pREeXP;                                                         \
        if(cOND) {                                                      \
            eXP;                                                        \
            if(yleis_atom(e)) {                                         \
                if(ylaif(e)->visit) { ylaif(e)->visit(e, user, &nAME); } \
            } else {                                                    \
                ylassert( (ylpcar(e) && ylpcdr(e)) || (!ylpcar(e) && !ylpcdr(e))); \
                if(ylpcar(e)) {                                         \
                    nAME(user, ylpcar(e));                              \
                    nAME(user, ylpcdr(e));                              \
                }                                                       \
            }                                                           \
        }                                                               \
        return 0;                                                       \
    }

/* --------------------------------
 * Element - Pair
 * --------------------------------*/
static inline void
ylpsetcar(yle_t* e, yle_t* exp) {
    ylpcar(e) = exp;
}

static inline void
ylpsetcdr(yle_t* e, yle_t* exp) {
    ylpcdr(e) = exp;
}


static inline void
ylpassign(yle_t* e, yle_t* car, yle_t* cdr) {
    yleset_type(e, YLEPair);
    ylpsetcar(e, car);
    ylpsetcdr(e, cdr);
}

static inline yle_t*
ylpcreate(yle_t* car, yle_t* cdr) {
    yle_t* e = ylmp_block();
    ylpassign(e, car, cdr);
    return e;
}


/* --------------------------------
 * Element - Atom - Common
 * --------------------------------*/
/**
 * This is used very often!
 */
static inline int
ylais_type(const yle_t* e, const ylatomif_t* aif) {
    return (yleis_atom(e) && ylaif(e) == aif );
}

/* --------------------------------
 * Element - Atom - Symbol
 * --------------------------------*/
/**
 * @sym: [in] responsibility for memory handling is passed to @se.
 */
static inline void
ylaassign_sym(yle_t* e, char* sym) {
    yleset_type(e, YLEAtom);
    ylaif(e) = ylaif_sym();
    ylasym(e).sym = sym;
    ylasym(e).ty = 0; /* 0 is default */
}

static inline yle_t*
ylacreate_sym(char* sym) {
    yle_t* e = ylmp_block();
    ylaassign_sym(e, sym);
    return e;
}

/* --------------------------------
 * Element - Atom - NFunc/SFunc
 * --------------------------------*/
static inline void
ylaassign_nfunc(yle_t* e, ylnfunc_t f, const char* name) {
    yleset_type(e, YLEAtom);
    ylaif(e) = ylaif_nfunc();
    ylanfunc(e).f = f;
    ylanfunc(e).name = name;
}

static inline void
ylaassign_sfunc(yle_t* e, ylnfunc_t f, const char* name) {
    yleset_type(e, YLEAtom);
    ylaif(e) = ylaif_sfunc();
    ylanfunc(e).f = f;
    ylanfunc(e).name = name;
}

static inline yle_t*
ylacreate_nfunc(ylnfunc_t f, const char* name) {
    yle_t* e = ylmp_block();
    ylaassign_nfunc(e, f, name);
    return e;
}

static inline yle_t*
ylacreate_sfunc(ylnfunc_t f, const char* name) {
    yle_t* e = ylmp_block();
    ylaassign_sfunc(e, f, name);
    return e;
}


/* --------------------------------
 * Element - Atom - Double
 * --------------------------------*/
static inline void
ylaassign_dbl(yle_t* e, double d) {
    yleset_type(e, YLEAtom);
    ylaif(e) = ylaif_dbl();
    yladbl(e) = d;
}

static inline yle_t*
ylacreate_dbl(double d) {
    yle_t* e = ylmp_block();
    ylaassign_dbl(e, d);
    return e;
}

/* --------------------------------
 * Element - Atom - Binary
 * --------------------------------*/
static inline void
ylaassign_bin(yle_t* e, char* data, unsigned int len) {
    yleset_type(e, YLEAtom);
    ylaif(e) = ylaif_bin();
    ylabin(e).d = data;
    ylabin(e).sz = len;
}

static inline yle_t*
ylacreate_bin(char* data, unsigned int len) {
    yle_t* e = ylmp_block();
    ylaassign_bin(e, data, len);
    return e;
}

/* --------------------------------
 * Element - Atom - Custom
 * --------------------------------*/
static inline void
ylaassign_cust(yle_t* e, const ylatomif_t* aif, void* data) {
    yleset_type(e, YLEAtom);
    ylaif(e) = aif;
    ylacd(e) = data;
}

static inline yle_t*
ylacreate_cust(const ylatomif_t* aif, void* data) {
    yle_t* e = ylmp_block();
    ylaassign_cust(e, aif, data);
    return e;
}
#endif /* ___YLDEv_h___ */
