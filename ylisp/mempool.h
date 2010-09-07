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



#ifndef ___MEMPOOl_h___
#define ___MEMPOOl_h___

#include "lisp.h"

/*
 * In the memory pool, reference count of memory block is YLEINVALID_REFCNT.
 */
#define YLEINVALID_REFCNT  (0xffffffff)


typedef enum {
    YLMP_GCSCAN_PARTIAL,
    YLMP_GCSCAN_FULL,
} ylmp_gcscanty_t;

extern ylerr_t
ylmp_init();

extern void
ylmp_deinit();


/*
 * Those two are very sensitive and dangerous function!
 * This may corrupt current running interpreting.
 * So, if you don't know what you are doing, DON'T USE THIS!
 */

/* GC when interpreting error occurs */
extern void
ylmp_recovery_gc();

extern void
ylmp_scan_gc(ylmp_gcscanty_t ty);

/*
 * @return: current usage
 */
extern unsigned int
ylmp_usage();


extern void
ylmp_log_stat(int loglevel);

extern void
ylmp_print_stat();

/*
 * is this block is free?
 * if true, this block is already in the free block list.
 */
static int
ylmp_is_free_block(const yle_t* e) {
    return YLEINVALID_REFCNT == ylercnt(e);
}

#endif /* ___MEMPOOl_h___ */
