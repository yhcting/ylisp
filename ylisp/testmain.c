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



#ifdef __LISPTEST__

#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <string.h>

#include "config.h"
#include "ylut.h"

#include <assert.h>

#define _LOGLV  YLLogW

static int _mblk = 0;

static const char* _exp =
    "(interpret-file '../test/test.yl)\n"
    /*
    "'xxxx\n"
    "(set 'x '(a b c))\n"
    "'(a b c)\n"
    "'(a (b c))\n"
    "(quote (a b c))\n"
    "(quote (a (b c)))\n"
    */
    ;


static void*
_malloc(unsigned int size) {
    _mblk++;
    return malloc(size);
}

static void
_free(void* p) {
    _mblk--;
    free(p);
}

int
get_mblk_size() {
    return _mblk;
}

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
    if(!a){
        assert(0);
    }
}


extern void ylmp_gc();

int
main(int argc, char* argv[]) {
    ylsys_t          sys;

    /* ylset system parameter */
    sys.log = _log;
    sys.print = printf;
    sys.assert = _assert;
    sys.malloc = _malloc;
    sys.free = _free;
    sys.mpsz = 64*1024;
    sys.gctp = 80;

    ylinit(&sys);

    if(YLOk != ylinterpret((unsigned char*)_exp, (unsigned int)strlen(_exp))) {
        printf("Fail to interpret...\n");
        return 0;
    }

    yldeinit();
    printf("MBLK : %d\n", get_mblk_size());

    assert(0 == get_mblk_size());

    printf("End of Test\n");
    return 0;
}


#endif /* __LISPTEST__ */
