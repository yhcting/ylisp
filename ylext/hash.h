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


#ifndef __HASh_h__
#define __HASh_h__

typedef struct _sHash hash_t;
/**
 * @fcb : callback to free user value(item)
 *        (NULL means, item doesn't need to be freed.)
 */
extern hash_t*
hash_create(void(*fcb)(void*));

extern void
hash_destroy(hash_t* h);

/**
 * @return : number of elements in hash.
 */
extern unsigned int
hash_sz(hash_t* h);

/**
 * @v      : user value(item)
 * @return:
 *   -1: error
 *    0: newly added
 *    1: overwritten
 */
extern int
hash_add(hash_t* h,
	 const unsigned char* key, unsigned int keysz,
	 void* v);

/**
 * If these is no matched item, nothing happened.
 * @v      : user value(item)
 * @return:
 *    <0: fail. cannot be found
 *    0 : deleted.
 */
extern int
hash_del(hash_t* h,
	 const unsigned char* key, unsigned int keysz);

/**
 * @return : NULL if fails.
 */
extern void*
hash_get(hash_t* h,
	 const unsigned char* key, unsigned int keysz);

/**
 * walking hash nodes
 * @return:
 *    1 : success. End of hash.
 *    0 : stop by user callback
 *   -1 : error.
 */
extern int
hash_walk(hash_t*, void* user,
	  /* return : 1 for keep going, 0 for stop and don't do anymore*/
	  int (*cb)(void*/*user*/,
		    const unsigned char*/*key*/, unsigned int/*sz*/,
		    void*/*value*/));

#endif /* __HASh_h__ */
