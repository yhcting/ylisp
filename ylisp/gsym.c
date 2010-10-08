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
#include "yltrie.h"
#include "lisp.h"

typedef struct _value {
    int        ty;     /* this should matches symbol atom type - see 'yle_t.u.a.u.sym.ty' */
    char*      desc;   /* description for this value */
    yle_t*     e;
} _value_t;


static char       _dummy_empty_desc = 0;
static yltrie_t*  _trie = NULL;

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
    yleref(e); /* e is reference manually! */
    return v;
}

static inline void
_free_value(_value_t* v) {
    _free_description(v->desc);
    yleunref(v->e);
    ylfree(v);
}

ylerr_t
ylgsym_init() {
    _trie = yltrie_create( (void(*)(void*))&_free_value );
    return YLOk;
}

void
ylgsym_deinit() {
    yltrie_destroy(_trie);
}

int
ylgsym_insert(const char* sym, int sty, yle_t* e) {
    _value_t* v = _alloc_value(sty, e);
    _value_t* ov = yltrie_get(_trie, sym);
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
    return yltrie_insert(_trie, sym, (void*)v);
}

int
ylgsym_delete(const char* sym) {
    return yltrie_delete(_trie, sym);
}

int
ylgsym_set_description(const char* sym, const char* description) {
    _value_t* v = yltrie_get(_trie, sym);
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
        return 0;
    } else {
        return -1;
    }
}

const char*
ylgsym_get_description(const char* sym) {
    _value_t* v = yltrie_get(_trie, sym);
    return v? v->desc: NULL;
}

yle_t*
ylgsym_get(int* outty, const char* sym) {
    _value_t* v = yltrie_get(_trie, sym);
    if(v) {
        if(outty) { *outty = v->ty; }
        return v->e;
    } else {
        return NULL;
    }
}

static void
_mark_chain_reachable(yle_t* e) {
    if(yleis_reachable(e)) {
        /* we don't need to preceed anymore. */
        return;
    }

    yleset_reachable(e);
    if(!yleis_atom(e)) {
        _mark_chain_reachable(ylcar(e));
        _mark_chain_reachable(ylcdr(e));
    }
}

static int
_cb_mark_reachable(void* user, const char* sym, _value_t* v) {
    if(v->e) { _mark_chain_reachable(v->e); }
    return 1;
}

void
ylgsym_mark_reachable() {
    yltrie_walk(_trie, NULL, "", 
                (int(*)(void*, const char*, void*))&_cb_mark_reachable);
}

int
ylgsym_auto_complete(const char* start_with, 
                     char* buf, unsigned int bufsz) {
    return yltrie_auto_complete(_trie, start_with, buf, bufsz);
}

typedef struct {
    unsigned int    cnt;
    unsigned int    maxlen;
} _candidates_sz_t;

static int
_cb_nr_candidates(void* user, const char* sym, _value_t* v) {
    _candidates_sz_t* st = (_candidates_sz_t*)user;
    unsigned int len = strlen(sym);
    st->cnt++;
    if(st->maxlen < len) { st->maxlen = len; }
    return 1;
}

int
ylgsym_nr_candidates(const char* start_with,
                     unsigned int* max_symlen) {
    _candidates_sz_t   st;
    st.cnt = st.maxlen = 0;

    if(0 > yltrie_walk(_trie, &st, start_with, 
                       (int(*)(void*, const char*, void*))&_cb_nr_candidates)) { 
        return 0;
    }

    if(max_symlen) { *max_symlen = st.maxlen; }
    return st.cnt;
}

typedef struct {
    char**         ppbuf;
    unsigned int   ppbsz;
    unsigned int   i;
} _candidates_t;


static int
_cb_candidates(void* user, const char* sym, _value_t* v) {
    _candidates_t* st = (_candidates_t*)user;
    if(st->i >= st->ppbsz) { return 0; } /* we should stop here! */
    strcpy(st->ppbuf[st->i], sym);
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

    if(0 > yltrie_walk(_trie, &st, start_with, 
                       (int(*)(void*, const char*, void*))&_cb_candidates)) {
        return -1;
    }
    return st.i;
}


