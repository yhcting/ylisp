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
#include <malloc.h>
#include <assert.h>
#include <string.h>

#include "ylisp.h"
#include "lisp.h"
#include "trie.h"

extern void testlisp01();


static int _mblk = 0;

static const char* _exp = 
    "(interpret-file '../yls/test.yl)\n"
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
_assert(int a) {
    if(!a){ assert(0); }
}

int
main(int argc, char* argv[]) {
    ylsys_t   sys;

    /* ylset system parameter */
    sys.loglv = YLLog_verb;
    sys.print = printf;
    sys.assert = _assert;
    sys.malloc = _malloc;
    sys.free = _free;

    ylinit(&sys);

    if(YLOk != ylinterpret(_exp, strlen(_exp))) {
        printf("Fail to interpret...\n");
        return 0;
    }

    yldeinit();
    printf("MBLK : %d\n", get_mblk_size());
    assert(0 == get_mblk_size());

    printf("End of Test\n");
}


#endif /* __LISPTEST__ */
