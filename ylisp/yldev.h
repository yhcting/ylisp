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


/**
 * !!NOTE!!
 *     "Difference in major version" means "incompatible"
 *     Version-Up SHOULD KEEP IT'S BACKWARD COMPATIBILITY!
 */
#define YLDEV_VERSION 0x00010000

#define yldev_ver_major(v) (((v)&0xffff0000)>>16)
#define yldev_ver_minor(v) ((v)&0x0000ffff)

/* A: means Atom */
#define YLASymbol          0x00
#define YLANfunc           0x01 /**< normal native C function */
#define YLASfunc           0x02 /**< parameter is passed before evaluation */
#define YLADouble          0x03 /**< Double type */
#define YLABinary          0x04 /**< Binary data */
#define YLAtype_mask       0x000000ff

#define YLEPair            0           
#define YLEAtom            0x80000000  /**< 0 means ylpair */ 
#define YLEReachable       0x40000000  /**< used for GC of memory pool */

/*===================================
 * Struct to represent ylatom data
 *===================================*/

struct yle;
/* nfunc : Native FUNCtion */
typedef struct yle* (*ylnfunc_t)(struct yle*, struct yle*);

typedef struct yle {
    int               t; /**< type */
    unsigned int      r; /**< reference count */
    union {
        struct {
            union {
                struct {
                    char*          sym;
                } sym; /**< for ATOM SYMBOL */

                struct {
                    const char*    name;
                    ylnfunc_t      f;
                } nfunc; /**< for ATOM_NFUNC */

                double     dbl; /**< for ATOM_DOUBLE */

                struct {
                    unsigned int sz;  /**< data size */
                    char*        d;   /**< data */
                } bin;
                
            } u;
        } a; /* ylatom */

        struct {
            struct yle  *car, *cdr;
        } p; /* ylpair */
    } u;

    /*
     * members for debugging SHOULD BE put at the END of STRUCTURE!.
     * If not, whever debug configuration is changed, we should re-compile all other plug-in modules!!!
     */
#ifdef __DBG_MEM
    /**< evaluation id that take this block from the pool. */
    unsigned int      evid; 
#endif /* __DBG_MEM */
} yle_t;



/* get system value */
extern const ylsys_t*
ylsysv();

/*
 * Variable number of argument in macro, is not used for the compatibility reason.
 * (GCC supports this. But some others don't)
 */
#define ylmalloc(x)     ylsysv()->malloc(x)
#define ylfree(x)       ylsysv()->free(x)

#ifdef __ENABLE_ASSERT
#   define ylassert(x)  ylsysv()->assert((int)(x))
#else /* __ENABLE_ASSERT */
#   define ylassert(x)  ((void*)0)
#endif /* __ENABLE_ASSERT */

#ifdef __ENABLE_LOG
#   define yllog(x)     ylsysv()->log x
#else /* __ENABLE_LOG */
#   define yllog(x)     ((void*)0)
#endif /* __ENABLE_LOG */

#define ylprint(x)      ylsysv()->print x

/*
 * mp : Memory Pool
 * get yle_t block 
 */
extern yle_t*
ylmp_get_block();

extern ylerr_t
ylregister_nfunc(unsigned int version,
                 const char* sym, ylnfunc_t nfunc, 
                 int ftype, const char* desc);

extern void
ylunregister_nfunc(const char* sym);

#endif /* ___YLDEv_h___ */
