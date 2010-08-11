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



#ifndef ___DEf_h___
#define ___DEf_h___

#include "config.h"

#ifndef NULL
#   define NULL ((void*)0)
#endif

#ifndef TRUE
#   define TRUE 1
#endif

#ifndef FALSE
#   define FALSE 0
#endif

#ifndef offset_of
#   define offset_of(type, member) ((unsigned int) &((type*)0)->member)
#endif

#ifndef container_of
#   define container_of(ptr, type, member) ((type*)(((char*)(ptr)) - offset_of(type, member)))
#endif

#define unroll16( expr, count, cond)                \
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


/*
 * For debugging
 */
#ifdef __DBG_EVAL
#   define dbg_eval(x)     do{ x } while(0)
#else /* _YLDBG_EVAL */
#   define dbg_eval(x)
#endif /* _YLDBG_EVAL */

#ifdef __DBG_MEM
#   define dbg_mem(x)      do{ x } while(0)
#else /* __DBG_MEM */
#   define dbg_mem(x)
#endif /* __DBG_MEM */

#ifdef __DBG_GEN
#   define dbg_gen(x)      do{ x } while(0)
#else /* __DBG_MEM */
#   define dbg_gen(x)
#endif /* __DBG_MEM */


#endif /* ___DEf_h___ */
