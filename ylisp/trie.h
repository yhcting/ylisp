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



#ifndef ___TRIe_h___
#define ___TRIe_h___

#include "lisp.h"

/* symbol value type */
enum {
    TRIE_VType_macro,
    TRIE_VType_set,
};

enum {
    TRIEMeet_branch   = 0,
    TRIEMeet_leaf,
    TRIEPrefix_not_matched,
};

extern ylerr_t
yltrie_init();

extern void
yltrie_deinit();

/**
 * @return: 
 *   -1: error
 *    0: newly inserted
 *    1: overwritten
 */
extern int
yltrie_insert(const char* sym, int ty, yle_t* e);

/**
 * @return: 
 *    <0: fail. cannot be found
 *    0 : deleted.
 */
extern int
yltrie_delete(const char* sym);

/**
 * @outty [out]: type of symbol. this can be NULL if not to want to get.
 * @return     : NULL if @sym is not in trie.
 */
extern yle_t*
yltrie_get(int* outty, const char* sym);

extern int
yltrie_set_description(const char* sym, const char* description);

/**
 * @return: NULL if @sym is not in trie.
 */
extern const char*
yltrie_get_description(const char* sym);

/**
 * Mark memory blocks those can be reachable from Trie,  'Reachable' 
 * @return: number of memblock that is marked as 'Reachable'
 */
extern void
yltrie_mark_reachable();

/**
 * get more symbols to make longest prefix.
 * @return: 
 *    TRIEMeet_branch : success and we meet the branch. 
 *    TRIEMeet_leaf : success and meet the leaf. 
 *    TRIEPrefix_not_matched : cannot find node which matchs prefix.
 *    <0 : error (ex. not enough buffer size)
 */
extern int
yltrie_get_more_possible_prefix(const char* prefix, char* buf, unsigned int bufsz);

/**
 * @max_symlen: [out] max symbol length of candidates(based on 'sizeof(char)' - excluding prefix.
 * @return: <0 : internal error(not enough internal buffer size)
 */
extern int
yltrie_get_candidates_num(const char* prefix, unsigned int* max_symlen);

/**
 * @return: <0: error. Otherwise number of candidates found.
 */
extern int
yltrie_get_candidates(const char* prefix, 
                      char** ppbuf,       /* in/out */
                      unsigned int ppbsz, /* size of ppbuf - regarding 'ppbuf[i]' */
                      unsigned int pbsz); /* size of pbuf - regarding 'ppbuf[0][x]' */



#endif /* ___TRIe_h___ */
