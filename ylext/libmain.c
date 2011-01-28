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

#include <dlfcn.h>
#include <string.h>
#ifdef CONFIG_DBG_GEN
#   include <signal.h>
#endif

#include "ylsfunc.h"

#define NFUNC(n, s, type, desc) extern YLDECLNF(n);
#   include "nfunc.in"
#undef NFUNC

#ifdef HAVE_LIBPCRE
#   define PCRELIB_PATH_SYM "pcrelib-path"
static void*  _pcrelib;
#endif /* HAVE_LIBPCRE */


static void*  _libmhandle;

#ifdef CONFIG_DBG_GEN

static void
_dbg_sig_handler (int sig) {
    switch (sig) {
        case SIGCHLD:
            yllogI ("SIGCHLD received!\n");
        break;

        case SIGPIPE:
            yllogE ("SIGPIPE received!\n");
            ylassert (0);
        break;
    }
}

#endif /* CONFIG_DBG_GEN */


void
ylcnf_onload(yletcxt_t* cxt) {

#ifdef CONFIG_DBG_GEN
    struct sigaction    act;
    memset (&act, 0, sizeof (act));
    act.sa_handler = &_dbg_sig_handler;

    if (sigaction (SIGCHLD, &act, NULL))
        yllogW ("Fail to set SIGCHLD action. - ignored");

    if (sigaction (SIGPIPE, &act, NULL))
        yllogW ("Fail to set SIGPIPE action. - ignored");
    
#endif /* CONFIG_DBG_GEN */


#ifdef HAVE_LIBM
    /* load math library */
    _libmhandle = dlopen("/usr/lib/libm.so", RTLD_NOW | RTLD_GLOBAL);
    if(!_libmhandle) {
        yllogE ("Cannot open use system library required [/usr/lib/libm.so]\n");
        return;
    }
#endif /* HAVE_LIBM */

#ifdef HAVE_LIBPCRE
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
#endif /* HAVE_LIBPCRE */

    /* return if fail to register */
#define NFUNC(n, s, type, desc)  \
    if(YLOk != ylregister_nfunc(YLDEV_VERSION ,s, YLNFN(n), type, ">> lib: ylext <<\n" desc)) { return; }
#   include "nfunc.in"
#undef NFUNC

#ifdef HAVE_LIBPCRE
    /* if fail to load pcre lib */
    if(!_pcrelib) {
        yllogW ("WARNING!\n"
                "    Fail to load pcre library!.\n"
                "    Set library path to 'string,pcrelib-path' before load-cnf.\n"
                "    ex. (set 'string,pcrelib-path '/usr/local/lib/libpcre.so).\n"
                "    (pcre-relative-cnfs are not loaded!)\n");

        /* functions that uses pcre-lib */
        ylunregister_nfunc("re-match");
        ylunregister_nfunc("re-replace");

    }
#endif /* HAVE_LIBPCRE */
}

void
ylcnf_onunload(yletcxt_t* cxt) {
#define NFUNC(n, s, type, desc) ylunregister_nfunc(s);
#   include "nfunc.in"
#undef NFUNC

#ifdef HAVE_LIBM
    /*
     * All functions that uses 'libm.so' SHOULD BE HERE.
     * We don't care of others who uses 'libm.so' out of here!
     * Let's close it!
     */
    dlclose(_libmhandle);
#endif /* HAVE_LIBM */

#ifdef HAVE_LIBPCRE
    if(_pcrelib) { /* re is loaded */
        /*
         * All functions that uses 'libpcre.so' SHOULD BE HERE.
         * We don't care of others who uses 'libm.so' out of here!
         * Let's close it!
         */
        dlclose(_pcrelib);
    }
#endif /* HAVE_LIBPCRE */
}

