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
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>

#include "ylisp.h"
#include "yllist.h"
#include "yldynb.h"
#include "ylut.h"

#define _LOGLV YLLogW


/* =================================
 * Simple Hash for memory address
 * ================================= */

/* hash node */
struct _hn {
	yllist_link_t    lk;
	const void*      addr;
	const void*      ra;   /* return address */
};

/* 16bit */
static yllist_link_t _mh[0xffff]; /* memory hash */

static void
_mhinit(void) {
	int i;
	for (i=0; i<0xffff; i++)
		yllist_init_link(_mh+i);

}

static void
_mhadd(const void* addr, const void* ra) {
	struct _hn* n = malloc(sizeof(*n));
	n->addr = addr;
	n->ra = ra;
	yllist_add_last(_mh + (0xffff & (unsigned long)addr), &n->lk);
}

static void
_mhdel(const void* addr) {
	struct _hn    *n, *p;
	yllist_link_t* h = _mh + (0xffff & (unsigned long)addr);
	yllist_foreach_item_removal_safe(p, n, h, struct _hn, lk) {
		if (p->addr == addr) {
			yllist_del(&p->lk);
			free(p);
			break;
		}
	}
}

static void
_mhdump(void) {
	yllist_link_t *h, *hend;
	h = _mh;
	hend = h + 0xffff;
	for (;h<hend; h++)
		if (!yllist_is_empty(h)) {
			struct _hn* n;
			yllist_foreach_item(n, h, struct _hn, lk)
				printf("%p, ", n->ra);
			printf("\n");
		}
}

/* =================================
 * To run test script
 * ================================= */

#define _NR_MAX_COLUMN 1024
#define _MAX_SLEEP_MS  20   /* max sleep in miliseconds */

typedef struct {
	yllist_link_t    lk;
	char*            s;
} _strn_t; /* string node */

typedef struct {
	unsigned char*   b;
	unsigned int     sz;
	int              section;
	char*            fname;
	int              bok;
} _interpthd_arg_t;

static pthread_mutex_t _m = PTHREAD_MUTEX_INITIALIZER;
static unsigned int    __tcnt = 0;

static inline void
_free_thdarg(_interpthd_arg_t* arg) {
	free(arg->b);
	free(arg->fname);
	free(arg);
}

static inline void
_inc_tcnt(void) {
	pthread_mutex_lock(&_m);
	__tcnt++;
	pthread_mutex_unlock(&_m);
}

static inline void
_dec_tcnt(void) {
	pthread_mutex_lock(&_m);
	__tcnt--;
	pthread_mutex_unlock(&_m);
}

static inline unsigned int
_tcnt(void) { return __tcnt; }

/* =================================
 * To parse script
 * ================================= */
#define _TESTRS "testrs" /* test script for single thread */


static inline void
_append_rsn(yllist_link_t* hd, char* fname) {
	_strn_t* n = malloc(sizeof(_strn_t));
	n->s = fname;
	yllist_add_last(hd, &n->lk);
}

static inline int
_is_digit(char c) {
	return '0'<=c && c<='9';
}

static inline int
_is_ws(char c) {
	return ' '== c || '\n' == c || '\t' == c;
}

/*
 * returned string should be freed by caller.
 */
static char*
_trim(char* s) {
	char        *p, *ns, *r;
	unsigned int sz;

	/* printf("trim IN \"%s\"\n", s); */
	p = s;
	while (*p && _is_ws(*p)) { p++; }
	if (!*p) { sz = 0; }
	else {
		ns = p;
		p = s + strlen(s) - 1;
		while (p>=s && _is_ws(*p)) { p--; }
		assert(p>=s && p>=ns);
		sz = p-ns+1;
	}

	r = malloc(sz+1);
	r[sz] = 0;/*trailing 0*/
	memcpy(r, ns, sz);
	/* printf("trim OUT \"%s\"\n", r); */
	return r;
}

/*
 * Separator : /={20,}$/
 */
static inline int
_is_separator(const char* s) {
#define _NR_MIN_SEP  20
	const char*  p = s;
	unsigned int nr = 0;
	while (*p && '=' == *p) {
		p++;
		nr++;
	}
	if (nr >= _NR_MIN_SEP && !*p)
		return 1;
	else
		return 0;
#undef _NR_MIN_SEP
}

static inline int
_is_mt_ok(const char* s) {
	if (0 == strcmp(s, "MT OK"))
		return 1;
	else if (0 == strcmp(s, "MT NO"))
		return 0;
	else {
		printf("Script syntax error. MT OK or MT NO is expected.\n");
		exit(1);
	}
}

static inline int
_is_return_ok(const char* s) {
	if (0 == strcmp(s, "OK"))
		return 1;
	else if (0 == strcmp(s, "FAIL"))
		return 0;
	else {
		printf("Script syntax error. OK or FAIL is expected.\n");
		exit(1);
	}
}

static int
_fcmp(const void* e0, const void* e1) {
	return strcmp(*((char**)e0), *((char**)e1));
}

/*
 * 0 : EOF
 * 1 : Not EOF
 */
static int
_read_section(FILE* f, int* bmtok, int* bok, yldynb_t* b) {
#define _ST_MT   0 /* state : get MT OK or NO */
#define _ST_RET  1 /* state : get return */
#define _ST_EXP  2 /* state : get exp */

	char    ln[_NR_MAX_COLUMN];
	char*   tln; /* 'trim'ed line */
	int     state = _ST_MT;
	while (1) {
		if (!fgets(ln, _NR_MAX_COLUMN, f)) {
			if (yldynbstr_len(b) > 0)
				return 1;
			else
				return 0;
		}
		tln = _trim(ln);
		/*
		 * Following two type line is ignored.
		 *    - white space line
		 *    - line starts with ';' (leading WS is ignored)
		 */
		if (!*tln || ';' == *tln) {
			free(tln);
			continue;
		}

		switch (state) {
		case _ST_MT:
			*bmtok = _is_mt_ok(tln);
			state = _ST_RET;
			break;

		case _ST_RET:
			*bok = _is_return_ok(tln);
			state = _ST_EXP;
			break;

		case _ST_EXP:
			if (_is_separator(tln)) {
				free(tln);
				goto done;
			}
			yldynbstr_append(b, "%s", ln);
			break;

		default: assert(0);
		}
		free(tln);
	}
 done:
	return 1;
#undef _ST_EXP
#undef _ST_RET
#undef _ST_MT
}


/* return script file name array (sorted) */
static char**
_testrs_files(unsigned int* nr/*out*/, const char* rsname) {
	yllist_link_t     hd;
	_strn_t          *pos, *n;
	DIR*              dip = NULL;
	struct dirent*    dit;
	char*             p;
	char**            r;
	unsigned int      i;

	yllist_init_link(&hd);

	dip = opendir(".");
	/* '!!' to make compiler be happy. */
	while (!!(dit = readdir(dip))) {
		if (DT_REG == dit->d_type &&
		   0 == memcmp(dit->d_name, rsname, strlen(rsname))) {
			p = dit->d_name + strlen(rsname);
			while (*p && _is_digit(*p)) { p++; }
			if (!*p) {
				/* OK! This is script name */
				p = malloc(strlen(dit->d_name)+1);
				strcpy(p, dit->d_name);
				_append_rsn(&hd, p);
			}
		}
	}
	closedir(dip);

	*nr = yllist_size(&hd);
	r = malloc(sizeof(char*) * *nr);
	i = 0;
	yllist_foreach_item_removal_safe(pos, n, &hd, _strn_t, lk) {
		r[i++] = pos->s;
		free(pos);
	}

	/* we need to sort filename - dictionary order */
	qsort(r, *nr, sizeof(char*), _fcmp);

	return r;
}

static void*
_interp_thread(void* arg) {
	int                ret;
	_interpthd_arg_t*  ia = (_interpthd_arg_t*)arg;
	useconds_t usec;

	/*
	 * To test MT interpreting at random timing
	 */
	usec = (rand() % _MAX_SLEEP_MS) * 1000;
	usleep(usec);

	/* printf("** Interpret[%d] **\n%s\n", bOK, yldynbstr_string(&dyb)); */
	ret = ylinterpret(ia->b, ia->sz);

	if (!((ia->bok && YLOk == ret)
	      || (!ia->bok && YLOk != ret)) ) {
		printf("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"
		       "****        TEST FAILS             *****\n"
		       "* File : %s\n"
		       "* %d-th Section\n"
		       "* Exp\n"
		       "%s\n", ia->fname, ia->section, ia->b);
		exit(1);
	}
	_free_thdarg(ia);
	_dec_tcnt();
	return NULL;
}

static void
_start_test_script(const char* fname, FILE* fh, int bmt) {
	int        bok, bmtok;
	yldynb_t   dyb;
	pthread_t  thd;
	int        seccnt = 0; /* section count */
	_interpthd_arg_t*  targ;
	void*      ret;

	printf("\n\n"
	       "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"
	       "    Runing Test Script : %s\n"
	       "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n"
	       "\n\n\n", fname);
	yldynbstr_init(&dyb, _NR_MAX_COLUMN*8);
	while (_read_section(fh, &bmtok, &bok, &dyb)) {
		seccnt++;
		targ = malloc(sizeof(_interpthd_arg_t));
		targ->b = malloc(yldynb_sz(&dyb));
		targ->sz = yldynb_sz(&dyb);
		memcpy(targ->b, yldynb_buf(&dyb), yldynb_sz(&dyb));
		targ->section = seccnt;
		targ->bok = bok;
		targ->fname = malloc(strlen(fname)+1);
		strcpy(targ->fname, fname);

		_inc_tcnt();
		if (!bmt) {
			/* ST case */
			pthread_create(&thd, NULL, &_interp_thread, targ);
			pthread_join(thd, &ret);
		} else {
			/* MT case */
			if (bmtok)
				pthread_create(&thd, NULL,
					       &_interp_thread, targ);
			else
				_free_thdarg(targ);
		}
		yldynbstr_reset(&dyb);
	}
	yldynb_clean(&dyb);
}


static void
_start_test(const char* rsname, int bmt) {
	char**       pf;
	unsigned int nr;
	int          i;
	FILE*        fh;
	pf = _testrs_files(&nr, rsname);
	for (i=0; i<nr; i++) {
		fh = fopen(pf[i], "r");
		assert(fh);
		_start_test_script(pf[i], fh, bmt);
		fclose(fh);
	}
}

/* =================================
 * To use ylisp interpreter
 * ================================= */


static unsigned int      _mblk = 0;
static pthread_mutex_t   _msys = PTHREAD_MUTEX_INITIALIZER;

void*
_malloc(size_t size) {
	register void* ra; /* return address */
	void*          m;
	m = malloc(size);
#ifdef __LP64__
	/*
	  asm ("movq 8(%%rbp), %0;"
	  :"=r"(ra));
	*/
	ra = NULL;
#else /* __LP64__ */
#   ifdef __i386__
	asm ("movl 4(%%ebp), %0;"
	     :"=r"(ra));
#   endif
#endif /* __LP64__ */
	pthread_mutex_lock(&_msys);
	_mhadd(m, ra);
	_mblk++;
	pthread_mutex_unlock(&_msys);
	return m;
}

void
_free(void* p) {
	pthread_mutex_lock(&_msys);
	assert(_mblk > 0);
	_mhdel(p);
	_mblk--;
	pthread_mutex_unlock(&_msys);
	free(p);
}

static int
_get_mblk_size(void) {
	return _mblk;
}

static void
_log(int lv, const char* format, ...) {
	if (lv >= _LOGLV) {
		va_list ap;
		va_start(ap, format);
		vprintf(format, ap);
		va_end(ap);
	}
}

static void
_assert_(int a) {
	if(!a){ assert(0); }
}


#ifdef CONFIG_STATIC_CNF
extern void ylcnf_load_ylbase(void);
extern void ylcnf_load_ylext(void);
#endif /* CONFIG_STATIC_CNF */

int
main(int argc, char* argv[]) {
	ylsys_t        sys;
	/* 10 min - android device is usually very slow. */
	unsigned int   maxwait = 600;

	_mhinit();

	/* set system parameter */
	sys.print   = &printf;
	sys.log     = &_log;
	sys.assert_ = &_assert_;
	sys.malloc  = &_malloc;
	sys.free    = &_free;
	sys.mode    = YLMode_batch;
	sys.mpsz    = 8*1024;
	sys.gctp    = 1;

	ylinit(&sys);

#ifdef CONFIG_STATIC_CNF
	ylcnf_load_ylbase(NULL);
	ylcnf_load_ylext(NULL);
#endif /* CONFIG_STATIC_CNF */

	srand( time(NULL) );

	printf("\n************ Single Thread Test ************\n");
	/* Test for single thread */
	_start_test(_TESTRS, 0);

	/*
	 * Now, test case is not many.
	 * So, to make code simple, cases are hard-coded!
	 * FIXME: do not hardcode
	 */
	{ /* Just scope */
		double d;
		char   b[100];
		/* Following code should match code in 'test.yl' */
		if (YLOk != ylreadv_dbl("tstdbl_100", &d))
			assert(0);
		assert(100 == d);
		if (YLOk != ylreadv_dbl("tstdbl_67", &d))
			assert(0);
		assert(67 == d);
		if (YLOk != ylreadv_str("tststr_hoho", b, 100))
			assert(0);
		assert(0 == strcmp("hoho", b));
		if (YLOk != ylreadv_str("tststr_haha", b, 100))
			assert(0);
		assert(0 == strcmp("haha", b));
		if (YLOk != ylreadv_bin("tstbin_hoho", (unsigned char*)b, 100))
			assert(0);
		assert(0 == memcmp("hoho", b, sizeof("hoho") - 1));
	}


	printf("\n************ Multi Thread Test *************\n");
	/* Test for multi thread */
	/* _binit_section = 1; *//* HACK */
	_start_test(_TESTRS, 1);

	/* Let's sleep enough to wait all threads are finished! */
	while (_tcnt() && maxwait) {
		sleep(1);
		maxwait--;
	}

	if (!maxwait) {
		printf("Some of interpreting threads are never finished!!\n"
		       "DeadLock???\n");
		return 0;
	}

	ylexit();

	if (_get_mblk_size()) {
		printf("\n"
		       "!!!!!!!!!!!!!!!! ERROR !!!!!!!!!!!!!!!!!!!!\n"
		       "====             TEST FAILS!            ===\n"
		       "====           Leak : count(%d)         ===\n",
		       _get_mblk_size());
		_mhdump();
		sleep(999999999);
		assert(0);
	}

	printf("\n\n\n"
	       "=======================================\n"
	       "!!!!!   Test Success - GOOD JOB   !!!!!\n"
	       "=======================================\n"
	       "\n\n\n");
	return 0;
}
