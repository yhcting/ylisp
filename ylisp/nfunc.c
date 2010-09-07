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
#include "trie.h"
#include "lisp.h"
#include "mempool.h"


/**********************************************************
 * Basic Functions for interpreting
 **********************************************************/

YLDEFNF(__dummy, 0, 0) {
    /* This is just dummy */
    ylnflogE0("'lambda' or 'mlambda' cannot be used as function name!\n");
    ylinterpret_undefined(YLErr_func_fail);
    return ylnil();
} YLENDNF(__dummy)

YLDEFNF(quote, 1, 1) {
    /* 
     * Following inactive part is code for test GC! 
     * Dangling cyclic-cross-referred!
     */
#if 0
    /* make intentional cross-reference!*/
    {
        yle_t *e0, *e1;
        e0 = ylmp_get_block();
        e1 = ylmp_get_block();
        ylpassign(e0, ylnil(), e1);
        ylpassign(e1, e0, ylnil());
    }
#endif
    return ylcar(e);
} YLENDNF(quote)

YLDEFNF(apply, 1, 9999) {
    return ylapply(ylcar(e), ylcdr(e), a);
} YLENDNF(apply)

YLDEFNF(f_mset, 2, 3) {
    if(pcsz > 2) {
        if(ylais_type(ylcaddr(e), YLASymbol))  {
            return ylmset(ylcar(e), ylcadr(e), ylasym(ylcaddr(e)).sym);
        } else {
            ylnflogE0("MSET : 3rd parameter should be description string\n");
            ylinterpret_undefined(YLErr_func_invalid_param);
        }
    } else {
        return ylmset(ylcar(e), ylcadr(e), NULL);
    }
} YLENDNF(f_mset)

/* eq [car [e]; EQ] -> [eval [cadr [e]; a] = eval [caddr [e]; a]] */
YLDEFNF(eq, 2, 2) {
    return (yle_t*)yleq(ylcar(e), ylcadr(e));
} YLENDNF(eq)

YLDEFNF(set, 2, 3) {
    if(pcsz > 2) {
        if(ylais_type(ylcaddr(e), YLASymbol))  {
            return ylset(ylcar(e), ylcadr(e), a,ylasym(ylcaddr(e)).sym);
        } else {
            ylnflogE0("MSET : 3rd parameter should be description string\n");
            ylinterpret_undefined(YLErr_func_invalid_param);
        }
    } else {
        return ylset(ylcar(e), ylcadr(e), a, NULL);
    }
} YLENDNF(set)

YLDEFNF(unset, 1, 1) {
    ylnfcheck_atype_chain1(e, YLASymbol);
    if(0 <= yltrie_delete(ylasym(ylcar(e)).sym)) { return ylt(); }
    else { return ylnil(); }
} YLENDNF(unset)

YLDEFNF(is_set, 1, 1) {
    ylnfcheck_atype_chain1(e, YLASymbol);
    if(yltrie_get(NULL, ylasym(ylcar(e)).sym)) { return ylt(); }
    else { return ylnil(); }
} YLENDNF(is_set)

YLDEFNF(eval, 1, 1) {
    return yleval(ylcar(e), a);
} YLENDNF(eval)

YLDEFNF(help, 1, 9999) {
    const char*  desc;
    ylnfcheck_atype_chain1(e, YLASymbol);
    while(!yleis_nil(e)) {
        if(desc = yltrie_get_description(ylasym(ylcar(e)).sym)) {
            int    outty;
            yle_t* v;
            v = yltrie_get(&outty, ylasym(ylcar(e)).sym);
            ylprint(("\n======== %s Desc =========\n"
                     "%s\n"
                     "-- Value --\n"
                     "%s\n"
                     , ylasym(ylcar(e)).sym
                     , desc
                     , yleprint(v)));
        } else {
            ylprint(("======== %s =========\n"
                     "Cannot find symbol\n", ylasym(ylcar(e)).sym));
        }
        e = ylcdr(e);
    }
    return ylt();
} YLENDNF(help)

YLDEFNF(load_cnf, 1, 1) {
    void*   handle = NULL;
    void  (*register_cnf)();
    const char*  fname;

    ylnfcheck_atype_chain1(e, YLASymbol);

    fname = ylasym(ylcar(e)).sym;
    handle = dlopen(fname, RTLD_LAZY);
    if(!handle) {
        ylnflogE1("Cannot open custom command library : %s\n", dlerror());
        goto bail;
    }

    register_cnf = dlsym(handle, "ylcnf_onload");
    if(NULL != dlerror()) {
        ylnflogE1("Error to get 'ylcnf_onload' : %s\n", dlerror());
        goto bail;
    }

    (*register_cnf)();
    
    ylnflogI0("done\n");

    /* dlclose(handle); */
    return ylt();

 bail:
    if(handle) { dlclose(handle); }
    ylinterpret_undefined(YLErr_func_fail);
} YLENDNF(load_cnf)

YLDEFNF(unload_cnf, 1, 1) {
    void*   handle = NULL;
    void  (*unregister_cnf)();
    const char* fname;

    ylnfcheck_atype_chain1(e, YLASymbol);

    fname = ylasym(ylcar(e)).sym;
    /* 
     * if the same library is loaded again, the same file handle is returned 
     * (see manpage of dlopen(3))
     */
    handle = dlopen(fname, RTLD_LAZY);
    if(!handle) {
        ylnflogE1("Cannot open custom command library : %s\n", dlerror());
        goto bail;
    }

    unregister_cnf = dlsym(handle, "ylcnf_onunload");
    if(NULL != dlerror()) {
        ylnflogE1("Error to get symbol : %s\n", dlerror());
        goto bail;
    }

    (*unregister_cnf)();

    dlclose(handle);

    ylnflogI0("done\n");

    return ylt();

 bail:
    if(handle) { dlclose(handle); }
    ylinterpret_undefined(YLErr_func_fail);

} YLENDNF(unload_cnf)


/*
 * We can easily implement that this function can handle variable number of paramter.
 * But, considering GC, we restrict number of parameter to one.!
 * (FULL scan GC can be run only at the topmost evaluation!.
 *    So, calling 'interpret-file' serveral times is good in GC's point of view!)
 */
YLDEFNF(interpret_file, 1, 1) {
    FILE*        fh = NULL;
    char*        buf = NULL;
    const char*  fname = NULL; /* file name */
    long int     sz;

    ylnfcheck_atype_chain1(e, YLASymbol);

    while(!yleis_nil(e)) {
        fh = NULL; buf = NULL;
        fname = ylasym(ylcar(e)).sym;

        fh = fopen(fname, "r");
        if(!fh) {
            ylnflogE1("Cannot open lisp file [%s]\n", fname);
            goto bail;
        }

        /* do ylnot check error.. very rare to fail!! */
        fseek(fh, 0, SEEK_END);
        sz = ftell(fh);
        fseek(fh, 0, SEEK_SET);

        buf = ylmalloc((unsigned int)sz);
        if(!buf) {
            ylnflogE1("Not enough memory to load file [%s]\n", fname);
            goto bail;
        }

        if(sz != fread(buf, 1, sz, fh)) {
            ylnflogE1("Fail to read file [%s]\n", fname);
            goto bail;
        }
    
        if(YLOk !=  ylinterpret(buf, sz)) {
            ylnflogE1("ERROR at interpreting [%s]\n", fname);
            goto bail;
        }

        if(fh) { fclose(fh); }
        if(buf) { ylfree(buf); }

        ylnflogI1("interpret-file: [%s] is done\n", fname);

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
    ylprint(("\n=========== Before ============\n"));
    ylmp_print_stat();

    ylmp_scan_gc(YLMP_GCSCAN_FULL); /* start full scan */

    ylprint(("\n=========== After ============\n"));
    ylmp_print_stat();
    return ylt();
} YLENDNF(gc)

YLDEFNF(memstat, 0, 0) {
    ylmp_print_stat();
    return ylt();
} YLENDNF(memstat)


ylerr_t
ylnfunc_init() {
    /* dlopen(0, RTLD_LAZY | RTLD_GLOBAL); */
    return YLOk;
}
