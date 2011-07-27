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


#ifndef ___GSYm_h___
#define ___GSYm_h___

#include "lisp.h"


extern ylerr_t
ylgsym_init(void);

extern void
ylgsym_deinit(void);

/**
 * @sty : symbol type (see comment of 'YLASymbol attributes' at yldev.h)
 * @return:
 *   -1: error
 *    0: newly inserted
 *    1: overwritten
 */
extern int
ylgsym_insert(const char* sym, int sty, yle_t* e);

/**
 * @return:
 *    <0: fail. cannot be found
 *    0 : deleted.
 */
extern int
ylgsym_delete(const char* sym);

/**
 * @description: can be NULL for empty descritpion.
 */
extern int
ylgsym_set_description(const char* sym, const char* description);

/**
 * At the moment of getting description, the other thread may unset that symbol
 *   from global.
 * Than memory of descriptiion will be freed and cannot be referred.
 * To avoid this, gsym will copy desc to buffer.
 * @return: 0< if @sym is not in global symbol space or unknown error.
 */
extern int
ylgsym_get_description(char* b, unsigned int bsz, const char* sym);

/**
 * @outty [out]: type of symbol. this can be NULL if not to want to get.
 * @return     : NULL if @sym is not in trie.
 */
extern yle_t*
ylgsym_get(int* outty, const char* sym);

/**
 * Mark memory blocks those can be reachable from Trie for GC.
 * @return: number of memblock that is marked as 'Reachable'
 */
extern void
ylgsym_gcmark(void);

/**
 * get auto-completed-symbol
 * @return:
 *    YLTRIEBranch : success and we meet the branch.
 *    YLTRIELeaf : success and meet the leaf.
 *    YLTRIEFail : cannot find node which matchs prefix.
 *    <0 : error (ex. not enough buffer size)
 */
extern int
ylgsym_auto_complete(const char* start_with,
		     char* buf, unsigned int bufsz);

/**
 * @max_symlen: [out] max symbol length of candidates(based on 'sizeof(char)'
 *                    - excluding prefix.
 * @return: <0 : internal error(not enough internal buffer size)
 */
extern int
ylgsym_nr_candidates(const char* start_with,
		     unsigned int* max_symlen);

/**
 * @return: <0: error. Otherwise number of candidates found.
 */
extern int
ylgsym_candidates(const char* start_with, char** ppbuf,
		  unsigned int ppbsz,
		  unsigned int pbsz);

#endif /* ___GSYm_h___ */
