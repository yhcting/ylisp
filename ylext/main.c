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





#ifdef __YLDBG__

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include "yldev.h"

#define _LOGLV  YLLogV

static int _mblk = 0;

static const char* _exp =
    "(load-cnf '../lib/libylbase.so)\n"
    "(interpret-file '../yls/base.yl)\n"
    "(interpret-file '../yls/ext.yl)\n"
    "(interpret-file '../yls/test_ext.yl)\n"
    ;

void*
_malloc(unsigned int size) {
    _mblk++;
    return malloc(size);
}

void
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

void
_assert(int a) {
    if(!a){ assert(0); }
}

#define NFUNC(n, s, type, desc) extern YLDECLNF(n);
#   include "nfunc.in"
#undef NFUNC

/* to check memory status of this library */
extern void ylmp_gc();

int
main(int argc, char* argv[]) {
    ylsys_t   sys;

    /* ylset system parameter */
    sys.print = printf;
    sys.log   = _log;
    sys.assert = _assert;
    sys.malloc = _malloc;
    sys.free = _free;
    sys.mpsz = 8*1024;
    sys.gctp = 80;

    ylinit(&sys);

#define NFUNC(n, s, type, desc)  \
    if(YLOk != ylregister_nfunc(YLDEV_VERSION ,s, YLNFN(n), type, desc)) { return; }
#   include "nfunc.in"
#undef NFUNC

    if(YLOk != ylinterpret(_exp, strlen(_exp))) {
        return;
    }

    /* to check memory status */
    yldeinit();

    assert(0 == get_mblk_size());

    printf("---------------------------\n"
           "Test Success\n");
}

#else /* __YLDBG__ */

#include "yldev.h"

#define NFUNC(n, s, type, desc) extern YLDECLNF(n);
#   include "nfunc.in"
#undef NFUNC

extern void __LNF__libylext__nfunc__INIT__();
extern void __LNF__libylext__nfunc__DeINIT__();

void
ylcnf_onload() {
    /* return if fail to register */
#define NFUNC(n, s, type, desc)  \
    if(YLOk != ylregister_nfunc(YLDEV_VERSION ,s, YLNFN(n), type, ">> lib: ylext <<\n" desc)) { return; }
#   include "nfunc.in"
#undef NFUNC

}

void
ylcnf_onunload() {
#define NFUNC(n, s, type, desc) ylunregister_nfunc(s);
#   include "nfunc.in"
#undef NFUNC
}
#endif /* __YLDBG__ */
