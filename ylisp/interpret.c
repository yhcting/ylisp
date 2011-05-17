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


#include <signal.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "lisp.h"

/* single element string should be less than */
#define _MAX_SINGLE_ELEM_STR_LEN    4096

typedef struct _sInterp_req {
	unsigned char*	s;
	unsigned int	sz;
	/* callback to free argment */
	void(*		fcb)(struct _sInterp_req*);
} _interp_req_t;


static inline void
_show_eval_stack(yletcxt_t* cxt) {
	while (ylstk_size(cxt->evalstk))
		ylprint("    %s\n",
			ylechain_print(ylethread_buf(cxt),
				       (yle_t*)ylstk_pop(cxt->evalstk)));
}

ylerr_t
ylinterpret_internal(yletcxt_t* cxt,
		     const unsigned char* stream,
		     unsigned int streamsz) {
	/* this is used for pthread_join. So, declare it as 'void*' */
	void*			 ret = (void*)YLErr_killed;
	pthread_t		 thd;
	struct __interpthd_arg	 arg;
	int			 line = 1;

	if (!stream || 0==streamsz)
		return YLOk; /* nothing to do */

	arg.cxt = cxt;
	arg.s = stream;
	arg.sz = streamsz;
	arg.line = &line;
	arg.ststk = ylstk_create(0, NULL);
	arg.pestk = ylstk_create(0, NULL);
	arg.bsz = _MAX_SINGLE_ELEM_STR_LEN;
	arg.b = ylmalloc(_MAX_SINGLE_ELEM_STR_LEN);
	if (!arg.b) {
		ret = (void*)YLErr_out_of_memory;
		goto done;
	}

	if (!(arg.ststk && arg.pestk)) {
		ret = (void*)YLErr_out_of_memory;
		goto done;
	}

	/*
	 * Why lock?
	 * Newly created thread may be in race condition with current thread.
	 * But, before actual starting of new thread,
	 *   we need to fill some parts of context with newly-create-thread-id.
	 * For this, 'cxt->m' is used at this point
	 *   and at early point of 'ylinterp_automa()'.
	 */
	_mlock(&cxt->m);

	if (pthread_create(&thd, NULL, &ylinterp_automata, &arg)) {
		ylassert(0);
		_munlock(&cxt->m);
		goto done;
	}
	ylstk_push(cxt->thdstk, (void*)thd);

	_munlock(&cxt->m);

	if (pthread_join(thd, &ret)) {
		ylassert(0);
		pthread_cancel(thd);
		goto done;
	}

	/*
	 * 'push' is located at child thread.
	 * NOTE !!
	 * There is no right place to put 'push' in this thread.
	 * Putthing 'push' before 'pthread_create' may cause race condition
	 *   between this thread and newly-created child thread.
	 *  (child thread also may call 'ylinterpret_internal'
	 *     before 'push' is called at this thread.)
	 */
	ylstk_pop(cxt->thdstk);

	if (YLOk != ret) {
		/*
		 * Close all process resources!! Thread is fails!!
		 */
		ylmt_close_all_pres(cxt);
		_show_eval_stack(cxt);
		ylprint("Interpret FAILS! : ERROR Line : %d\n", line);
		goto done;
	} else {
		/*
		 * All process resources should be closed before end of thread!
		 */
		ylassert(!ylmt_nr_pres(cxt));
	}

 done:
	if (arg.b)
		ylfree(arg.b);
	if (arg.ststk)
		ylstk_destroy(arg.ststk);
	if (arg.pestk)
		ylstk_destroy(arg.pestk);
	return (ylerr_t)ret;
}

static void*
_interpret(void* arg) {
	ylerr_t		ret;
	_interp_req_t*	req = (_interp_req_t*)arg;
	yletcxt_t	cxt;
	ret =  ylinit_thread_context(&cxt);
	if (YLOk != ret)
		goto done;

	cxt.stream = req->s;
	cxt.streamsz = req->sz;
	ylmt_add(&cxt);

	ret = ylinterpret_internal(&cxt, cxt.stream, cxt.streamsz);

	ylmt_rm(&cxt);
	ylexit_thread_context(&cxt);

 done:
	if (req->fcb)
		(*req->fcb)(req);
	return (void*)ret;
}

static void
_fcb_interp_req(_interp_req_t* req) {
	ylfree(req->s);
	ylfree(req);
}

ylerr_t
ylinterpret_async(pthread_t* thd,
		  const unsigned char* stream,
		  unsigned int streamsz) {
	_interp_req_t*	req = ylmalloc(sizeof(_interp_req_t));
	/* this rarely fails! */
	ylassert (req);
	req->s = ylmalloc(streamsz);
	if (!req->s) {
		ylfree (req);
		return YLErr_out_of_memory;
	}

	memcpy(req->s, stream, streamsz);
	req->sz = streamsz;
	req->fcb = &_fcb_interp_req;

	if (pthread_create(thd, NULL, &_interpret, req))
		ylassert(0);

	return YLOk;
}

ylerr_t
ylinterpret(const unsigned char* stream, unsigned int streamsz) {
	_interp_req_t req;

	req.s = (unsigned char*)stream;
	req.sz = streamsz;
	req.fcb = NULL; /* DO NOT FREE */

	return (ylerr_t)_interpret(&req);
}

void
ylinterpret_undefined(long reason) {
	pthread_exit((void*)reason);
}

static ylerr_t
_mod_init() {
	return YLOk;
}

static ylerr_t
_mod_exit() {
	return YLOk;
}

YLMODULE_INITFN(nfunc, _mod_init)
YLMODULE_EXITFN(nfunc, _mod_exit)

