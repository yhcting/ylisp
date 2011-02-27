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


#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>

/*
 * To support multi-threading in trie!
 */
#include <unistd.h>
#include <pthread.h>
#include "ylsfunc.h"
#include "ylut.h"

/* walkaround for android ndk platform 9 */
#ifdef ANDROID_NDK
#       define pthread_rwlock_t       pthread_mutex_t
#       define pthread_rwlock_init    pthread_mutex_init
#       define pthread_rwlock_destroy pthread_mutex_destroy
#       define pthread_rwlock_rdlock  pthread_mutex_lock
#       define pthread_rwlock_wrlock  pthread_mutex_lock
#       define pthread_rwlock_unlock  pthread_mutex_unlock
#endif /* ANDROID_NDK */

#define _arr(e) ((_err_t*)ylacd(e))

typedef struct {
	yle_t**            arr;
	unsigned int       sz;
	pthread_rwlock_t   m;
} _earr_t;

static inline void
_destroy_arr_data(_earr_t* at) {
	ylassert(at);
	/* unlinking is enough! */
	if (at->arr)
		ylfree(at->arr);
	/* see comments at '_atrie_destroy' in 'nfunc_trie.c' */
	pthread_rwlock_destroy(&at->m);
	ylfree(at);
}

static int
_aif_arr_eq(const yle_t* e0, const yle_t* e1) {
	_earr_t* at0 = ylacd(e0);
	_earr_t* at1 = ylacd(e1);

	if (!at0 && !at1)
		return 1;
	else if (at0 && at1) {
		int i, ret = 1;

		pthread_rwlock_rdlock(&at0->m);
		pthread_rwlock_rdlock(&at1->m);
		if (at0->sz != at1->sz)
			ret = 0;
		else {
			for (i=0; i<at0->sz; i++)
				if (yleis_nil(yleq(at0->arr[i], at1->arr[i])))
					ret = 0;
		}
		pthread_rwlock_unlock(&at1->m);
		pthread_rwlock_unlock(&at0->m);

		return ret;
	} else
		return 0;
}

#if 0 /* Keep it for future use! */
static int
_aif_arr_copy(void* map, yle_t* n, const yle_t* e) {
	_earr_t*    sat = ylacd(e);
	_earr_t*    at = NULL;
	if (sat) {
		int i;
		at = ylmalloc(sizeof(_earr_t));
		if (!at)
			goto oom;

		at->sz = sat->sz;
		at->arr = ylmalloc(sizeof(*at->arr) * at->sz);
		if (!at->arr)
			goto oom;
		memset(at->arr, 0, sizeof(*at->arr) * at->sz);
		for (i=0; i<sat->sz; i++)
			if (sat->arr[i])
				at->arr[i] = ylechain_clone(sat->arr[i]);
	}
	ylacd(n) = at;
	return 0;

 oom:
	if (at)
		_destroy_arr_data(at);
	return -1;
}
#endif /* Keep it for future use! */

static int
_aif_arr_to_string(const yle_t* e, char* b, unsigned int sz) {
	/*
	 * recursive printing by using 'ylechain_print' is impossible...
	 * (ylechain_print uses static buffer..
	 *    and this function may be re-enterred by recursive printing!
	 * atom should print it's own data... not containing reference!
	 */
	_earr_t* at = ylacd(e);
	int      bw;

	pthread_rwlock_rdlock(&at->m);
	bw = snprintf(b, sz, ">>ARR:%d<<", at->sz);
	pthread_rwlock_unlock(&at->m);

	if (bw >= sz)
		return -1; /* not enough buffer */
	return bw;
}

static int
_aif_arr_visit(yle_t* e, void* user, int(*cb)(void*, yle_t*)) {
	_earr_t* at = ylacd(e);
	if (at) {
		int    i;
		/* see comments at '_aif_trie_visit' in 'nfunc_trie.c' */
		pthread_rwlock_rdlock(&at->m);
		for (i=0; i<at->sz; i++)
			if (at->arr[i])
				(*cb)(user, at->arr[i]);
		pthread_rwlock_unlock(&at->m);
	}
	return 0;
}

static void
_aif_arr_clean(yle_t* e) {
	/* see comments at '_aif_trie_clean' in 'nfunc_trie.c' */
	_destroy_arr_data(ylacd(e));
	ylacd(e) = NULL; /* <- I'm not sure that this is essential or not! */
}

static ylatomif_t _aif_arr = {
	&_aif_arr_eq,
	NULL,
	&_aif_arr_to_string,
	&_aif_arr_visit,
	&_aif_arr_clean
};

static inline int
_is_arr_type(const yle_t* e) {
	return &_aif_arr == ylaif(e);
}

/*
 * return <0 if fails (Usually OOM)
 */
static int
_arr_alloc(yle_t* ar, yle_t* e) {
	int         i;
	_earr_t*    at;

	ylassert(!yleis_nil(e));

	at = ylmalloc(sizeof(_earr_t));
	if (!at)
		goto oom;

	pthread_rwlock_init(&at->m, NULL);

	/* only integer is allowed */
	at->sz = (unsigned int)(yladbl(ylcar(e)));
	if (at->sz > 0) {
		at->arr = ylmalloc(sizeof(*at->arr) * at->sz);
		if (!at->arr)
			goto oom;
		memset(at->arr, 0, sizeof(*at->arr) * at->sz);
	} else
		at->arr = NULL;

	/* move to next dimension */
	e = ylcdr(e);
	for (i=0; i<at->sz; i++) {
		if (yleis_nil(e))
			at->arr[i] = ylnil();
		else {
			at->arr[i] = ylacreate_cust(&_aif_arr, NULL);
			if (0 > _arr_alloc(at->arr[i], e))
				goto oom;
		}
	}

	ylacd(ar) = at;
	return 0;

 oom:
	/* clean */
	if (at)
		_destroy_arr_data(at);
	return -1;
}

#define _check_dim_param(e)                                             \
	ylelist_foreach(e) {						\
		ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_dbl())	\
				    && (long long)(yladbl(ylcar(e)))	\
				                    == yladbl(ylcar(e))	\
				    && yladbl(ylcar(e)) >= 0);		\
	}


YLDEFNF(make_array, 1, 9999) {
	yle_t*      r = NULL;

	r = e;
	_check_dim_param(r);

	r = ylacreate_cust(&_aif_arr, NULL);
	if (0 > _arr_alloc(r, e))
		goto oom;

	return r;

 oom:
	ylnfinterp_fail(YLErr_out_of_memory, "Out Of Memory\n");
} YLENDNF(make_array)


  /*
   * Get array value pointer of given index!
   * return NULL if fails
   */
static yle_t**
_arr_get(yle_t* e, yle_t* ie, int (*lock)(pthread_rwlock_t*)) {
	const _earr_t*  at;
	yle_t**         ret = NULL;
	long long       i = yladbl(ylcar(ie));
	at = ylacd(e);

	if (0 > i || at->sz <= i) {
		yllogE("Array Access - Out Of Bound\n");
		return NULL;
	}

	if (yleis_nil(ylcdr(ie)))
		/* this is last dim-index */
		ret =  &at->arr[i];
	else {
		yle_t*  ne = at->arr[i]; /* next e */
		if (_is_arr_type(ne) && ylacd(ne)) {
			lock( &((_earr_t*)ylacd(ne))->m );
			ret = _arr_get(at->arr[i], ylcdr(ie), lock);
			pthread_rwlock_unlock(&((_earr_t*)ylacd(ne))->m);
		} else
			yllogE("Invalid Array Access\n");
	}

	return ret;
}

YLDEFNF(arr_get, 2, 9999) {
	yle_t**     pv;
	_earr_t*    at;
	ylnfcheck_parameter(_is_arr_type(ylcar(e)) && ylacd(ylcar(e)));
	{ /* Just scope */
		yle_t* w = ylcdr(e);
		_check_dim_param(w);
	}

	at = ylacd( ylcar(e));

	pthread_rwlock_rdlock( &at->m );
	pv = _arr_get(ylcar(e), ylcdr(e), &pthread_rwlock_rdlock);
	pthread_rwlock_unlock( &at->m );

	if (pv)
		return *pv;
	else
		ylinterpret_undefined(YLErr_func_fail);
} YLENDNF(arr_get)


YLDEFNF(arr_set, 3, 9999) {
	yle_t**     pv;
	_earr_t*    at;

	ylnfcheck_parameter(_is_arr_type(ylcar(e)) && ylacd(ylcar(e)));
	{ /* Just scope */
		yle_t* w = ylcddr(e); /* dimension starts from 3rd parameter */
		_check_dim_param(w);
	}

	at = ylacd( ylcar(e));

	pthread_rwlock_wrlock( &at->m );
	pv = _arr_get(ylcar(e), ylcddr(e), pthread_rwlock_wrlock);

	if (pv && *pv) { /* *pv cannot be NULL */
		*pv = ylcadr(e);
		pthread_rwlock_unlock( &at->m );
		return ylcadr(e);
	} else {
		pthread_rwlock_unlock( &at->m );
		ylinterpret_undefined(YLErr_func_fail);
	}
} YLENDNF(arr_set)
