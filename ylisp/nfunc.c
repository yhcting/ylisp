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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lisp.h"


/**********************************************************
 * Basic Functions for interpreting
 **********************************************************/

YLDEFNF(__dummy, 0, 0) {
	/* This is just dummy */
	ylnfinterp_fail(
		YLErr_func_fail,
		"'lambda' or 'mlambda' cannot be used as function name!\n");
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
		e0 = ylmp_block();
		e1 = ylmp_block();
		ylpassign(e0, ylnil(), e1);
		ylpassign(e1, e0, ylnil());
	}
#endif
	return ylcar(e);
} YLENDNF(quote)

YLDEFNF(apply, 1, 9999) {
	return ylapply(cxt, ylcar(e), ylcdr(e), a);
} YLENDNF(apply)

/* eq [car [e]; EQ] -> [eval [cadr [e]; a] = eval [caddr [e]; a]] */
YLDEFNF(eq, 2, 2) {
	/* compare memory address */
	return (ylcar(e)==ylcadr(e))? ylt(): ylnil();
} YLENDNF(eq)

YLDEFNF(equal, 2, 2) {
	return (yle_t*)yleq(ylcar(e), ylcadr(e));
} YLENDNF(equal)

YLDEFNF(set, 2, 3) {
	if (pcsz > 2) {
		if (ylais_type(ylcaddr(e), ylaif_sym()))
			return ylset(cxt,
				     ylcar(e),
				     ylcadr(e),
				     a,
				     ylasym(ylcaddr(e)).sym, /* desc */
				     0);
		else
			ylnfinterp_fail(YLErr_func_invalid_param,
"SET : 3rd parameter should be description string\n");

	} else
		return ylset(cxt, ylcar(e), ylcadr(e), a, NULL, 0);
} YLENDNF(set)

YLDEFNF(tset, 2, 3) {
	if (pcsz > 2) {
		if (ylais_type(ylcaddr(e), ylaif_sym()))
			return yltset(cxt,
				      ylcar(e),
				      ylcadr(e),
				      a,
				      ylasym(ylcaddr(e)).sym, /* desc */
				      0);
		else
			ylnfinterp_fail(YLErr_func_invalid_param,
"SET : 3rd parameter should be description string\n");
	} else
		return yltset(cxt,
			      ylcar(e),
			      ylcadr(e),
			      a,
			      NULL,
			      0);
} YLENDNF(tset)

YLDEFNF(f_mset, 2, 3) {
	if (pcsz > 2) {
		if (ylais_type(ylcaddr(e), ylaif_sym()))
			return ylset(cxt,
				     ylcar(e),
				     ylcadr(e),
				     a,
				     ylasym(ylcaddr(e)).sym,
				     STymac);
		else
			ylnfinterp_fail(YLErr_func_invalid_param,
"MSET : 3rd parameter should be description string\n");
	} else
		return ylset(cxt,
			     ylcar(e),
			     ylcadr(e),
			     a,
			     NULL,
			     STymac);
} YLENDNF(f_mset)

YLDEFNF(f_tmset, 2, 3) {
	if (pcsz > 2) {
		if (ylais_type(ylcaddr(e), ylaif_sym()))
			return yltset(cxt,
				      ylcar(e),
				      ylcadr(e),
				      a,
				      ylasym(ylcaddr(e)).sym,
				      STymac);
		else
			ylnfinterp_fail(YLErr_func_invalid_param,
"MSET : 3rd parameter should be description string\n");
	} else
		return yltset(cxt,
			      ylcar(e),
			      ylcadr(e),
			      a,
			      NULL,
			      STymac);
} YLENDNF(f_tmset)

YLDEFNF(unset, 1, 1) {
	ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym()));
	if (0 <= ylgsym_delete(ylasym(ylcar(e)).sym))
		return ylt();
	else
		return ylnil();
} YLENDNF(unset)

YLDEFNF(tunset, 1, 1) {
	ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym()));
	if (0 <= ylslu_delete(cxt->slut, ylasym(ylcar(e)).sym))
		return ylt();
	else
		return ylnil();
} YLENDNF(unset)

YLDEFNF(is_set, 1, 1) {
    int temp;
    ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym()));
    /* check global symbol */
    return (ylgsym_get(&temp,
		       ylasym(ylcar(e)).sym))?
	    ylt():
	    ylnil();
} YLENDNF(is_set)

YLDEFNF(is_tset, 1, 1) {
    int temp;
    ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym()));
    return (ylslu_get(cxt->slut,
		      &temp,
		      ylasym(ylcar(e)).sym))?
	    ylt():
	    ylnil();
} YLENDNF(is_tset)

YLDEFNF(eval, 1, 1) {
	return yleval(cxt, ylcar(e), a);
} YLENDNF(eval)

YLDEFNF(help, 1, 9999) {
#define __MAX_DESC_SZ	4096
	char  desc[__MAX_DESC_SZ];
	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
	while (!yleis_nil(e)) {
		if (0 > ylgsym_get_description(desc,
					       __MAX_DESC_SZ,
					       ylasym(ylcar(e)).sym))
			ylprint("======== %s =========\n"
				"Cannot find symbol\n",
				ylasym(ylcar(e)).sym);
		else {
			int	   outty;
			yle_t*	   v;
			v = ylgsym_get(&outty, ylasym(ylcar(e)).sym);
			ylprint("\n======== %s Desc =========\n"
				"%s\n"
				"-- Value --\n"
				"%s : %s\n"
				, ylasym(ylcar(e)).sym
				, desc
				, styis(outty, STymac)? "M": ""
				, ylechain_print(ylethread_buf(cxt), v));
		}
		e = ylcdr(e);
	}
	return ylt();
#undef __MAX_DESC_SZ
} YLENDNF(help)

YLDEFNF(load_cnf, 1, 1) {
	void*       handle = NULL;
	void      (*register_cnf)(yletcxt_t*);
	const char* fname;

	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	fname = ylasym(ylcar(e)).sym;
	handle = dlopen(fname, RTLD_LAZY);
	if (!handle) {
		ylnflogE("Cannot open custom command library : %s\n",
			 dlerror());
		goto bail;
	}

	register_cnf = dlsym(handle, "ylcnf_onload");
	if (NULL != dlerror()) {
		ylnflogE("Error to get 'ylcnf_onload' : %s\n",
			 dlerror());
		goto bail;
	}

	(*register_cnf)(cxt);

	ylnflogI("done\n");

	/* dlclose(handle); */
	return ylt();

 bail:
	if (handle)
		dlclose(handle);
	ylinterpret_undefined(YLErr_func_fail);

} YLENDNF(load_cnf)

YLDEFNF(unload_cnf, 1, 1) {
	void*   handle = NULL;
	void  (*unregister_cnf)(yletcxt_t*);
	const char* fname;

	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	fname = ylasym(ylcar(e)).sym;
	/*
	 * If the same library is loaded again,
	 *   the same file handle is returned.
	 * (see manpage of dlopen(3))
	 */
	handle = dlopen(fname, RTLD_LAZY);
	if (!handle) {
		ylnflogE("Cannot open custom command library : %s\n",
			 dlerror());
		goto bail;
	}

	unregister_cnf = dlsym(handle, "ylcnf_onunload");
	if (NULL != dlerror()) {
		ylnflogE("Error to get symbol : %s\n",
			 dlerror());
		goto bail;
	}

	(*unregister_cnf)(cxt);

	dlclose(handle);

	ylnflogI("done\n");

	return ylt();

 bail:
	if (handle)
		dlclose(handle);
	ylinterpret_undefined(YLErr_func_fail);
} YLENDNF(unload_cnf)

YLDEFNF(interpret, 1, 1) {
	const char* code;
	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
	code = ylasym(ylcar(e)).sym;
	if (YLOk != ylinterpret_internal(cxt,
					 (unsigned char*) code,
					 (unsigned int) strlen(code))) {
		ylnflogE("ERROR at interpreting given code\n%s\n",
			 code);
		return ylnil();
	} else
		return ylt();
} YLENDNF(interpret)

YLDEFNF(interpret_file, 1, 9999) {
	FILE*            fh = NULL;
	unsigned char*   buf = NULL;
	const char*      fname = NULL; /* file name */
	long int         sz;

	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	while (!yleis_nil(e)) {
		fh = NULL; buf = NULL;
		fname = ylasym(ylcar(e)).sym;

		fh = fopen(fname, "r");
		if (!fh) {
			ylnflogE("Cannot open lisp file [%s]\n", fname);
			goto bail;
		}

		/* do not check error.. very rare to fail!! */
		fseek(fh, 0, SEEK_END);
		sz = ftell(fh);
		fseek(fh, 0, SEEK_SET);

		buf = ylmalloc(sz);
		if (!buf) {
			ylnflogE("Not enough memory to load file [%s]\n",
				 fname);
			goto bail;
		}

		if (sz != fread(buf, 1, sz, fh)) {
			ylnflogE("Fail to read file [%s]\n",
				 fname);
			goto bail;
		}

		if (YLOk != ylinterpret_internal(cxt, buf, sz)) {
			ylnflogE("ERROR at interpreting [%s]\n",
				 fname);
			goto bail;
		}

		if (fh)
			fclose(fh);
		if (buf)
			ylfree(buf);

		ylnflogI("interpret-file: [%s] is done\n",
			 fname);

		e = ylcdr(e);
	}

	return ylt();

 bail:
	if (fh)
		fclose(fh);
	if (buf)
		ylfree(buf);

	ylinterpret_undefined(YLErr_func_fail); /* error during interpreting */
} YLENDNF(interpret_file)

/**********************************************************
 * Functions for managing interpreter internals.
 **********************************************************/
static ylerr_t
_mod_init(void) {
	/* dlopen(0, RTLD_LAZY | RTLD_GLOBAL); */
	return YLOk;
}

static ylerr_t
_mod_exit(void) {
	return YLOk;
}

YLMODULE_INITFN(nfunc, _mod_init)
YLMODULE_EXITFN(nfunc, _mod_exit)
