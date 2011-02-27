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

#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include "yldev.h"

#define _LOGLV  YLLogW

static unsigned int _mblk = 0;

static const char* _exp =
	"(interpret-file '../yls/base.yl)\n"
	"(interpret-file '../test/test_base.yl)\n"
	;


static inline void*
_malloc(size_t size) {
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
	if (lv >= _LOGLV) {
		va_list ap;
		va_start(ap, format);
		vprintf(format, ap);
		va_end(ap);
	}
}

static inline void
_assert_(int a) {
	assert(a);
}

#define NFUNC(n, s, type, desc) extern YLDECLNF(n);
#       include "nfunc.in"
#undef NFUNC

int
main(int argc, char* argv[]) {
	ylsys_t   sys;

	/* set system parameter */
	sys.print   = &printf;
	sys.log     = &_log;
	sys.assert_ = &_assert_;
	sys.malloc  = &_malloc;
	sys.free    = &_free;
	sys.mode    = YLMode_batch;
	sys.mpsz    = 4*1024;
	sys.gctp    = 80;

	ylinit(&sys);

#define NFUNC(n, s, type, desc)						\
	if (YLOk != ylregister_nfunc(YLDEV_VERSION ,s, YLNFN(n), type, desc)) \
		return 0;
#       include "nfunc.in"
#undef NFUNC

	if (YLOk != ylinterpret((unsigned char*)_exp,
				(unsigned int)strlen(_exp)))
		return 0;

	yldeinit();
	assert(0 == _get_mblk_size());

	printf("---------------------------\n"
	       "Test Success\n");
	return 0;
}
