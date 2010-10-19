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
#include "yltrie.h"
#include "yldev.h"

#define _MAX_SYM_LEN  1024

/* use 4 bit trie node */
typedef struct _node {
    /*
     * We need two node to represent 1 ASCII character!
     * (If 8bit is used, every node should have 256 pointers. And this is memory-burden.
     *  4bits-way (16 pointers-way) is good choice, in my opinion.)
     */
    struct _node*  n[16];
    void*          v;
} _node_t;

typedef struct _trie {
    _node_t          rt;   /* root. sentinel */
    void           (*fcb)(void*); /* callback for free value */
} _trie_t;

static inline _node_t*
_alloc_node() {
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
_free_node(_node_t* n, void(*fcb)(void*) ) {
    if(n->v) { 
        if(fcb) { fcb(n->v); }
    }
    ylfree(n);
}

static inline void 
_delete_node(_node_t* n, void(*fcb)(void*) ) {
    register int i;
    for(i=0; i<16; i++) {
        if(n->n[i]) { _delete_node(n->n[i], fcb); }
    }
    _free_node(n, fcb);
}

static inline int
_delete_empty_leaf(_node_t* n) {
    register int i;
    /*
     * Check followings
     *    This node doens't have 'value'
     *    This node is leaf.
     */
    for(i=0; i<16; i++) { if(n->n[i]) { break; } }
    if(i >= 16 /* Is this leaf? */
       && !n->v) { /* Doesn't this have 'value'? */
        _free_node(n, NULL); /* there is no 'value'. So, free callback is not required */
        return 1; /* true */
    } else { return 0; } /* false */
}

/*
 * @return : see '_delete_sym(...)'
 */
static inline int
_delete_empty_char_leaf(_node_t* n, char c) {
    register int  fi = c>>4;
    register int  bi = c&0x0f;
    if(_delete_empty_leaf(n->n[fi]->n[bi])) {
        /*
         * node itself is deleted!
         * So, front node should also be checked!
         */
        n->n[fi]->n[bi] = NULL;
        if(_delete_empty_leaf(n->n[fi])) {
            n->n[fi] = NULL;
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
_delete_sym(_node_t* n, const unsigned char* p, void(*fcb)(void*)) {
    register int  fi = *p>>4;
    register int  bi = *p&0x0f;
    if(*p) {
        if(n->n[fi] && n->n[fi]->n[bi]) { /* check that there is front & back node */
            switch(_delete_sym(n->n[fi]->n[bi], p+1, fcb)) {
                case 1:    return _delete_empty_char_leaf(n, *p);
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
        if(n->v) {
            if(fcb) { fcb(n->v); }
            n->v = NULL;
            return 1;
        } else {
            return -1;
        }
    }
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
_get_node(_trie_t* t, const unsigned char* sym, int bcreate) {
    register _node_t*             n = &t->rt;
    register const unsigned char* p = sym;
    register int                  fi, bi; /* front index */

    if(bcreate) {
        while(*p) {
            _move_down_node(*p, n, fi, bi, 
                            n = n->n[fi] = _alloc_node(),
                            n = n->n[bi] = _alloc_node());
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

static int
_equal_internal(const _node_t* n0, const _node_t* n1, 
                  int(*cmp)(const void*, const void*)) {
    register int i;
    int          r;
    if(n0->v && n1->v) {
        if(0 == cmp(n0->v, n1->v)) { return 0; /* NOT same */ }
    } else if(n0->v || n1->v) {
        return 0; /* NOT same */
    }

    /* compare child nodes! */
    for(i=0; i<16; i++) {
        if(n0->n[i] && n1->n[i]) {
            if(0 == _equal_internal(n0->n[i], n1->n[i], cmp)) { return 0; /* NOT same */ }
        } else if(n0->n[i] || n1->n[i]) {
            return 0; /* NOT same */
        }
    }
    return 1; /* n0 and n1 is same */
}

static _node_t*
_node_clone(const _node_t* n, void* user, void*(*clonev)(void*, const void*)) {
    register int i;
    _node_t* r = _alloc_node();
    if(n->v) { r->v = clonev(user, n->v); }
    for(i=0; i<16; i++) {
        if(n->n[i]) {
            r->n[i] = _node_clone(n->n[i], user, clonev);
        }
    }
    return r;
}

/*
 * @return: see '_walk_node'
 */
static int
_walk_internal(void* user, _node_t* n,
               int(cb)(void* user, const char* sym, void* v),
               unsigned char* buf, unsigned int bsz, /* bsz: excluding space for trailing 0 */
               unsigned int bitoffset) {
    register unsigned int i;

    if(n->v) { 
        /* round-up base on 8. And add trailing 0 */
        if(buf) { buf[bitoffset>>3] = 0 ; }
        /* keep going? */
        if(!cb(user, (char*)buf, n->v)) { return 0; }
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
            r = _walk_internal(user, n->n[i], cb, buf, bsz, bitoffset+4); /* go to next depth */
            if(r<=0) { return r; }
        }
    }
    return 1;
}

void**
yltrie_getref(yltrie_t* t, const char* sym) {
    _node_t* n;
    ylassert(sym);
    if(0 == *sym) { return NULL; } /* 0 length string */
    n = _get_node(t, sym, FALSE);
    return n? &n->v: NULL;
}

void*
yltrie_get(_trie_t* t, const char* sym) {
    void** pv = yltrie_getref(t, sym);
    return pv? *pv: NULL;
}

int
yltrie_walk(_trie_t* t, void* user, const char* from,
            /* return 1 for keep going, 0 for stop and don't do anymore */
            int(cb)(void* user, const char* sym, void* v)) {
    char        buf[_MAX_SYM_LEN + 1];
    _node_t*    n = _get_node(t, from, FALSE);

    ylassert(t && from);
    if(n) {
        return _walk_internal(user, n, cb, (unsigned char*)buf, _MAX_SYM_LEN, 0); 
    } else { return -1; }
}

int
yltrie_insert(_trie_t* t, const char* sym, void* v) {
    _node_t*  n;

    ylassert(t && sym);
    if(0 == *sym) { return -1; } /* 0 length symbol */
    if(!v || (strlen(sym) >= _MAX_SYM_LEN) ) { return -1; /* error case */ }

    n = _get_node(t, (const unsigned char*)sym, TRUE);
    if(n->v) {
        if(t->fcb) { t->fcb(n->v); }
        n->v = v;
        return 1; /* overwritten */
    } else {
        n->v = v;
        return 0; /* newly created */
    }
}

yltrie_t*
yltrie_create(void(*fcb)(void*)) {
    _trie_t* t = ylmalloc(sizeof(_trie_t));
    ylassert(t);
    memset(t, 0, sizeof(*t));
    t->fcb = fcb;
    return t;
}

static void
_trie_clean(_trie_t* t) {
    register int i;
    for(i=0; i<16; i++) {
        if(t->rt.n[i]) {
            _delete_node(t->rt.n[i], t->fcb);
            t->rt.n[i] = NULL;
        }
    }
}

extern void
yltrie_destroy(yltrie_t* t) {
    _trie_clean(t);
    ylfree(t);
}


int
yltrie_delete(_trie_t* t, const char* sym) {
    ylassert(t && sym);
    switch(_delete_sym(&t->rt, sym, t->fcb)) {
        case 1:    _delete_empty_char_leaf(&t->rt, *sym); return 0;
        case 0:    return 0;
        default:   return -1;
    }
}

void(*yltrie_fcb(const yltrie_t* t))(void*) {
    return t->fcb;
}

int
yltrie_equal(const yltrie_t* t0, const yltrie_t* t1,
             int(*cmp)(const void*, const void*)) {
    return _equal_internal(&t0->rt, &t1->rt, cmp);
}

int
yltrie_copy(yltrie_t* dst, const yltrie_t* src, void* user,
            void*(*clonev)(void*,const void*)) {
    register int i;
    _trie_clean(dst);
    dst->fcb = src->fcb;
    for(i=0; i<16; i++) {
        if(src->rt.n[i]) { dst->rt.n[i] = _node_clone(src->rt.n[i], user, clonev); }
    }

}

yltrie_t*
yltrie_clone(const yltrie_t* t, void* user, void*(*clonev)(void*, const void*)) {
    register int i;
    yltrie_t*    r = yltrie_create(t->fcb);
    yltrie_copy(r, t, user, clonev);
    return r;
}

int
yltrie_auto_complete(_trie_t* t, 
                     const char* start_with, 
                     char* buf, unsigned int bufsz) {
    int                   ret = -1;
    register _node_t*     n;
    register unsigned int i;
    unsigned int          j, cnt, bi;
    unsigned char         c;

    ylassert(t && start_with && buf);

    /* move to prefix */
    n = _get_node(t, start_with, FALSE);
    if(!n) { goto bail; }

    /* find more possbile prefix */
    bi = 0;
    while(n) {
        cnt = j = c = 0;
        for(i=0; i<16; i++) {
            if(n->n[i]) { cnt++; j=i; }
        }
        if( cnt > 1 || ((1 == cnt) && n->v) ) { ret = YLTRIEBranch; break; }
        else if(0 == cnt) { ylassert(n->v); ret = YLTRIELeaf; break; }
        c = j<<4;
        n = n->n[j];

        /* Sanity check! */
        /* we don't expect that this can have value */
        ylassert(!n->v);

        cnt = j = 0;
        for(i=0; i<16; i++) {
            if(n->n[i]) { cnt++; j=i; }
        }
        if(cnt > 1) { ret = YLTRIEBranch; break; }
        else if(0 == cnt) { 
            /* This is unexpected case on this trie algorithm */
            ylassert(0);
        }
        c |= j;
        n = n->n[j];
    
        if(bi >= (bufsz-1)) { return -1; }
        else { buf[bi] = c; bi++; }
    }

    buf[bi] = 0; /* add trailing 0 */
    return ret;

 bail:
    return YLTRIEFail;
}
