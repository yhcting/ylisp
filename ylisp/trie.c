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


/*
 * To store global symbols, Trie is used as an main data structure.
 * But, having 256 pointers for every node is heavy-memory-burden.
 * So, we choose 4-bit-way; One ASCII character(8 bit) is represented
 *   by 2 node (4bit + 4bit) - front and back node.
 * We can also get sorted result from Trie.
 * Why Trie is chosen (versus Hash, Tree etc)?
 * Pros
 *    - To search symbol is fast.
 *    - It is efficient(in terms of performance)
 *	 and easy to implement Auto-Completion.
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
 * |o	...   | 16 pointers
 * |t  +----+ |	     <front node>
 * |   |0011|-|----> +---+----+
 * |   +----+ |	     |	  ... |
 * |	...   v	     |	 +----+	     <back node>
 * +---+----+---     |	 |0100|----> +---+----+
 *		     |	 +----+	     | D |0000|
 *		     |	  ...	     | A +----+
 *				     | T | ...|
 *				     | A |    |
 *				     +---+----+
 */
#include <memory.h>
#include <string.h>
#include "lisp.h"

#define _MAX_KEY_LEN  1024

/* use 4 bit trie node */
struct _node {
	/*
	 * We need two node to represent 1 ASCII character!
	 * (If 8bit is used, every node should have 256 pointers.
	 *  And this is memory-burden.
	 *  4bits-way (16 pointers-way) is good choice, in my opinion.)
	 */
	struct _node*  n[16];
	void*	       v;
};

struct _trie {
	struct _node	      rt;   /* root. sentinel */
	void	       (*fcb)(void*); /* callback for free value */
};

static inline struct _node*
_alloc_node(void) {
	struct _node* n = ylmalloc(sizeof(*n));
	if (!n)
		/*
		 * if allocing such a small size of memory fails,
		 *  we can't do anything!
		 * Just ASSERT IT!
		 */
		ylassert(0);

	memset(n, 0, sizeof(struct _node));
	return n;
}

static inline void
_free_node(struct _node* n, void(*fcb)(void*) ) {
	if (n->v && fcb)
		(*fcb)(n->v);
	ylfree(n);
}

static inline void
_delete_node(struct _node* n, void(*fcb)(void*) ) {
	register int i;
	for (i=0; i<16; i++)
		if (n->n[i])
			_delete_node(n->n[i], fcb);
	_free_node(n, fcb);
}

static inline int
_delete_empty_leaf(struct _node* n) {
	register int i;
	/*
	 * Check followings
	 *    This node doens't have 'value'
	 *    This node is leaf.
	 */
	for (i=0; i<16; i++)
		if (n->n[i])
			break;
	if (i >= 16 /* Is this leaf? */
	    && !n->v) { /* Doesn't this have 'value'? */
		/* there is no 'value'. So, free callback is not required */
		_free_node(n, NULL);
		return 1; /* true */
	} else
		return 0; /* false */
}

/*
 * @return : see '_delete_key(...)'
 */
static inline int
_delete_empty_byte_leaf(struct _node* n, unsigned char c) {
	register int  fi = c>>4;
	register int  bi = c&0x0f;
	if (_delete_empty_leaf(n->n[fi]->n[bi])) {
		/*
		 * node itself is deleted!
		 * So, front node should also be checked!
		 */
		n->n[fi]->n[bi] = NULL;
		if (_delete_empty_leaf(n->n[fi])) {
			n->n[fi] = NULL;
			/* this node itself should be checked! */
			return 1;
		} else
			return 0; /* all done */
	} else
		return 0; /* all done */
}

/*
 * @return:
 *     1 : needed to be checked that the node @t is empty leaf or not!
 *     0 : all done successfully
 *     -1: cannot find node(symbol is not in trie)
 */
static int
_delete_key(struct _node* n, const unsigned char* p,
	    unsigned int sz, void(*fcb)(void*)) {
	register int	 fi = *p>>4;
	register int	 bi = *p&0x0f;
	if (sz>0) {
		/* check that there is front & back node */
		if (n->n[fi] && n->n[fi]->n[bi]) {
			switch (_delete_key(n->n[fi]->n[bi], p+1, sz-1, fcb)) {
			case 1:	   return _delete_empty_byte_leaf(n, *p);
			case 0:	   return 0;
			}
		}
		/* Cannot find symbol! */
		return -1;
	} else {
		/*
		 * End of stream.
		 * We need to check this node has valid data.
		 * If yes, delete value!
		 * If not, symbol name is not valid one!
		 */
		if (n->v) {
			if (fcb)
				(*fcb)(n->v);
			n->v = NULL;
			return 1;
		} else
			return -1;

	}
}

/**
 * @c:	   (char)     character
 * @n:	   (struct _node*) node to move down
 * @fi:	   (int)      variable to store front index
 * @bi:	   (int)      variable to store back index
 * @felse: <code>     code in 'else' brace of front node
 * @belse: <code>     code in 'else' brace of back node
 */
#define _move_down_node(c, n, fi, bi, felse, belse)	\
	do {						\
		(fi) = (c)>>4; (bi) = (c)&0x0f;		\
		if ((n)->n[fi])				\
			(n) = (n)->n[fi];		\
		else {					\
			felse;				\
		}					\
		if ((n)->n[bi])				\
			(n) = (n)->n[bi];		\
		else {					\
			belse;				\
		}					\
	} while (0)
/*
 * If node exists, it returns node.
 * If not, it creates new one and returns it.
 * @return: back node of last character of symbol.
 *          If fails to get, NULL is returned.
 */
static struct _node*
_get_node(struct _trie* t,
	  const unsigned char* key, unsigned int sz,
	  int bcreate) {
	register struct _node*        n = &t->rt;
	register const unsigned char* p = key;
	register const unsigned char* pend = p+sz;
	register int		      fi, bi; /* front index */

	if (bcreate) {
		while (p<pend) {
			_move_down_node(*p, n, fi, bi,
					n = n->n[fi] = _alloc_node(),
					n = n->n[bi] = _alloc_node());
			p++;
		}
	} else {
		while (p<pend) {
			_move_down_node(*p, n, fi, bi,
					return NULL, return NULL);
			p++;
		}
	}
	return n;
}

static int
_equal_internal(const struct _node* n0, const struct _node* n1,
		  int(*cmp)(const void*, const void*)) {
	register int i;
	if (n0->v && n1->v) {
		if (0 == cmp(n0->v, n1->v))
			return 0; /* NOT same */
	} else if (n0->v || n1->v)
		return 0; /* NOT same */

	/* compare child nodes! */
	for (i=0; i<16; i++) {
		if (n0->n[i] && n1->n[i]) {
			if (0 == _equal_internal(n0->n[i], n1->n[i], cmp))
				return 0; /* NOT same */
		} else if (n0->n[i] || n1->n[i])
			return 0; /* NOT same */
	}
	return 1; /* n0 and n1 is same */
}

static struct _node*
_node_clone(const struct _node* n,
	    void* user,
	    void*(*clonev)(void*, const void*)) {
	register int i;
	struct _node* r = _alloc_node();
	/* Do shallow copy if copy callback is NULL. */
	if (n->v)
		r->v = clonev? clonev(user, n->v): n->v;
	for (i=0; i<16; i++)
		if (n->n[i])
			r->n[i] = _node_clone(n->n[i], user, clonev);
	return r;
}

/*
 * @return: see '_walk_node'
 */
static int
_walk_internal(void* user,
	       struct _node*   n,
	       int(*           cb)(void*,
				   const unsigned char*,
				   unsigned int,
				   void*),
	       unsigned char*  buf,
	       unsigned int    bsz, /* bsz: excluding space for trailing 0 */
	       unsigned int    bitoffset) {
	register unsigned int   i;
	int	                r;
	register unsigned char* p =  buf + (bitoffset>>3);

	ylassert(buf);
	if (n->v) {
		/* value should exists at byte-based-node */
		ylassert(0 == bitoffset%8);
		/* if (buf) { buf[bitoffset>>3] = 0 ; } */
		/* keep going? */
		if (!(*cb)(user, buf, bitoffset/8, n->v))
			return 0;
	}

	for (i=0; i<16; i++) {
		if (!n->n[i])
			continue;

		/* check that buffer is remained enough */
		if (bitoffset>>3 == bsz)
			/* not enough buffer.. */
			return -1;

		if (!(bitoffset%8))
			/*
			 * multiple of 8
			 *  - getting index for front node
			 */
			*p = (i<<4);
		else {
			/*
			 * multiple of 4
			 *  - getting index for back node
			 */
			*p &= 0xf0;
			*p |= i;
		}

		/* go to next depth */
		r = _walk_internal(user, n->n[i], cb, buf, bsz, bitoffset+4);
		if (r<=0)
			return r;
	}
	return 1;
}

void**
yltrie_getref(yltrie_t* t,
	      const unsigned char* key, unsigned int sz) {
	struct _node* n;
	ylassert(key);
	if (0 == sz)
		return NULL; /* 0 length string */
	n = _get_node(t, key, sz, FALSE);
	return n? &n->v: NULL;
}

void*
yltrie_get(struct _trie* t, const unsigned char* key, unsigned int sz) {
	void** pv = yltrie_getref(t, key, sz);
	return pv? *pv: NULL;
}

int
yltrie_walk(struct _trie* t, void* user,
	    const unsigned char* from, unsigned int fromsz,
	    /* return 1 for keep going, 0 for stop and don't do anymore */
	    int(*cb)(void*, const unsigned char*, unsigned int, void*)) {
	char	         buf[_MAX_KEY_LEN + 1];
	struct _node*    n = _get_node(t, from, fromsz, FALSE);

	ylassert(t && from);
	if (n)
		return _walk_internal(user, n, cb,
				      (unsigned char*)buf,
				      _MAX_KEY_LEN,
				      0);
	else
		return -1;
}

int
yltrie_full_walk (yltrie_t* t, void* user,
		  /* return :1 for keep going, 0 for stop */
		  int(*cb)(void*/*user*/,
			   const unsigned char*/*key*/, unsigned int/*sz*/,
			   void*/*value*/)) {
	return yltrie_walk (t, user, (unsigned char*)"", 0, cb);
}

int
yltrie_insert(struct _trie* t,
	      const unsigned char* key,
	      unsigned int sz,
	      void* v) {
	struct _node*  n;

	ylassert(t && key);
	if (0 == sz)
		return -1; /* 0 length symbol */
	if (!v || (sz >= _MAX_KEY_LEN))
		return -1; /* error case */

	n = _get_node(t, key, sz, TRUE);
	if (n->v) {
		if (t->fcb)
			(*t->fcb)(n->v);
		n->v = v;
		return 1; /* overwritten */
	} else {
		n->v = v;
		return 0; /* newly created */
	}
}

yltrie_t*
yltrie_create(void(*fcb)(void*)) {
	struct _trie* t = ylmalloc(sizeof(*t));
	ylassert(t);
	memset(t, 0, sizeof(*t));
	t->fcb = fcb;
	return t;
}

void
yltrie_clean(struct _trie* t) {
	register int i;
	for (i=0; i<16; i++) {
		if (t->rt.n[i]) {
			_delete_node(t->rt.n[i], t->fcb);
			t->rt.n[i] = NULL;
		}
	}
}

void
yltrie_destroy(yltrie_t* t) {
	yltrie_clean(t);
	ylfree(t);
}


int
yltrie_delete(struct _trie* t, const unsigned char* key, unsigned int sz) {
	ylassert(t && key);
	switch(_delete_key(&t->rt, key, sz, t->fcb)) {
	case 1:
	case 0:	   return 0;
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
	yltrie_clean(dst);
	dst->fcb = src->fcb;
	for (i=0; i<16; i++)
		if (src->rt.n[i])
			dst->rt.n[i] = _node_clone(src->rt.n[i], user, clonev);
	/* return value of this function is reserved for future use */
	return 0;
}

yltrie_t*
yltrie_clone(const yltrie_t* t,
	     void* user,
	     void*(*clonev)(void*, const void*)) {
	yltrie_t*	 r = yltrie_create(t->fcb);
	yltrie_copy(r, t, user, clonev);
	return r;
}

int
yltrie_auto_complete(struct _trie* t,
		     const unsigned char* start_with, unsigned int sz,
		     unsigned char* buf, unsigned int bufsz) {
	int                     ret = -1;
	register struct _node*  n;
	register unsigned int   i;
	unsigned int	        j, cnt, bi;
	unsigned char           c;

	ylassert(t && start_with && buf);

	/* move to prefix */
	n = _get_node(t, start_with, sz, FALSE);
	if (!n)
		goto bail;

	/* find more possbile prefix */
	bi = 0;
	while (n) {
		cnt = j = c = 0;
		for (i=0; i<16; i++) {
			if (n->n[i]) {
				cnt++;
				j=i;
			}
		}

		if (cnt > 1 || ((1 == cnt) && n->v)) {
			ret = YLTRIEBranch;
			break;
		} else if (0 == cnt) {
			ylassert(n->v);
			ret = YLTRIELeaf;
			break;
		}
		c = j<<4;
		n = n->n[j];

		/* Sanity check! */
		/* we don't expect that this can have value */
		ylassert(!n->v);

		cnt = j = 0;
		for (i=0; i<16; i++) {
			if (n->n[i]) {
				cnt++;
				j=i;
			}
		}
		if (cnt > 1) {
			ret = YLTRIEBranch;
			break;
		} else if (0 == cnt)
			/* This is unexpected case on this trie algorithm */
			ylassert(0);

		c |= j;
		n = n->n[j];

		if (bi >= (bufsz-1))
			return -1;
		else {
			buf[bi] = c;
			bi++;
		}
	}

	buf[bi] = 0; /* add trailing 0 */
	return ret;

 bail:
	return YLTRIEFail;
}
