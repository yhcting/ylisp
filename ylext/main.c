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

static unsigned int _mblk = 0;

static const char* _exp =
    "(load-cnf '../lib/libylbase.so)\n"
    "(interpret-file '../yls/base.yl)\n"
    "(interpret-file '../yls/ext.yl)\n"
    "(interpret-file '../test/test_ext.yl)\n"
    ;

static inline void*
_malloc(unsigned int size) {
    _mblk++;
    return malloc(size);
}

static inline void
_free(void* p) {
    assert(_mblk > 0);
    _mblk--;
    free(p);
}

static inline int
_get_mblk_size() {
    return _mblk;
}

static inline void
_log(int lv, const char* format, ...) {
    if(lv >= _LOGLV) {
        va_list ap;
        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);
    }
}

static inline void
_assert(int a) {
    if(!a){ assert(0); }
}

#define NFUNC(n, s, type, desc) extern YLDECLNF(n);
#   include "nfunc.in"
#undef NFUNC


int
main(int argc, char* argv[]) {
    ylsys_t   sys;

    /* set system parameter */
    sys.print = printf;
    sys.log   = _log;
    sys.assert = _assert;
    sys.malloc = _malloc;
    sys.free = _free;
    sys.mpsz = 8*1024;
    sys.gctp = 80;

    ylinit(&sys);

#define NFUNC(n, s, type, desc)  \
    if(YLOk != ylregister_nfunc(YLDEV_VERSION ,s, YLNFN(n), type, desc)) { return 0; }
#   include "nfunc.in"
#undef NFUNC

    if(YLOk != ylinterpret((unsigned char*)_exp, (unsigned int)strlen(_exp))) {
        return 0;
    }

    /* to check memory status */
    yldeinit();

    assert(0 == _get_mblk_size());

    printf("---------------------------\n"
           "Test Success\n");

    return 0;
}

#else /* __YLDBG__ */

#include <dlfcn.h>
#include <string.h>

#define CONFIG_LOG
#define CONFIG_ASSERT

#include "ylsfunc.h"

#define NFUNC(n, s, type, desc) extern YLDECLNF(n);
#   include "nfunc.in"
#undef NFUNC

#define PCRELIB_PATH_SYM "pcrelib-path"

static void*  _libmhandle;
static void*  _pcrelib;

void
ylcnf_onload(yletcxt_t* cxt) {

    /* load math library */
    _libmhandle = dlopen("/usr/lib/libm.so", RTLD_NOW | RTLD_GLOBAL);
    if(!_libmhandle) {
        yllogE0("Cannot open use system library required [/usr/lib/libm.so]\n");
        return;
    }


    { /* Just Scope */
        char*      sym;
        yle_t*     libpath;

        /* make lib path symbol to get pcre-lib path */
        sym = ylmalloc(sizeof(PCRELIB_PATH_SYM));
        strcpy(sym, PCRELIB_PATH_SYM);
        libpath = ylacreate_sym(sym);

        if(ylis_set (cxt, ylnil (), sym)) {
            libpath = yleval(cxt, libpath, ylnil());
            if(!yleis_nil(libpath) && ylais_type(libpath, ylaif_sym()) ) {
                /* if there is pcre, let's use it! */
                _pcrelib = dlopen(ylasym(libpath).sym, RTLD_NOW | RTLD_GLOBAL);
            }
        }
    } /* Just Scope */

    /* return if fail to register */
#define NFUNC(n, s, type, desc)  \
    if(YLOk != ylregister_nfunc(YLDEV_VERSION ,s, YLNFN(n), type, ">> lib: ylext <<\n" desc)) { return; }
#   include "nfunc.in"
#undef NFUNC

    /* if fail to load pcre lib */
    if(!_pcrelib) {
        yllogW0("WARNING!\n"
                "    Fail to load pcre library!.\n"
                "    Set library path to 'string,pcrelib-path' before load-cnf.\n"
                "    ex. (set 'string,pcrelib-path '/usr/local/lib/libpcre.so).\n"
                "    (pcre-relative-cnfs are not loaded!)\n");

        /* functions that uses pcre-lib */
        ylunregister_nfunc("re-match");
        ylunregister_nfunc("re-replace");

    }
}

void
ylcnf_onunload(yletcxt_t* cxt) {
#define NFUNC(n, s, type, desc) ylunregister_nfunc(s);
#   include "nfunc.in"
#undef NFUNC

    /*
     * All functions that uses 'libm.so' SHOULD BE HERE.
     * We don't care of others who uses 'libm.so' out of here!
     * Let's close it!
     */
    dlclose(_libmhandle);

    if(_pcrelib) { /* re is loaded */
        /*
         * All functions that uses 'libpcre.so' SHOULD BE HERE.
         * We don't care of others who uses 'libm.so' out of here!
         * Let's close it!
         */
        dlclose(_pcrelib);
    }
}
#endif /* __YLDBG__ */
