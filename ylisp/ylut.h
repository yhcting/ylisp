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
 * Understanding these code is far away from understand 'ylisp'.
 * This is OPTIONAL.
 * Utility functions
 */

#ifndef ___YLUt_h___
#define ___YLUt_h___

#include "yldynb.h"


/*================================
 *
 * Handling C String (Use dynamic buffer)
 *
 *================================*/
/*
 * @return: <0 means, "this is not string buffer!"
 */
static inline unsigned int
ylutstr_len(const yldynb_t* b) {
    return b->sz - 1; /* '-1' to exclude trailing 0 */
}

static inline unsigned char*
ylutstr_ptr(const yldynb_t* b) {
    return b->b + ylutstr_len(b);
}

static inline unsigned char*
ylutstr_string(const yldynb_t* b) {
    return b->b;
}

static inline void
ylutstr_reset(yldynb_t* b) {
    *b->b = 0; /* add trailing 0 */
    b->sz = 1;
}

static inline int
ylutstr_init(yldynb_t* b, unsigned int init_limit) {
    if(0 <= yldynb_init(b, init_limit+1)) {
        ylutstr_reset(b);
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
ylutstr_append(yldynb_t* b, const char* format, ...);

/*================================
 *
 * Misc. utilities
 *
 *================================*/

/**
 * CALLER SHOULD FREE returned MEMORY by calling 'free()'
 * @btext:
 *    TRUE : return value is string (includes trailing 0)
 *    FALSE: return value is pure binary data.
 * @outsz:
 *    if success, this includes size of file.
 *    if fails, this includes error cause
 *        YLOk: success.
 *        YLErr_io: file I/O error.
 *        YLErr_out_of_memory: not enough memory
 *
 * @return:
 *    NULL : if fails or size of file is 0.
 *    So, reading empty file returns NULL and *outsz == YLUTREADF_OK
 */
extern void*
ylutfile_read(unsigned int* outsz, const char* fpath, int btext);



#endif /* ___YLUt_h___ */
