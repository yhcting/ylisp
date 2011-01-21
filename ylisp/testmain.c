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
#include <memory.h>
#include <string.h>
#include <assert.h>

#include "lisp.h"

#define _LOGLV  YLLogW

#ifdef CONFIG_DBG_MEM
#define _MAX_BLKS    128*1024
static struct {
    void*    caller;
    void*    addr;
} _mdbg[_MAX_BLKS];
#endif /* CONFIG_DBG_MEM */

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
    void* addr = malloc(size);
#ifdef CONFIG_DBG_MEM
    do {
        register void* ra; /* return address */
        asm ("movl 4(%%ebp), %0;"
             :"=r"(ra));
        _mdbg[_mblk].caller = ra;
        _mdbg[_mblk].addr = addr;
    } while(0);
#endif/* CONFIG_DBG_MEM */
    _mblk++;
    return addr;
}

static void
_free(void* p) {
#ifdef CONFIG_DBG_MEM
    do {
        register int i;
        for(i=0; i<_mblk; i++) {
            if(_mdbg[i].addr == p) {
                memmove(&_mdbg[i], &_mdbg[i+1], (_mblk-i-1)*sizeof(_mdbg[i]));
                break;
            }
        }
    } while(0);
#endif /* CONFIG_DBG_MEM */
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


int
main(int argc, char* argv[]) {
    ylsys_t          sys;

    /* ylset system parameter */
    sys.log = _log;
    sys.print = printf;
    sys.assert_ = _assert;
    sys.malloc = _malloc;
    sys.free = _free;
    sys.mode = YLMode_batch;
    sys.mpsz = 64*1024;
    sys.gctp = 80;

    ylinit(&sys);

    if(YLOk != ylinterpret((unsigned char*)_exp, (unsigned int)strlen(_exp))) {
        printf("Fail to interpret...\n");
        return 0;
    }

    yldeinit();
    printf("MBLK : %d\n", get_mblk_size());

#ifdef CONFIG_DBG_MEM
    if(get_mblk_size()) {
        register int i;
        printf("======= Leak!! Callers ========\n");
        for(i=0; i<get_mblk_size(); i++) {
            printf("%p\n", _mdbg[i].caller);
        }
        assert(0); /* fail! memleak at somewhere! */
    }
#else /* CONFIG_DBG_MEM */
    assert(0 == get_mblk_size());
#endif /* CONFIG_DBG_MEM */

    printf("End of Test\n");
    return 0;
}


#endif /* __LISPTEST__ */
