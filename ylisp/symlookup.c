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

/*
 * Handle Global Symbols
 */

#include <string.h>
#include "lisp.h"


struct _value {
	/* this should matches symbol atom type - see 'yle_t.sty' for symbol */
	short      ty;

	/* description for this value */
	char*      desc;

	yle_t*     e;
};

/*
 * Globally singleton!!
 */
static char              _dummy_empty_desc = 0;

static inline void
_free_description(char* desc) {
	if (&_dummy_empty_desc != desc && desc)
		ylfree(desc);
}

static inline struct _value*
_alloc_value(short sty, yle_t* e) {
	struct _value* v = ylmalloc(sizeof(*v));
	if (!v)
		/*
		 * if allocing such a small size of memory fails,
		 *  we can't do anything!
		 * Just ASSERT IT!
		 */
		ylassert(0);

	v->ty = sty; v->desc = &_dummy_empty_desc;
	v->e = e;
	return v;
}

static inline void
_free_value(struct _value* v) {
	_free_description(v->desc);
	/*
	 * we don't need to free 'v->e' explicitly here!
	 * unlinking from global is enough!
	 */
	ylfree(v);
}

slut_t*
ylslu_create(void) {
	return (slut_t*)yltrie_create( (void(*)(void*))&_free_value );
}

void
ylslu_destroy(slut_t* t) {
	yltrie_destroy((yltrie_t*)t);
}

int
ylslu_insert(slut_t* t, const char* sym, short sty, yle_t* e) {
	int              ret;
	struct _value*   v;
	unsigned int     slen;
	struct _value*   ov;
	v = _alloc_value(sty, e);
	slen = strlen(sym);
	ov = yltrie_get((yltrie_t*)t, (unsigned char*)sym, slen);
	if (ov) {
		if (ylais_type (v->e, ylaif_nfunc ())
		    || ylais_type (v->e, ylaif_sfunc ()))
			yllogW(
"Warn : native function symbol is overwrittened\n"
"    -> %s\n",
                               sym
			       );
		/*
		 * in case of overwritten, description should be preserved.
		 * If this twice-search is bottle-neck of performance,
		 *   we can reduce overhead by setting value directly
		 *   to the existing 'ov'.
		 * By doing this, we can recduce cost
		 * by saving 1-trie-seach and 1-freeing-trie-node.
		 * (But in my opinion, it's not big deal!
		 *   We whould better not to write hard-to-read-code.)
		 */
		v->desc = ov->desc;
		/* to prevent from freeing inside trie! - HACK */
		ov->desc = &_dummy_empty_desc;
	}
	ret = yltrie_insert((yltrie_t*)t, (unsigned char*)sym, slen, (void*)v);

	return ret;
}

int
ylslu_delete(slut_t* t, const char* sym) {
	int         ret;
	ret = yltrie_delete((yltrie_t*)t, (unsigned char*)sym, strlen(sym));
	return ret;
}

int
ylslu_set_description(slut_t* t, const char* sym, const char* description) {
	struct _value*     v;
	v = yltrie_get((yltrie_t*)t, (unsigned char*)sym, strlen(sym));
	if (v) {
		unsigned int sz;
		char*        desc;
		if (description) {
			sz = strlen(description);
			desc = ylmalloc(sz+1);
			if (!desc)
				ylassert(0);
			memcpy(desc, description, sz);
			desc[sz] = 0; /* trailing 0 */
		} else
			/* NULL description */
			desc = &_dummy_empty_desc;

		_free_description(v->desc);
		v->desc = desc;
		return 0;
	} else
		return -1;
}

const char*
ylslu_get_description(slut_t* t, const char* sym) {
	struct _value*     v;
	v = yltrie_get((yltrie_t*)t, (unsigned char*)sym, strlen(sym));
	return v? v->desc: NULL;
}

yle_t*
ylslu_get(slut_t* t, short* outty, const char* sym) {
	struct _value*     v;
	v = yltrie_get((yltrie_t*)t, (unsigned char*)sym, strlen(sym));
	if (v) {
		if (outty)
			*outty = v->ty;
		return v->e;
	} else
		return NULL;
}

_DEF_VISIT_FUNC(static, _gcmark, ,!yleis_gcmark(e), yleset_gcmark(e))

static int
_cb_gcmark(void* user,
	   const unsigned char* key, unsigned int sz,
	   struct _value* v) {
	if (v->e)
		_gcmark(NULL, v->e);
	return 1;
}

void
ylslu_gcmark(slut_t* t) {
	/* This is called by only 'GC in Mempool' */
	yltrie_full_walk((yltrie_t*)t, NULL, (void*)&_cb_gcmark);
}

int
ylslu_auto_complete(slut_t* t, const char* start_with,
		    char* buf, unsigned int bufsz) {
	int    ret;
	ret =yltrie_auto_complete((yltrie_t*)t,
				  (unsigned char*)start_with,
				  (unsigned int)strlen(start_with),
				  (unsigned char*)buf,
				  bufsz);
	return ret;
}

struct _candidates_sz {
	unsigned int    cnt;
	unsigned int    maxlen;
};

static int
_cb_nr_candidates(void* user, const unsigned char* key,
		  unsigned int sz,struct _value* v) {
	struct _candidates_sz* st = (struct _candidates_sz*)user;
	st->cnt++;
	if (st->maxlen < sz)
		st->maxlen = sz;
	return 1;
}

int
ylslu_nr_candidates(slut_t* t, const char* start_with,
		    unsigned int* max_symlen) {
	struct _candidates_sz    st;
	st.cnt = st.maxlen = 0;

	if (0 > yltrie_walk((yltrie_t*)t,
			    &st,
			    (unsigned char*)start_with,
			    (unsigned int)strlen(start_with),
			    (void*)&_cb_nr_candidates))
		return 0;

	if (max_symlen)
		*max_symlen = st.maxlen;
	return st.cnt;
}

struct _candidates {
    char**	   ppbuf;
    unsigned int   ppbsz;
    unsigned int   i;
};


static int
_cb_candidates(void* user, const unsigned char* key,
	       unsigned int sz, struct _value* v) {
	struct _candidates* st = (struct _candidates*)user;
	if (st->i >= st->ppbsz)
		return 0; /* we should stop here! */
	memcpy(st->ppbuf[st->i], key, sz);
	st->ppbuf[st->i][sz] = 0; /* add trailing 0 */
	st->i++;
	return 1; /* keep going */
}

int
ylslu_candidates(slut_t* t, const char* start_with, char** ppbuf,
		 unsigned int ppbsz,
		 unsigned int pbsz) {
	struct _candidates   st;
	st.ppbuf = ppbuf;
	st.ppbsz = ppbsz;
	st.i = 0;

	if (0 > yltrie_walk((yltrie_t*)t,
			    &st,
			    (unsigned char*)start_with,
			    (unsigned int)strlen(start_with),
			    (void*)&_cb_candidates))
		return -1;

	return st.i;
}


