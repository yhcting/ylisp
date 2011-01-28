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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include <stdarg.h>
#include <assert.h>

#include "ylisp.h"
#include "ylut.h"

#define _LOGLV YLLogW

static void
_log(int lv, const char* format, ...) {
    if(lv >= _LOGLV) {
        va_list ap;
        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);
    }
}

static void
_assert(int a) {
    if(!a){ assert(0); }
}

int
main(int argc, char* argv[]) {
    ylsys_t        sys;
    int            i;
    void*          d = NULL;
    unsigned int   dsz;

    /* set system parameter */
    sys.print   = printf;
    sys.log     = _log;
    sys.assert_ = _assert;
    sys.malloc  = malloc;
    sys.free    = free;
    sys.mode    = YLMode_repl;
    sys.mpsz    = 1024*1024;
    sys.gctp    = 80;

    if (YLOk != ylinit(&sys)) {
        printf("Fail to initialize ylisp\n");
        exit(1);
    }

    for (i=1; i<argc; i++) {
        d = ylutfile_read(&dsz, argv[i], 1);
        if (!d && YLOk != dsz) {
            printf("Fail to read file : %s\n", argv[i]);
            exit(1);
        }
        if (YLOk != ylinterpret(d, dsz)) {
            printf("Fail to interpret!\n"
                   "    file : %s\n", argv[i]);
            exit(1);
        }
        free(d);
    }
    return 0;
}
