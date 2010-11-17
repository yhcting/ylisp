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
#include <time.h>
#include "lisp.h"

void*
ylutfile_fread(unsigned int* outsz, void* f, int btext) {
    unsigned int    osz;
    unsigned char*  buf = NULL;
    FILE*           fh = (FILE*)f;
    unsigned int    sz;

    /* do not check error.. very rare to fail!! */
    if(0 > fseek(fh, 0, SEEK_END)) { osz = YLErr_io; goto bail; }
    sz = ftell(fh);
    if(0 > sz) { *outsz = YLErr_io; goto bail;  }
    if(0 > fseek(fh, 0, SEEK_SET)) { osz = YLErr_io; goto bail; }

    /* handle special case - empty file */
    if(0 == sz) {
        buf = (btext)? (unsigned char*)ylmalloc(1): NULL;
    } else {
        buf = ylmalloc((unsigned int)sz+((btext)? 1: 0)); /* +1 for trailing 0 */
        if(!buf) { osz = YLErr_out_of_memory; goto bail; }
        if(1 != fread(buf, sz, 1, fh)) { osz = YLErr_io; goto bail; }
    }

    if(btext) {
        osz = sz+1;
        buf[sz] = 0; /* add trailing 0 */
    } else {
        osz = sz;
    }

    /* check case (reading empty file) */
    if(!buf) { osz = YLOk; }
    if(outsz) { *outsz = osz; }

    return buf;

 bail:
    if(buf) { ylfree(buf); }
    return NULL;

}

void*
ylutfile_read(unsigned int* outsz, const char* fpath, int btext) {
    FILE*     fh;
    void*     ret;
    ylassert(outsz && fpath);

    fh = fopen(fpath, "rb");
    if(!fh) {
        if(outsz) { *outsz = YLErr_io; }
        return NULL;
    }
    ret = ylutfile_fread(outsz, fh, btext);
    fclose(fh);
    return ret;
}


int
yldynbstr_append(yldynb_t* b, const char* format, ...) {
    va_list       args;
    int           cw = 0, cwsv; /* charactera written */

    va_start (args, format);
    do {
        cwsv = cw;
        cw = vsnprintf ((char*)yldynbstr_ptr(b), yldynb_freesz(b), format, args);
        ylassert(cw >= 0);
        if( cw >= yldynb_freesz(b) ) {
            if( 0 > yldynb_expand(b) ) {
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
