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

#include <signal.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "lisp.h"

/* single element string should be less than */
#define _MAX_SINGLE_ELEM_STR_LEN    4096

ylerr_t
ylinterpret_internal(yletcxt_t* cxt, const unsigned char* stream, unsigned int streamsz) {
    /* Declar space to use */
    ylerr_t                  ret = YLErr_force_stopped;
    pthread_t                thd;
    struct __interpthd_arg   arg;
    int                      line = 1;

    if(!stream || 0==streamsz) { return YLOk; } /* nothing to do */

    arg.cxt = cxt;
    arg.s = stream;
    arg.sz = streamsz;
    arg.line = &line;
    arg.ststk = ylstk_create(0, NULL);
    arg.pestk = ylstk_create(0, NULL);
    arg.bsz = _MAX_SINGLE_ELEM_STR_LEN;
    arg.b = ylmalloc(_MAX_SINGLE_ELEM_STR_LEN);
    if(!arg.b) { ret = YLErr_out_of_memory; goto done; }
    

    if( !(arg.ststk && arg.pestk) ) {
        ret = YLErr_out_of_memory;
        goto done;
    }

    if(pthread_create(&thd, NULL, &ylinterp_automata, &arg)) {
        ylassert(0);
        goto done;
    }

    if(pthread_join(thd, (void**)&ret)) {
        ylassert(0);
        pthread_cancel(thd);
        goto done;
    }

    if(YLOk != ret) {
        ylprint(("Interpret FAILS! : ERROR Line : %d\n", line));
        goto done;
    }

 done:
    if(arg.b)     { ylfree(arg.b); }
    if(arg.ststk) { ylstk_destroy(arg.ststk); }
    if(arg.pestk) { ylstk_destroy(arg.pestk); }
    return ret;
}

ylerr_t
ylinterpret(const unsigned char* stream, unsigned int streamsz) {
    ylerr_t    ret;
    yletcxt_t  cxt;
    ret =  ylinit_thread_context(&cxt);
    if(YLOk != ret) { return ret; }
    ylmp_thread_context_add(&cxt);

    ret = ylinterpret_internal(&cxt, stream, streamsz);

    ylmp_thread_context_rm(&cxt);
    yldeinit_thread_context(&cxt);

    return ret;
}

void
ylforce_stop() {
}

void
ylinterpret_undefined(int reason) {
    pthread_exit((void*)reason);
}

ylerr_t
ylinterp_init() {
    return YLOk;
}

void
ylinterp_deinit() {
}
