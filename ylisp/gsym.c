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
 *    along with this program.	If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


/*
 * Handle Global Symbols
 */

#include <string.h>
#include "lisp.h"

static slut_t*		 _t = NULL;

/*
 * Global Symbol Trie Should not be corrupted!!
 * This mutex is to secure TRIE structrue's completeness.
 */
static pthread_mutex_t	 _m;

ylerr_t
ylgsym_init() {
	pthread_mutex_init(&_m, ylmutexattr());
	_t = ylslu_create();
	return YLOk;
}

void
ylgsym_deinit() {
	ylslu_destroy(_t);
	pthread_mutex_destroy(&_m);
}

int
ylgsym_insert(const char* sym, int sty, yle_t* e) {
	int    ret;
	_mlock(&_m);
	ret = ylslu_insert(_t, sym, sty, e);
	_munlock(&_m);
	return ret;
}

int
ylgsym_delete(const char* sym) {
	int    ret;
	_mlock(&_m);
	ret = ylslu_delete(_t, sym);
	_munlock(&_m);
	return ret;
}

int
ylgsym_set_description(const char* sym, const char* description) {
	int    ret;
	_mlock(&_m);
	ret = ylslu_set_description(_t, sym, description);
	_munlock(&_m);
	return ret;
}

int
ylgsym_get_description(char* b, unsigned int bsz, const char* sym) {
	const char*  desc;
	unsigned int sz;
	_mlock(&_m);
	desc = ylslu_get_description(_t, sym);
	if (desc) {
		sz = (unsigned int)strlen(desc);
		if (sz >= bsz )
			sz = bsz -1;
		memcpy(b, desc, sz);
		b[sz] = 0; /* add trailing 0 */
		_munlock(&_m);
		return 0;
	} else {
		_munlock(&_m);
		return -1;
	}
}

yle_t*
ylgsym_get(int* outty, const char* sym) {
	yle_t*	  ret;
	_mlock(&_m);
	ret = ylslu_get(_t, outty, sym);
	_munlock(&_m);
	return ret;
}

void
ylgsym_gcmark() {
	/* This is called by only 'GC in Mempool' */
	if (_t) {
		_mlock(&_m);
		ylslu_gcmark(_t);
		_munlock(&_m);
	}
}

int
ylgsym_auto_complete(const char* start_with,
		     char* buf, unsigned int bufsz) {
	int ret;
	_mlock(&_m);
	ret =ylslu_auto_complete(_t, start_with,  buf, bufsz);
	_munlock(&_m);
	return ret;
}

int
ylgsym_nr_candidates(const char* start_with,
		     unsigned int* max_symlen) {
	int    ret;
	_mlock(&_m);
	ret = ylslu_nr_candidates(_t, start_with, max_symlen);
	_munlock(&_m);
	return ret;
}

int
ylgsym_candidates(const char* start_with, char** ppbuf,
		  unsigned int ppbsz,
		  unsigned int pbsz) {
	int	   ret;
	_mlock(&_m);
	ret = ylslu_candidates(_t, start_with, ppbuf, ppbsz, pbsz);
	_munlock(&_m);
	return ret;
}


