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

/*
 * Handle Global Symbols
 */

#include <string.h>
#include "lisp.h"

typedef struct _value {
    int        ty;     /* this should matches symbol atom type - see 'yle_t.u.a.u.sym.ty' */
    char*      desc;   /* description for this value */
    yle_t*     e;
} _value_t;

/*
 * Globally singleton!!
 */
static char              _dummy_empty_desc = 0;
static yltrie_t*         _trie = NULL;

/*
 * Global Symbol Trie Should not be corrupted!!
 * This mutex is to secure TRIE structrue's completeness.
 */
static pthread_mutex_t   _m;

static inline void
_free_description(char* desc) {
    if(&_dummy_empty_desc != desc && desc) {
        ylfree(desc);
    }
}

static inline _value_t*
_alloc_value(int sty, yle_t* e) {
    _value_t* v = ylmalloc(sizeof(_value_t));
    if(!v) {
        /*
         * if allocing such a small size of memory fails, we can't do anything!
         * Just ASSERT IT!
         */
        ylassert(0);
    }
    v->ty = sty; v->desc = &_dummy_empty_desc;
    v->e = e;
    return v;
}

static inline void
_free_value(_value_t* v) {
    _free_description(v->desc);
    /*
     * we don't need to free 'v->e' explicitly here!
     * unlinking from global is enough!
     */
    ylfree(v);
}

ylerr_t
ylgsym_init() {
    pthread_mutex_init(&_m, ylmutexattr());
    _trie = yltrie_create( (void(*)(void*))&_free_value );
    return YLOk;
}

void
ylgsym_deinit() {
    yltrie_destroy(_trie);
    pthread_mutex_destroy(&_m);
}

int
ylgsym_insert(const char* sym, int sty, yle_t* e) {
    int           ret;
    _value_t*     v;
    unsigned int  slen;
    _value_t*     ov;
    v = _alloc_value(sty, e);
    slen = strlen(sym);
    _mlock(&_m);
    ov = yltrie_get(_trie, (unsigned char*)sym, slen);
    if(ov) {
        /*
         * in case of overwritten, description should be preserved.
         * If this twice-search is bottle-neck of performance,
         *  we can reduce overhead by setting value directly to the existing 'ov'.
         * By doing this, we can recduce cost
         * by saving 1-trie-seach and 1-freeing-trie-node.
         * (But in my opinion, it's not big deal!
         *  We whould better not to write hard-to-read-code.)
         */
        v->desc = ov->desc;
        /* to prevent from freeing inside trie! - HACK */
        ov->desc = &_dummy_empty_desc;
    }
    ret = yltrie_insert(_trie, (unsigned char*)sym, slen, (void*)v);
    _munlock(&_m);
    
    return ret;
}

int
ylgsym_delete(const char* sym) {
    int ret;
    _mlock(&_m);
    ret = yltrie_delete(_trie, (unsigned char*)sym, strlen(sym));
    _munlock(&_m);
    return ret;
}

int
ylgsym_set_description(const char* sym, const char* description) {
    _value_t* v;
    _mlock(&_m);
    v = yltrie_get(_trie, (unsigned char*)sym, strlen(sym));
    if(v) {
        unsigned int sz;
        char*        desc;
        if(description) {
            sz = strlen(description);
            desc = ylmalloc(sz+1);
            if(!desc) { ylassert(0); }
            memcpy(desc, description, sz);
            desc[sz] = 0; /* trailing 0 */
        } else {
            /* NULL description */
            desc = &_dummy_empty_desc;
        }

        _free_description(v->desc);
        v->desc = desc;
        _munlock(&_m);
        return 0;
    } else {
        _munlock(&_m);
        return -1;
    }
}

const char*
ylgsym_get_description(const char* sym) {
    _value_t* v;
    _mlock(&_m);
    v = yltrie_get(_trie, (unsigned char*)sym, strlen(sym));
    _munlock(&_m);
    return v? v->desc: NULL;
}

yle_t*
ylgsym_get(int* outty, const char* sym) {
    _value_t* v;
    _mlock(&_m);
    v = yltrie_get(_trie, (unsigned char*)sym, strlen(sym));
    _munlock(&_m);
    if(v) {
        if(outty) { *outty = v->ty; }
        return v->e;
    } else {
        return NULL;
    }
}

_DEF_VISIT_FUNC(static, _gcmark, ,!yleis_gcmark(e), yleset_gcmark(e))

static int
_cb_gcmark(void* user,
           const unsigned char* key, unsigned int sz,
           _value_t* v) {
    if(v->e) { _gcmark(NULL, v->e); }
    return 1;
}

void
ylgsym_gcmark() {
    /* This is called by only 'GC in Mempool' */
    if(_trie) {
        _mlock(&_m);
        yltrie_walk(_trie, NULL, (unsigned char*)"", 0,
                    (int(*)(void*, const unsigned char*,
                            unsigned int, void*))&_cb_gcmark);
        _munlock(&_m);
    }
}

int
ylgsym_auto_complete(const char* start_with,
                     char* buf, unsigned int bufsz) {
    int ret;
    _munlock(&_m);
    ret =yltrie_auto_complete(_trie, (unsigned char*)start_with,
                              (unsigned int)strlen(start_with),
                              (unsigned char*)buf, bufsz);
    _munlock(&_m);
    return ret;
}

typedef struct {
    unsigned int    cnt;
    unsigned int    maxlen;
} _candidates_sz_t;

static int
_cb_nr_candidates(void* user, const unsigned char* key,
                  unsigned int sz,_value_t* v) {
    _candidates_sz_t* st = (_candidates_sz_t*)user;
    st->cnt++;
    if(st->maxlen < sz) { st->maxlen = sz; }
    return 1;
}

int
ylgsym_nr_candidates(const char* start_with,
                     unsigned int* max_symlen) {
    _candidates_sz_t   st;
    st.cnt = st.maxlen = 0;

    _mlock(&_m);
    if(0 > yltrie_walk(_trie, &st, (unsigned char*)start_with,
                       (unsigned int)strlen(start_with),
                       (int(*)(void*, const unsigned char*,
                               unsigned int, void*))&_cb_nr_candidates)) {
        _munlock(&_m);
        return 0;
    }
    _munlock(&_m);

    if(max_symlen) { *max_symlen = st.maxlen; }
    return st.cnt;
}

typedef struct {
    char**         ppbuf;
    unsigned int   ppbsz;
    unsigned int   i;
} _candidates_t;


static int
_cb_candidates(void* user, const unsigned char* key,
               unsigned int sz, _value_t* v) {
    _candidates_t* st = (_candidates_t*)user;
    if(st->i >= st->ppbsz) { return 0; } /* we should stop here! */
    memcpy(st->ppbuf[st->i], key, sz);
    st->ppbuf[st->i][sz] = 0; /* add trailing 0 */
    st->i++;
    return 1; /* keep going */
}

int
ylgsym_candidates(const char* start_with, char** ppbuf,
                  unsigned int ppbsz,
                  unsigned int pbsz) {
    _candidates_t  st;
    st.ppbuf = ppbuf;
    st.ppbsz = ppbsz;
    st.i = 0;

    _mlock(&_m);
    if(0 > yltrie_walk(_trie, &st, (unsigned char*)start_with,
                       (unsigned int)strlen(start_with),
                       (int(*)(void*, const unsigned char*,
                               unsigned int, void*))&_cb_candidates)) {
        _munlock(&_m);
        return -1;
    }
    _munlock(&_m);
    return st.i;
}


