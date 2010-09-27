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
 * To store global symbols, Trie is used as an main data structure.
 * But, having 256 pointers for every node is heavy-memory-burden.
 * So, we choose 4-bit-way; One ASCII character(8 bit) is represented by 2 node (4bit + 4bit) - front and back node.
 * We can also get sorted result from Trie.
 * Why Trie is chosen (versus Hash, Tree etc)?
 * Pros
 *    - To search symbol is fast.
 *    - It is efficient(in terms of performance) and easy to implement Auto-Completion.
 * Cons
 *    - Memory overhead.
 *
 * Implementation example
 *
 * ex. symbol '4' has 'DATA'
 *    '4' == 0x34 == 0011 0100b
 * +---+----+--
 * |R  |0000| ^
 * |o  +----+ |
 * |o   ...   | 16 pointers
 * |t  +----+ |      <front node>
 * |   |0011|-|----> +---+----+
 * |   +----+ |      |    ... |
 * |    ...   v      |   +----+      <back node>
 * +---+----+---     |   |0100|----> +---+----+
 *                   |   +----+      | D |0000|
 *                   |    ...        | A +----+
 *                                   | T | ...|
 *                                   | A |    |
 *                                   +---+----+
 */
#include <memory.h>
#include <string.h>
#include "trie.h"

#define _MAX_SYM_LEN  1024

static char _dummy_empty_desc = 0;

typedef struct _value {
    int        ty;     /* this should matches symbol atom type - see 'yle_t.u.a.u.sym.ty' */
    char*      desc;   /* description for this value */
    yle_t*     e;
} _value_t;

/* use 4 bit trie node */
typedef struct _node {
    /*
     * We need two node to represent 1 ASCII character!
     * (If 8bit is used, every node should have 256 pointers. And this is memory-burden.
     *  4bits-way (16 pointers-way) is good choice, in my opinion.)
     */
    struct _node*    n[16];
    _value_t*        v;
} _node_t;

static struct {
    _node_t          rt;   /* root. sentinel */
    unsigned int     sz;   /* size of trie - number of symbols that is added. */
} _trie;

static inline void
_free_description(char* desc) {
    if(&_dummy_empty_desc != desc && desc) {
        ylfree(desc);
    }
}

static inline _value_t*
_alloc_trie_value(int sty, yle_t* e) {
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
_free_trie_value(_value_t* v) {
    _free_description(v->desc);
    yleunref(v->e);
    ylfree(v);
}

static inline _node_t*
_alloc_trie_node() {
    _node_t* n = ylmalloc(sizeof(_node_t));
    if(!n) { 
        /* 
         * if allocing such a small size of memory fails, we can't do anything!
         * Just ASSERT IT!
         */
        ylassert(0); 
    }
    memset(n, 0, sizeof(_node_t));
    return n;
}

static inline void
_free_trie_node(_node_t* n) {
    if(n->v) { _free_trie_value(n->v); }
    ylfree(n);
}


/**
 * @c:     (char)     character
 * @n:     (_node_t*) node to move down
 * @fi:    (int)      variable to store front index
 * @bi:    (int)      variable to store back index
 * @felse: <code>     code in 'else' brace of front node
 * @belse: <code>     code in 'else' brace of back node
 */
#define _move_down_node(c, n, fi, bi, felse, belse)     \
    do {                                                \
        (fi) = (c)>>4; (bi) = (c)&0x0f;                 \
        if((n)->n[fi]) { (n) = (n)->n[fi]; }            \
        else { felse; }                                 \
        if((n)->n[bi]) { (n) = (n)->n[bi]; }            \
        else { belse; }                                 \
    } while(0)
/*
 * if node exists, it returns node. If not, it creates new one and returns it.
 * @return: back node of last character of symbol. If fails to get, NULL is returned.
 */
static _node_t*
_get_node(const unsigned char* sym, int bcreate) {
    register _node_t*             n = &_trie.rt;
    register const unsigned char* p = sym;
    register int                  fi, bi; /* front index */

    if(bcreate) {
        while(*p) {
            _move_down_node(*p, n, fi, bi, 
                            n = n->n[fi] = _alloc_trie_node(),
                            n = n->n[bi] = _alloc_trie_node());
            p++;
        }
    } else {
        while(*p) {
            _move_down_node(*p, n, fi, bi, 
                            return NULL, return NULL);
            p++;
        }
    }
    return n;
}

/*
 * @return: see '_walk_node'
 */
static int
_walk_node_internal(void* user, _node_t* n,
                    int(cb)(void* user, const char* sym, _value_t* v),
                    unsigned char* buf, unsigned int bsz, /* bsz: excluding space for trailing 0 */
                    unsigned int bitoffset) {
    register unsigned int i;

    if(n->v) { 
        /* round-up base on 8. And add trailing 0 */
        if(buf) { buf[bitoffset>>3] = 0 ; }
        /* keep going? */
        if(!cb(user, (char*)buf, n->v)) { return 1; }
    }

    for(i=0; i<16; i++) {
        if(n->n[i]) {
            int   r;
            if(buf) {
                unsigned char* p =  buf + (bitoffset>>3);
                /* check that buffer is remained enough */
                if(bitoffset>>3 == bsz) {
                    /* not enough buffer.. */
                    return -1;
                }

                if(!(bitoffset%8)) {*p = (i<<4); } /* multiple of 8 - getting index for front node */
                else { *p &= 0xf0; *p |= i; } /* multiple of 4 - getting index for back node */
            }
            r = _walk_node_internal(user, n->n[i], cb, buf, bsz, bitoffset+4); /* go to next depth */
            if(r) { return r; }
        }
    }
    return 0;
}

/*
 * @return:
 *    0 : success
 *    1 : stop by user callback
 *    -1: error (ex. not enough buffer size)
 */
static int
_walk_node(void* user, _node_t* n,
           /* return : b_keepgoing => return TRUE for keep going, FALSE for stop here */
           int(cb)(void* user, const char* sym, _value_t* v),
           char* buf, unsigned int bsz) {
    /* buffer size in '_walk_node_internal' doesn't include space for trailing 0 */
    return _walk_node_internal(user, n, cb, (unsigned char*)buf, bsz-1, 0);
}

ylerr_t
yltrie_init() {
    /* initialize */
    memset(&_trie, 0, sizeof(_trie));
    return YLOk;
}

static void _delete_trie_node(_node_t* n) {
    register int i;
    for(i=0; i<16; i++) {
        if(n->n[i]) { _delete_trie_node(n->n[i]); }
    }
    _free_trie_node(n);
}

void
yltrie_deinit() {
    register int i;
    for(i=0; i<16; i++) {
        if(_trie.rt.n[i]) { _delete_trie_node(_trie.rt.n[i]); }
    }
}

int
yltrie_insert(const char* sym, int sty, yle_t* e) {
    _node_t*  n = _get_node((const unsigned char*)sym, TRUE);
    _value_t* esv = n->v;

    n->v = _alloc_trie_value(sty, e);
    if(!n->v) { return -1; /* error case */ }
    if(esv) {
        /* there is already inserted values */
        n->v->desc = esv->desc;
        esv->desc = NULL; /* prevent description from freeing */
        _free_trie_value(esv);
        return 1; /* overwritten */
    } else { 
        _trie.sz ++; 
        return 0; /* newly created */
    }
}

static inline int
_delete_empty_leaf(_node_t* t) {
    register int i;
    /*
     * Check followings
     *    This node doens't have 'value'
     *    This node is leaf.
     */
    for(i=0; i<16; i++) { if(t->n[i]) { break; } }
    if(i >= 16 /* Is this leaf? */
       && !t->v) { /* Doesn't this have 'value'? */
        _free_trie_node(t);
        return 1; /* true */
    } else { return 0; } /* false */
}


/*
 * @return : see '_delete_sym(...)'
 */
static inline int
_delete_empty_char_leaf(_node_t* t, char c) {
    register int  fi = c>>4;
    register int  bi = c&0x0f;
    if(_delete_empty_leaf(t->n[fi]->n[bi])) {
        /*
         * node itself is deleted!
         * So, front node should also be checked!
         */
        t->n[fi]->n[bi] = NULL;
        if(_delete_empty_leaf(t->n[fi])) {
            t->n[fi] = NULL;
            /* this node itself should be checked! */
            return 1;
        } else {
            return 0; /* all done */
        }
    } else {
        return 0; /* all done */
    }
}

/*
 * @return:
 *     1 : needed to be checked that the node @t is empty leaf or not!
 *     0 : all done successfully
 *     -1: cannot find node(symbol is not in trie)
 */
static int
_delete_sym(_node_t* t, const unsigned char* p) {
    register int  fi = *p>>4;
    register int  bi = *p&0x0f;
    if(*p) {
        if(t->n[fi] && t->n[fi]->n[bi]) { /* check that there is front & back node */
            switch(_delete_sym(t->n[fi]->n[bi], p+1)) {
                case 1:    return _delete_empty_char_leaf(t, *p);
                case 0:    return 0;
            }
        }
        /* Cannot find symbol! */
        return -1;
    } else {
        /* 
         * End of string.
         * We need to check this node has valid data.
         * If yes, delete value!
         * If not, symbol name is not valid one!
         */
        if(t->v) {
            _free_trie_value(t->v);
            t->v = NULL;
            return 1;
        } else {
            return -1;
        }
    }
}

int
yltrie_delete(const char* sym) {
    switch(_delete_sym(&_trie.rt, sym)) {
        case 1:    _delete_empty_char_leaf(&_trie.rt, *sym); return 0;
        case 0:    return 0;
        default:   return -1;
    }
}

yle_t*
yltrie_get(int* outty, const char* sym) {
    _node_t* n = _get_node(sym, FALSE);
    if(n && n->v) {
        if(outty) { *outty = n->v->ty; }
        return n->v->e;
    } else {
        return NULL;
    }
}

int
yltrie_set_description(const char* sym, const char* description) {
    _node_t* n = _get_node(sym, FALSE);
    if(n && n->v) {
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

        _free_description(n->v->desc);
        n->v->desc = desc;
        return 0;
    } else {
        return -1;
    }
}



const char*
yltrie_get_description(const char* sym) {
    _node_t* n = _get_node(sym, FALSE);
    if(n && n->v) {
        return n->v->desc;
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
    return TRUE;
}

void
yltrie_mark_reachable() {
    register int i;
    for(i=0; i<16; i++) {
        if(_trie.rt.n[i]) {
            _walk_node(NULL, _trie.rt.n[i], &_cb_mark_reachable, NULL, 0);
        }
    }
}


int
yltrie_get_more_possible_prefix(const char* prefix, 
                                char* buf, unsigned int bufsz) {
    int                   ret = -1;
    register _node_t*     n = &_trie.rt;
    register unsigned int i;
    unsigned int          j, cnt, bi;
    unsigned char         c;

    /* move to prefix */
    { /* scope */
        register int      fi, bi; /* front index */
        register const unsigned char* p = prefix;
        /* move to prefix-node */
        while(*p) {
            _move_down_node(*p, n, fi, bi, 
                            goto no_matching_prefix,
                            goto no_matching_prefix);
            p++;
        }
    }
    /* find more possbile prefix */
    bi = 0;
    while(n) {
        cnt = j = c = 0;
        for(i=0; i<16; i++) {
            if(n->n[i]) { cnt++; j=i; }
        }
        if( cnt > 1 || ((1 == cnt) && n->v) ) { ret = TRIEMeet_branch; break; }
        else if(0 == cnt) { ylassert(n->v); ret = TRIEMeet_leaf; break; }
        c = j<<4;
        n = n->n[j];

        /* Sanity check! */
        /* we don't expect that this can have value */
        ylassert(!n->v);

        cnt = j = 0;
        for(i=0; i<16; i++) {
            if(n->n[i]) { cnt++; j=i; }
        }
        if(cnt > 1) { ret = TRIEMeet_branch; break; }
        else if(0 == cnt) { 
            /* This is unexpected case on this trie algorithm */
            ylassert(0);
            /* ylassert(n->v); ret = TRIEMeet_leaf; break; */
        }
        c |= j;
        n = n->n[j];
    
        if(bi >= (bufsz-1)) { return -1; }
        else { buf[bi] = c; bi++; }
    }

    buf[bi] = 0; /* add trailing 0 */
    return ret;

 no_matching_prefix:
    return TRIEPrefix_not_matched;
}


typedef struct {
    unsigned int    cnt;
    unsigned int    maxlen;
} _get_candidates_num_t;

static int
_cb_get_candidates_num(void* user, const char* sym, _value_t* v) {
    _get_candidates_num_t* st = (_get_candidates_num_t*)user;
    unsigned int len = strlen(sym);
    st->cnt++;
    if(st->maxlen < len) { st->maxlen = len; }
    return TRUE;
}

int
yltrie_get_candidates_num(const char*   prefix, 
                          unsigned int* max_symlen) {
    register _node_t*     n = &_trie.rt;
    _get_candidates_num_t st;

    { /* scope */
        register const unsigned char* p = prefix;
        register int                  fi, bi; /* front index */
        /* move to prefix-node */
        while(*p) {
            _move_down_node(*p, n, fi, bi, 
                            goto no_matching_prefix,
                            goto no_matching_prefix);
            p++;
        }
    }

    /* Now we are in prefix-node */
    { /* scope */
        char    buf[_MAX_SYM_LEN+1];
        st.cnt = st.maxlen = 0;
        if(0 > _walk_node(&st, n, &_cb_get_candidates_num, 
                          buf, _MAX_SYM_LEN+1)) { return -1; }
        if(max_symlen) {
            *max_symlen = st.maxlen;
        }
        return st.cnt;
    }

 no_matching_prefix:
    return 0;
}

typedef struct {
    char**         ppbuf;
    unsigned int   ppbsz;
    unsigned int   i;
} _get_candidates_t;


static int
_cb_get_candidates(void* user, const char* sym, _value_t* v) {
    _get_candidates_t* st = (_get_candidates_t*)user;
    if(st->i >= st->ppbsz) { return FALSE; } /* we should stop here! */
    strcpy(st->ppbuf[st->i], sym);
    st->i++;
    return TRUE; /* keep going */
}

int
yltrie_get_candidates(const char* prefix, char** ppbuf,
                      unsigned int ppbsz,
                      unsigned int pbsz) {
    register _node_t*     n = &_trie.rt;
    _get_candidates_t    st;
    char                 buf[_MAX_SYM_LEN+1];

    ylassert(pbsz <= _MAX_SYM_LEN +1);

    { /* scope */
        register const unsigned char* p = prefix;
        register int                  fi, bi; /* front index */
        /* move to prefix-node */
        while(*p) {
            _move_down_node(*p, n, fi, bi, 
                            goto no_matching_prefix,
                            goto no_matching_prefix);
            p++;
        }
    }

    st.ppbuf = ppbuf; 
    st.ppbsz = ppbsz; 
    st.i = 0;

    /* we assume that pbsz is enough to contain symbol */
    if(0 > _walk_node(&st, n, &_cb_get_candidates, 
                      buf, pbsz)) { return -1; }
    return st.i;

 no_matching_prefix:
    return 0;
}


