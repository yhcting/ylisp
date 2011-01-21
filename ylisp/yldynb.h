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
 * Handling dynamic buffer
 *
 * This is just HELPER DATA STRUCTURE for YLISP
 *================================*/

#ifndef ___YLDYNb_h___
#define ___YLDYNb_h___

#include <memory.h>
#include "yldev.h"

/* DYNmaic Buffer */
typedef struct yldynb {
    unsigned int    limit;
    unsigned int    sz;
    unsigned char*  b;
} yldynb_t;

static inline unsigned int
yldynb_limit(const yldynb_t* b) {
    return b->limit;
}

static inline unsigned int
yldynb_freesz(const yldynb_t* b) {
    return b->limit - b->sz;
}

static inline unsigned int
yldynb_sz(const yldynb_t* b) {
    return b->sz;
}

static inline unsigned char*
yldynb_buf(const yldynb_t* b) {
    return b->b;
}

static inline unsigned char*
yldynb_ptr(const yldynb_t* b) {
    return b->b + b->sz;
}

/*
 * @return: 0 if success.
 */
static inline int
yldynb_init(yldynb_t* b, unsigned int init_limit) {
    b->sz = 0;
    b->b = (unsigned char*)ylmalloc(init_limit);
    if(b->b) { b->limit = init_limit; return 0; }
    else { b->limit = 0; return -1; }
}

static inline void
yldynb_reset(yldynb_t* b) {
    b->sz = 0;
}

static inline void
yldynb_clean(yldynb_t* b) {
    if(b->b) { ylfree(b->b); }
    b->b = NULL;
    b->limit = b->sz = 0;
}

/*
 * increase buffer size by two times.
 * due to using memcpy, it cannot be static inline
 * @return: <0 if fails.
 */
static inline int
yldynb_expand(yldynb_t* b) {
    unsigned char* tmp = (unsigned char*)ylmalloc(b->limit*2);
    if(tmp) {
        memcpy(tmp, b->b, b->sz);
        ylfree(b->b);
        b->b = tmp;
        b->limit *= 2;
        return 0;
    } else {
        return -1;
    }
}

static inline int
yldynb_secure(yldynb_t* b, unsigned int sz_required) {
    while( sz_required > yldynb_freesz(b)
           && !yldynb_expand(b) ) {}
    return sz_required <= yldynb_freesz(b);
}


static inline int
yldynb_shrink(yldynb_t* b, unsigned int sz_to) {
    if( b->limit > sz_to  && b->sz < sz_to ) {
        unsigned char* tmp = (unsigned char*)ylmalloc(sz_to);
        if(tmp) {
            ylassert(b->b);
            memcpy(tmp, b->b, b->sz);
            ylfree(b->b);
            b->b = tmp;
            b->limit = sz_to;
            return 0;
        }
    }
    return -1;
}

static inline int
yldynb_append(yldynb_t* b, const unsigned char* d, unsigned int dsz) {
    if( 0 > yldynb_secure(b, dsz) ) { return -1; }
    memcpy(yldynb_ptr(b), d, dsz);
    b->sz += dsz;
    return 0;
}



/*================================
 *
 * Handling C String (Use dynamic buffer)
 *
 *================================*/
/*
 * @return: <0 means, "this is not string buffer!"
 */
static inline unsigned int
yldynbstr_len(const yldynb_t* b) {
    return b->sz - 1; /* '-1' to exclude trailing 0 */
}

static inline unsigned char*
yldynbstr_ptr(const yldynb_t* b) {
    return b->b + yldynbstr_len(b);
}

static inline unsigned char*
yldynbstr_string(const yldynb_t* b) {
    return b->b;
}

static inline void
yldynbstr_reset(yldynb_t* b) {
    *b->b = 0; /* add trailing 0 */
    b->sz = 1;
}

static inline int
yldynbstr_init(yldynb_t* b, unsigned int init_limit) {
    if(0 <= yldynb_init(b, init_limit+1)) {
        yldynbstr_reset(b);
        return 0;
    }
    return -1;
}

/*
 * @return:
 *    number of bytes appended.
 *    '0' means nothing appended. may be error?
 */
extern int
yldynbstr_append(yldynb_t* b, const char* format, ...);


#endif /* ___YLDYNb_h___ */
