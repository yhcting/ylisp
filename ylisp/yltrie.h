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


/*================================
 *
 * 4bit Trie
 *
 * This is just DATA STRUCTURE HELPER for YILSP
 *================================*/

#ifndef ___YLTRIe_h___
#define ___YLTRIe_h___

typedef struct _trie yltrie_t;

/*
 * @fcb : callback to free trie value.
 */
extern yltrie_t*
yltrie_create(void(*fcb)(void*));

extern void
yltrie_destroy(yltrie_t*);

/**
 * @return: 
 *   -1: error
 *    0: newly inserted
 *    1: overwritten
 */
extern int
yltrie_insert(yltrie_t*, const char* sym, void* v);

/**
 * @return: 
 *    <0: fail. cannot be found
 *    0 : deleted.
 */
extern int
yltrie_delete(yltrie_t*, const char* sym);

/**
 * @return     : NULL if @sym is not in trie.
 */
extern void*
yltrie_get(yltrie_t*, const char* sym);

/**
 * walking order is dictionary order of symbol.
 * @return:
 *    0 : success. End of trie.
 *    1 : stop by user callback
 *   -1 : fail. (ex. @from is invalid prefix of symbol.)
 */
extern int
yltrie_walk(yltrie_t*, void* user, const char* from,
            /* return : b_keepgoing => return 1 for keep going, 0 for stop and don't do anymore*/
            int(cb)(void* user, const char* sym, void* v));

/*
 * return value of auto complete
 */
enum {
    YLTRIEBranch   = 0,
    YLTRIELeaf,
    YLTRIEFail,
};

/**
 * @start_with : start string.
 * @buf[out] : string for auto complete is store at.
 * @return
 *    YLTRIEFail : ex. buffer size is not enough
 */
int
yltrie_auto_complete(yltrie_t* t, 
                     const char* start_with, 
                     char* buf, unsigned int bufsz);


#endif /* ___YLTRIe_h___ */
