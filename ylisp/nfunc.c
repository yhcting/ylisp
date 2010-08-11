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



#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ylsfunc.h"
#include "trie.h"
#include "lisp.h"

/**********************************************************
 * Basic Functions for interpreting
 **********************************************************/

YLDEFNF(__dummy, 0, 0) {
    /* This is just dummy */
    yllogE(("'lambda' or 'mlambda' cannot be used as function name!\n")); 
    ylinterpret_undefined(YLErr_func_fail);
    return ylnil();
} YLENDNF(__dummy)

YLDEFNF(quote, 1, 1) {
    return ylcar(e);
} YLENDNF(quote)

YLDEFNF(apply, 1, 9999) {
    return ylapply(ylcar(e), ylcdr(e), a);
} YLENDNF(apply)

YLDEFNF(mset, 2, 3) {
    if(pcsz > 2) {
        if(ylais_type(ylcaddr(e), YLASymbol))  {
            return ylmset(ylcar(e), ylcadr(e), ylasym(ylcaddr(e)).sym);
        } else {
            yllogE(("MSET : 3rd parameter should be description string\n"));
            ylinterpret_undefined(YLErr_func_invalid_param);
        }
    } else {
        return ylmset(ylcar(e), ylcadr(e), NULL);
    }
} YLENDNF(mset)

/* eq [car [e]; EQ] -> [eval [cadr [e]; a] = eval [caddr [e]; a]] */
YLDEFNF(eq, 2, 2) {
    return (yle_t*)yleq(ylcar(e), ylcadr(e));
} YLENDNF(eq)

YLDEFNF(set, 2, 3) {
    if(pcsz > 2) {
        if(ylais_type(ylcaddr(e), YLASymbol))  {
            return ylset(ylcar(e), ylcadr(e), a,ylasym(ylcaddr(e)).sym);
        } else {
            yllogE(("MSET : 3rd parameter should be description string\n"));
            ylinterpret_undefined(YLErr_func_invalid_param);
        }
    } else {
        return ylset(ylcar(e), ylcadr(e), a, NULL);
    }
} YLENDNF(set)

YLDEFNF(unset, 1, 1) {
    ylcheck_chain_atom_type1(help, e, YLASymbol);
    if(yltrie_delete(ylasym(ylcar(e)).sym)) { return ylt(); }
    else { return ylnil(); }
} YLENDNF(unset)

YLDEFNF(eval, 1, 1) {
    return yleval(ylcar(e), a);
} YLENDNF(eval)

YLDEFNF(help, 1, 9999) {
    const char*  desc;
    ylcheck_chain_atom_type1(help, e, YLASymbol);
    while(!yleis_nil(e)) {
        if(desc = yltrie_get_description(ylasym(ylcar(e)).sym)) {
            int    outty;
            yle_t* v;
            v = yltrie_get(&outty, ylasym(ylcar(e)).sym);
            yllogO(("\n======== %s Desc =========\n"
                    "%s\n"
                    "-- Value --\n"
                    "%s\n"
                    , ylasym(ylcar(e)).sym
                    , desc
                    , yleprint(v)));
        } else {
            yllogO(("======== %s =========\n"
                    "Cannot find symbol\n", ylasym(ylcar(e)).sym));
        }
        e = ylcdr(e);
    }
    return ylt();
} YLENDNF(help)

YLDEFNF(load_cnf, 2, 2) {
    void*   handle = NULL;
    void  (*register_cnf)();
    const char*  fname;

    ylcheck_chain_atom_type1(help, e, YLASymbol);

    fname = ylasym(ylcar(e)).sym;
    handle = dlopen(ylasym(ylcar(e)).sym, RTLD_LAZY);
    if(!handle) {
        yllogE(("Cannot open custom command library [%s]\n"
              "    [%s]\n", ylasym(ylcar(e)).sym, dlerror()));
        goto bail;
    }

    e = ylcdr(e);
    if(!ylais_type(ylcar(e), YLASymbol)) {
        goto bail;
    }
    
    register_cnf = dlsym(handle, ylasym(ylcar(e)).sym);
    if(NULL != dlerror()) {
        yllogE(("Error to get symbol [%s]\n"
              "    [%s]\n", ylasym(ylcar(e)).sym, dlerror()));
        goto bail;
    }

    (*register_cnf)();
    
    yllogI(("load-cnf: [%s / %s] is done\n", fname, ylasym(ylcar(e)).sym));

    /* dlclose(handle); */
    return ylt();

 bail:
    if(handle) { dlclose(handle); }
    ylinterpret_undefined(YLErr_func_fail);
} YLENDNF(load_cnf)

YLDEFNF(unload_cnf, 2, 2) {
    void*   handle = NULL;
    void  (*unregister_cnf)();
    const char* fname;

    ylcheck_chain_atom_type1(help, e, YLASymbol);

    fname = ylasym(ylcar(e)).sym;
    /* 
     * if the same library is loaded again, the same file handle is returned 
     * (see manpage of dlopen(3))
     */
    handle = dlopen(ylasym(ylcar(e)).sym, RTLD_LAZY);
    if(!handle) {
        yllogE(("Cannot open custom command library [%s]\n"
              "    [%s]\n", ylasym(ylcar(e)).sym, dlerror()));
        goto bail;
    }

    e = ylcdr(e);
    if(!ylais_type(ylcar(e), YLASymbol)) {
        goto bail;
    }
    
    unregister_cnf = dlsym(handle, ylasym(ylcar(e)).sym);
    if(NULL != dlerror()) {
        yllogE(("Error to get symbol [%s]\n"
              "    [%s]\n", ylasym(ylcar(e)).sym, dlerror()));
        goto bail;
    }

    (*unregister_cnf)();

    dlclose(handle);

    yllogI(("unload-cnf: [%s / %s] is done\n", fname, ylasym(ylcar(e)).sym));

    return ylt();

 bail:
    if(handle) { dlclose(handle); }
    ylinterpret_undefined(YLErr_func_fail);
} YLENDNF(unload_cnf)

YLDEFNF(interpret_file, 1, 9999) {
    FILE*        fh = NULL;
    char*        buf = NULL;
    const char*  fname = NULL; /* file name */
    long int     sz;

    ylcheck_chain_atom_type1(help, e, YLASymbol);

    while(!yleis_nil(e)) {
        fh = NULL; buf = NULL;
        fname = ylasym(ylcar(e)).sym;

        fh = fopen(fname, "r");
        if(!fh) {
            yllogE(("<!interpret-file!> Cannot open lisp file [%s]\n", fname));
            goto bail;
        }

        /* do ylnot check error.. very rare to fail!! */
        fseek(fh, 0, SEEK_END);
        sz = ftell(fh);
        fseek(fh, 0, SEEK_SET);

        buf = ylmalloc((unsigned int)sz);
        if(!buf) {
            yllogE(("<!interpret-file!> Not enough memory to load file [%s]\n", fname));
            goto bail;
        }

        if(sz != fread(buf, 1, sz, fh)) {
            yllogE(("<!interpret-file!> Fail to read file [%s]\n", fname));
            goto bail;
        }
    
        if(YLOk !=  ylinterpret(buf, sz)) {
            yllogE(("<!interpret-file!> ERROR at interpreting\n"
                    "    => %s\n", fname));
            goto bail;
        }

        if(fh) { fclose(fh); }
        if(buf) { ylfree(buf); }

        yllogI(("interpret-file: [%s] is done\n", fname));

        e = ylcdr(e);
    }

    return ylt();

 bail:
    if(fh) { fclose(fh); }
    if(buf) { ylfree(buf); }

    ylinterpret_undefined(YLErr_func_fail); /* error during interpreting */
    
} YLENDNF(interpret_file)

/**********************************************************
 * Functions for managing interpreter internals.
 **********************************************************/
YLDEFNF(gc, 0, 0) {
    yllogp(YLLog_output, ("\n=========== Before ============\n"));
    ylmp_print_stat(YLLog_output);

    ylmp_full_scan_gc();

    yllogp(YLLog_output, ("\n=========== After ============\n"));
    ylmp_print_stat(YLLog_output);
    return ylt();
} YLENDNF(gc)

YLDEFNF(memstat, 0, 0) {
    ylmp_print_stat(YLLog_output);
    return ylt();
} YLENDNF(memstat)


void
ylnfunc_init() {
    /* dlopen(0, RTLD_LAZY | RTLD_GLOBAL); */
}
