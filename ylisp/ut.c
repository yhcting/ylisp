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


#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include "ylut.h"
#include "yldev.h"


void*
ylutfile_read(unsigned int* outsz, const char* fpath, int btext) {
    unsigned char*  buf = NULL;
    FILE*           fh = NULL;
    unsigned int    sz;

    ylassert(outsz && fpath);

    fh = fopen(fpath, "r");
    if(!fh) { *outsz = YLErr_io;  goto bail; }

    /* do ylnot check error.. very rare to fail!! */
    fseek(fh, 0, SEEK_END);
    sz = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    if(0 == sz && !btext) {
        buf = NULL;
    } else {
        buf = ylmalloc((unsigned int)sz+((btext)? 1: 0)); /* +1 for trailing 0 */
        if(!buf) { *outsz = YLErr_out_of_memory; goto bail; }
        if(1 != fread(buf, sz, 1, fh)) { *outsz = YLErr_io; goto bail; }
    }

    fclose(fh);

    if(btext) { 
        *outsz = sz+1;
        buf[sz] = 0; /* add trailing 0 */
    } else {
        *outsz = sz;
    }

    /* check case (reading empty file) */
    if(!buf) { *outsz = YLOk; }

    return buf;

 bail:
    if(fh) { fclose(fh); }
    if(buf) { ylfree(buf); }
    return NULL;
}

int
ylutdynb_expand(ylutdynb_t* b) {
    unsigned char* tmp = (unsigned char*)ylmalloc(b->limit*2);
    if(tmp) {
        memcpy(tmp, b->b, b->sz);
        ylfree(b->b);
        b->b = tmp;
        b->limit *= 2;
        return TRUE;
    } else {
        return FALSE;
    }
}

int
ylutdynb_shrink(ylutdynb_t* b, unsigned int sz_to) {
    if( b->limit > sz_to 
        && b->sz < sz_to ) {
        unsigned char* tmp = (unsigned char*)ylmalloc(sz_to);
        if(tmp) {
            ylassert(b->b);
            memcpy(tmp, b->b, b->sz);
            b->b = tmp;
            b->limit = sz_to;
            return TRUE;
        }
    }
    return FALSE;
}

int
ylutdynb_append(ylutdynb_t* b, const unsigned char* d, unsigned int dsz) {
    if( !ylutdynb_secure(b, dsz) ) { return FALSE; }
    memcpy(ylutdynb_ptr(b), d, dsz);
    b->sz += dsz;
    return TRUE;
}

int
ylutstr_append(ylutdynb_t* b, const char* format, ...) {
    va_list       args;
    int           ret = TRUE;
    char*         tmp;
    int           cw = 0, cwsv; /* charactera written */

    va_start (args, format);
    do {
        cwsv = cw;
        cw = vsnprintf (ylutstr_ptr(b), ylutdynb_freesz(b), format, args);
        if( cw >= ylutdynb_freesz(b) ) {
            if( !ylutdynb_expand(b) ) {
                cw = cwsv;
                break;
            }
        } else { break; }
    } while(1);
    /*
     * 'cw' doesn't counts trailing 0. 
     * But, 'b->sz' already counts 1 for tailing 0 at 'ylutstr_init'.
     * So, just adding 'cw' is OK!
     */
    b->sz += cw;
    va_end (args);

    return cw;
}
