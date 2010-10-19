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

/*
 * get fcb
 */
extern void(*yltrie_fcb(const yltrie_t* t))(void*);


/**
 * @return     : NULL if @sym is not in trie.
 */
extern void*
yltrie_get(yltrie_t*, const char* sym);

/**
 * get reference of value.
 * (*yltrie_getref()) is value stored at trie.
 * Sometimes, direct modification of reference is required
 * (Especially to avoid duplicated 'Trie-Seaching')
 */
extern void**
yltrie_getref(yltrie_t*, const char* sym);

/**
 * walking order is dictionary order of symbol.
 * @return:
 *    1 : success. End of trie.
 *    0 : stop by user callback
 *   -1 : fail. (ex. @from is invalid prefix of symbol.)
 */
extern int
yltrie_walk(yltrie_t*, void* user, const char* from,
            /* return : b_keepgoing => return 1 for keep going, 0 for stop and don't do anymore*/
            int(cb)(void* user, const char* sym, void* v));

/**
 * @cmp    : compare function of trie data. this should return 1 if value is same, otherwise 0.
 * @return : 1 if same, otherwise 0.
 */
extern int
yltrie_equal(const yltrie_t* t0, const yltrie_t* t1,
             int(*cmp)(const void*, const void*));

/**
 * clone trie.
 * @clonev   : return cloned value.
 */
extern yltrie_t*
yltrie_clone(const yltrie_t* t, void* user,
             void*(*clonev)(void*/*user*/, const void*/*src*/));


/**
 * copy trie.
 * @dst    : old data will be freed before copying.
 * @clonev : return cloned value.
 */
extern int
yltrie_copy(yltrie_t* dst, const yltrie_t* src, void* user,
            void*(*clonev)(void*/*user*/, const void*/*src*/));

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
extern int
yltrie_auto_complete(yltrie_t* t, 
                     const char* start_with, 
                     char* buf, unsigned int bufsz);


#endif /* ___YLTRIe_h___ */
