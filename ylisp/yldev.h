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

/*******************************************
 *
 * BIG RULE of CNF
 *    - Keep considering about GC.
 *    - Do operation as simple as possible.
 *      (ex. spawning more than one child process is WRONG!)
 *
 *******************************************/


/**
 * Header for custom-native-function(henceforth CNF) developer
 * Essential stuffs for CNF development.
 */
#ifndef ___YLDEv_h___
#define ___YLDEv_h___

/******************************************
 * !! Multi-Threaded Evaluation !!
 *
 * Most functions listed here, are used in 'inteval_lock'!
 * And, functiond that affects only local state(stack frame),
 *  it can be used out of 'inteval_lock'.
 * But, it is easy to assume that all functions should be in the 'lock'.!
 *
 * NOTE!
 *    CNF code already in 'inteval_lock'!
 *    So, in many cases, you don't need to consider of the 'lock'!
 *
 ******************************************/

#include <stdint.h>
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

/*===================================
 *
 * Types
 *
 *===================================*/

struct sYle;
typedef struct _sEtcxt yletcxt_t;
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
    int           (*eq)(const struct sYle*, const struct sYle*);
    /*
     * Deep copier
     * Values  will be shallow-copied before calling this function.
     * So, 'copy' SHOULD DO DEEP COPY if needed.
     * This can be NULL.(Nothing to deep-copy.
     * @map :
     *    map of [orignal block - cloned block]
     *    Usually Trie is used as a data structure. (Hash is also good choice)
     * @return : <0 if error.
     *
     * !!! NOT USED : RESERVED FOR FUTURE !!!
     */
    int           (*copy)(void*/*map*/, struct sYle*, const struct sYle*);
    /*
     * @sz : exclude space for trailing 0.
     *       That is this is one-byte-smaller than real-buffer-size.
     *       It's caller's responsibility to pass ''sz' less than 1 byte of real one.
     *       So, implementation of 'to_string' don't need to add trailing 0 to complete C-style string.
     * @return :
     *    bytes written to buffer if success. -1 if fails (ex. Not enough buffer size)
     *
     */
    int           (*to_string)(const struct sYle*, char*/*buf*/, unsigned int/*sz*/);
    /*
     * To visit referenced element of given atom.
     * (This may used to implement special atom type - ex. array, struture, class etc. if required)
     * return : 0: stop by user, 1: complete visiting.
     */
    int           (*visit)(struct sYle*, void* user,
                           /* 1: keep visiting / 0:stop visiting */
                           int(*)(void*/*user*/,
                                  struct sYle*/*referred element*/));
    void          (*clean)(struct sYle*);
} ylatomif_t; /* atom inteface */

/* nfunc : Native FUNCtion */
typedef struct sYle* (*ylnfunc_t)(yletcxt_t*, struct sYle*, struct sYle*);

typedef struct sYle {
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
                    int            ty; /**< symbol type - interenally used */
                    char*          sym;
                } sym; /**< for YLASymbol */

                struct {
                    const char*    name;  /**< human readable function name */
                    ylnfunc_t      f;
                } nfunc; /**< for YLANfunc/YLASfunc */

                double     dbl;

                struct {
                    unsigned int    sz;  /**< data size */
                    unsigned char*  d;   /**< data */
                } bin; /**< for YLABinary */

                void*      cd; /**< any data for custom atom(YLACustom) - Custom Data*/
            } u;
        } a; /**< atom */

        struct {
            struct sYle  *car, *cdr;
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
/* type casting integer to pointer */
#define itoptr(x) ((void*)(intptr_t)(x))

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

#define yleset_mark(e)          ((e)->t |= YLEMark)
#define yleclear_mark(e)        ((e)->t &= ~YLEMark)
#define yleis_mark(e)           (!!((e)->t & YLEMark))

#define yleset_gcmark(e)        ((e)->t |= YLEGCMark)
#define yleclear_gcmark(e)      ((e)->t &= ~YLEGCMark)
#define yleis_gcmark(e)         (!!((e)->t & YLEGCMark))

/*
 * Variable number of argument in macro, is not used for the compatibility reason.
 * (GCC supports this. But some others don't)
 */
#define ylmalloc(x)     ylsysv()->malloc(x)
#define ylfree(x)       ylsysv()->free(x)

#ifdef CONFIG_ASSERT
#   define ylassert(x)  do { ylsysv()->assert_(!!(x)); } while(0)
#else /* CONFIG_ASSERT */
#   define ylassert(x)  ((void*)0)
#endif /* CONFIG_ASSERT */

#ifdef CONFIG_LOG
#   define yllog(lv, x...)  do { ylsysv()->log (lv, x); } while(0)
#else /* CONFIG_LOG */
#   define yllog(lv, x...)  ((void*)0)
#endif /* CONFIG_LOG */

#define ylprint(x...)   do { ylsysv()->print (x); } while(0)
#define ylmode()        (ylsysv()->mode)
#define ylmpsz()        (ylsysv()->mpsz)
#define ylgctp()        (ylsysv()->gctp)
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


#define ylb2e(i)          ((i)? ylt(): ylnil()) /* boolean => yle_t* */
#define yle2b(e)         !(ylnil() == (e))      /* yle_t* => boolean */
#define yleis_true(e)     (ylnil() != (e))
#define yleis_false(e)    (ylnil() == (e))


/**
 * notify that ylisp reached to the ylinterpret_undefined state.
 * @r: reason
 */
extern void ylinterpret_undefined(long reason);

/* ------------------------------
 * Macros for Log -- START
 * ------------------------------*/
#define yllogV(x...) yllog (YLLogV, x)
#define yllogD(x...) yllog (YLLogD, x)
#define yllogI(x...) yllog (YLLogI, x)
#define yllogW(x...) yllog (YLLogW, x)
#define yllogE(x...) yllog (YLLogE, x)


/* ------------------------------
 * Macros for NF Log -- START
 *    'nfname' is predefined string variable of native function name.
 *    This log macros automatically add 'nfname' as a prefix of log!
 * ------------------------------*/

#define ylnflogV(x...)  do { yllog (YLLogV, "<!%s!> ", __nFNAME); yllog (YLLogV, x); } while (0)
#define ylnflogD(x...)  do { yllog (YLLogD, "<!%s!> ", __nFNAME); yllog (YLLogD, x); } while (0)
#define ylnflogI(x...)  do { yllog (YLLogI, "<!%s!> ", __nFNAME); yllog (YLLogI, x); } while (0)
#define ylnflogW(x...)  do { yllog (YLLogW, "<!%s!> ", __nFNAME); yllog (YLLogW, x); } while (0)
#define ylnflogE(x...)  do { yllog (YLLogE, "<!%s!> ", __nFNAME); yllog (YLLogE, x); } while (0)


/* ------------------------------
 * Macros for NF Log -- END
 * ------------------------------*/


/*========================
 * To define native functions.
 *========================*/
/* make native function name : lnf (Lisp Native Funcion) */
#define YLNFN(n)    __LNF__##n##__



#define YLDECLNF(n) yle_t* YLNFN(n)(yletcxt_t* cxt, yle_t* e, yle_t* a)
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
            ylnflogE ("invalid number of parameter\n");                 \
            ylinterpret_undefined(YLErr_func_invalid_param);            \
        }                                                               \
    }                                                                   \
    do

/* This should be ylpair with YLDEFNF */
#define YLENDNF(n) while(0); }

#define ylnfcheck_parameter(cond)                               \
    if(!(cond)) {                                               \
        ylnflogE ("invalid parameter type\n");                  \
        ylinterpret_undefined(YLErr_func_invalid_param);        \
    }


#define ylelist_foreach(e)       for(;!yleis_nil(e);(e)=ylpcdr(e))

/*===================================
 *
 * Non-static Functions/Symbols
 *
 *===================================*/
struct yldynb;

/* get system value */
extern const ylsys_t*
ylsysv();

/*
 * Get perthread dynamic buffer.
 * We can use this without worry about memory leak of dynamic buffer!
 */
struct yldynb*
ylethread_buf(yletcxt_t* cxt);

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
 * add Base Block
 */
extern void
ylmp_add_bb(yle_t* e);

#define ylmp_add_bb1(e0)                     do{ ylmp_add_bb(e0); } while(0)
#define ylmp_add_bb2(e0, e1)                 do{ ylmp_add_bb1(e0); ylmp_add_bb1(e1); } while(0)
#define ylmp_add_bb3(e0, e1, e2)             do{ ylmp_add_bb1(e0); ylmp_add_bb2(e1, e2); } while(0)
#define ylmp_add_bb4(e0, e1, e2, e3)         do{ ylmp_add_bb1(e0); ylmp_add_bb3(e1, e2, e3); } while(0)
#define ylmp_add_bb5(e0, e1, e2, e3, e4)     do{ ylmp_add_bb1(e0); ylmp_add_bb4(e1, e2, e3, e4); } while(0)
#define ylmp_add_bb6(e0, e1, e2, e3, e4, e5) do{ ylmp_add_bb1(e0); ylmp_add_bb5(e1, e2, e3, e4, e5); } while(0)
#define ylmp_add_bb7(e0, e1, e2, e3, e4, e5, e6)                          \
    do{ ylmp_add_bb1(e0); ylmp_add_bb6(e1, e2, e3, e4, e5, e6); } while(0)
#define ylmp_add_bb8(e0, e1, e2, e3, e4, e5, e6, e7)                      \
    do{ ylmp_add_bb1(e0); ylmp_add_bb7(e1, e2, e3, e4, e5, e6, e7); } while(0)
#define ylmp_add_bb9(e0, e1, e2, e3, e4, e5, e6, e7, e8)                  \
    do{ ylmp_add_bb1(e0); ylmp_add_bb8(e1, e2, e3, e4, e5, e6, e7, e8); } while(0)

/*
 * pop base block
 */
extern void
ylmp_rm_bb(yle_t* e);

#define ylmp_rm_bb1(e0)                     do{ ylmp_rm_bb(e0); } while(0)
#define ylmp_rm_bb2(e0, e1)                 do{ ylmp_rm_bb1(e0); ylmp_rm_bb1(e1); } while(0)
#define ylmp_rm_bb3(e0, e1, e2)             do{ ylmp_rm_bb1(e0); ylmp_rm_bb2(e1, e2); } while(0)
#define ylmp_rm_bb4(e0, e1, e2, e3)         do{ ylmp_rm_bb1(e0); ylmp_rm_bb3(e1, e2, e3); } while(0)
#define ylmp_rm_bb5(e0, e1, e2, e3, e4)     do{ ylmp_rm_bb1(e0); ylmp_rm_bb4(e1, e2, e3, e4); } while(0)
#define ylmp_rm_bb6(e0, e1, e2, e3, e4, e5) do{ ylmp_rm_bb1(e0); ylmp_rm_bb5(e1, e2, e3, e4, e5); } while(0)
#define ylmp_rm_bb7(e0, e1, e2, e3, e4, e5, e6)                          \
    do{ ylmp_rm_bb1(e0); ylmp_rm_bb6(e1, e2, e3, e4, e5, e6); } while(0)
#define ylmp_rm_bb8(e0, e1, e2, e3, e4, e5, e6, e7)                      \
    do{ ylmp_rm_bb1(e0); ylmp_rm_bb7(e1, e2, e3, e4, e5, e6, e7); } while(0)
#define ylmp_rm_bb9(e0, e1, e2, e3, e4, e5, e6, e7, e8)                  \
    do{ ylmp_rm_bb1(e0); ylmp_rm_bb8(e1, e2, e3, e4, e5, e6, e7, e8); } while(0)

/*
 * clean base block set
 */
extern void
ylmp_clean_bb();

/* -------------------------------
 * Interface for multi-threaded evaluation
 * -------------------------------*/

/*
 * To notify that evaluation thread enters safe state (including GC).
 * Thread usually enter GC safe state at the end of evaluation.
 * But in some cases, one-evaluation-cycle takes quite long time.
 * (Usually, inside CNF)
 * And, this thread can be bottle-neck to trigger GC.
 * To avoid this, thread may notify "I'm safe to GC from now on" manually!
 * Interrupt evaluation (including GC) only can be triggerred at only this point.
 * (Where 'ylmt_notify_safe' is called.)
 */
extern void
ylmt_notify_safe(yletcxt_t*);

/*
 * I'm not safe from now on
 */
extern void
ylmt_notify_unsafe(yletcxt_t*);

/*
 * This SHOULD be used to be safe from "kill thread!"
 * value is added at the first of storage (link stack push.)
 * @ccb : close callback.
 */
extern int
ylmt_add_pres(yletcxt_t* cxt, void* pres, void(*ccb)(void*));

/*
 * Recently added 'pres' has priority if there are more than one item
 *  that have same 'pres' value..
 * This function doesn't close 'pres'. It just unregister 'pres' value!.
 * Closing 'pres' is caller's responsibility.
 */
extern int
ylmt_rm_pres(yletcxt_t* cxt, void* pres);

/* -------------------------------
 * Interface to handle ylisp element.
 *
 * !!CONVENTION!!
 * ylechain_xxx :
 *    function accesses element and those that it refers recursively.
 * ylexxx
 *    function accesses only given element.
 * -------------------------------*/

extern int
ylelist_size(const yle_t* e);

/*
 * Buffer of dynb should end with trailing 0!
 */
extern const char*
ylechain_print(struct yldynb* dynb, const yle_t* e);


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

static inline int
ylais_type2(const yle_t* e, const ylatomif_t* aif0, const ylatomif_t* aif1) {
    return (yleis_atom(e) && (ylaif(e) == aif0 || ylaif(e) == aif1) );
}

/**
 * @return : 0 for FALSE, 1 for TRUE
 */
static inline int
yleis_pair_chain(yle_t* e) {
    ylelist_foreach(e) {
        if(yleis_atom(ylpcar(e))) { return 0; }
    }
    return 1;
}

static inline int
ylais_type_chain(yle_t* e, const ylatomif_t* aif) {
    ylelist_foreach(e) {
        if(!ylais_type(ylpcar(e), aif)) { return 0; }
    }
    return 1;
}

static inline int
ylais_type_chain2(yle_t* e,
                  const ylatomif_t* aif0, const ylatomif_t* aif1) {
    ylelist_foreach(e) {
        if(!ylais_type2(ylpcar(e), aif0, aif1)) { return 0; }
    }
    return 1;
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
ylaassign_bin(yle_t* e, unsigned char* data, unsigned int len) {
    yleset_type(e, YLEAtom);
    ylaif(e) = ylaif_bin();
    ylabin(e).d = data;
    ylabin(e).sz = len;
}

static inline yle_t*
ylacreate_bin(unsigned char* data, unsigned int len) {
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
