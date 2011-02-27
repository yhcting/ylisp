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


/*****************************************************
 *
 * NFunctions to support Multi-Threading Features
 *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lisp.h"

#define _MAX_EXPSZ 4096

static int
_ts_cb(void* user, yletcxt_t* cxt) {
	unsigned char* b = (unsigned char*)user;
	unsigned int   mx;
	mx = (_MAX_EXPSZ > cxt->streamsz)? cxt->streamsz: _MAX_EXPSZ-1;
	memcpy(b, cxt->stream, mx);
	b[mx] = 0; /* trailing 0 */
	ylprint("%8x %8x %s\n", cxt->base_id, cxt->sig, b);
	return 1; /* keep going */
}

YLDEFNF(ts, 0, 0) {
	static char buf[_MAX_EXPSZ];
	ylprint("id       sig      exp\n");
	ylmt_walk(cxt, buf, &_ts_cb);
	return ylt();
} YLENDNF(ts)

YLDEFNF(kill, 1, 1) {
	ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_dbl()));
	return (0 > ylmt_kill(cxt, (pthread_t)yladbl(ylcar(e))))?
		ylnil():
		ylt();
} YLENDNF(kill)

YLDEFNF(create_thread, 1, 1) {
	pthread_t    thd;
	ylerr_t      r;
	ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym()));
	r = ylinterpret_async(&thd, (unsigned char*)ylasym(ylcar(e)).sym,
			      (unsigned int)strlen(ylasym(ylcar(e)).sym));
	if (YLOk == r)
		/* change thd into double...to use easily in other place */
		return ylacreate_dbl((double)thd);
	else {
		ylnflogE ("Fail to create interpret thread : %d\n", r);
		return ylnil();
	}
} YLENDNF(create_thread)

