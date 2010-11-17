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

extern ylerr_t
ylmp_init();

extern void
ylmp_deinit();

/*****************************************
 * Multi-Thread
 *****************************************/
void
ylmp_pre_add_thread(yletcxt_t* thd);

void
ylmp_post_add_thread(yletcxt_t* thd);

void
ylmp_pre_rm_thread(yletcxt_t* thd);

void
ylmp_post_rm_thread(yletcxt_t* thd);


/*
 * Clean memory block.
 * All data in memory block will be removed!
 * (memory block that is not in use, should be in clean-state!)
 *
 * This is for making GC be happy.
 * There are two unexpected state during GC
 *     Case 1 : it's type is PAIR but it's car and cdr is NOT NULL.
 *     Case 2 : it's type is ATOM but it's 'atomif' value is invalid.
 *
 * To avoid these two error-state, we need to clean block into expected state after/before using block.
 * NOTE! : Reference count is NOT handled at 'ylmp_clean_block'.
 *
 * Appendix :
 *     Case 1 (detail)
 *     This cleaned block may be used as a pair. That is a point!.
 *     When the block is used as pair later,
 *      if car/cdr value is not initialised as NULL,
 *      'ylpassign()' may try to unref car/cdr which has invalid initial value.
 *     (See, ylpassign->ylpsetcar/ylpsetcdr)
 *     => To set into NULL, we SHOULD NOT use ylpsetcar()/ylpsetcdr().
 *        Set directly.
 */
static inline void
ylmp_clean_block(yle_t* e) {
    /*
     * Clean-block's value is like this.!
     */
    yleset_type(e, YLEPair);
    dbg_mt( ylanfunc(e).f = (void*)0xdeaddead; );
    ylpcar(e) = ylpcdr(e) = NULL;
}

#endif /* ___MEMPOOl_h___ */
