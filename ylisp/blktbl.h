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

/*****************************************************************************
 * NOTE
 * ----
 * This header MUST NOT be included in other header file.
 * Only 'c' file should include this if needed.
 * < So, '#ifdef -> #define -> #endif' sequence is missed!
 *
 * WHY?
 * ----
 * This kind of data structure usually is used very frequently.
 * So, to improve performance, design using static size and structure is used
 *   instead of using dynamic size (ex. by using 'element size' etc.)
 *
 * Prerequisite
 * ------------
 * 'mbtublk_t' should be defined before including this header!
 *
 *****************************************************************************/

/*
 * memory pool
 *     * U(used) / F(free)
 *
 *                fbp
 *             +-------+
 *             |   U   | <- index [sz - 1]
 *             +-------+
 *             |   U   |
 *             +-------+
 *             |  ...  |
 *             +-------+
 *             |   U   |
 *             +-------+
 *      fbi -> |   U   |
 *             +-------+
 *             |   F   |
 *             +-------+
 *             |  ...  |
 *             +-------+
 *             |   F   |
 *             +-------+
 *             |   F   | <- index [0]
 *             +-------+
 *
 */


/*
 * block unit of table.
 */
struct _mbtblk {
	unsigned int      i; /**< index of free block pointer */
	_mbtublk_t        b; /**< MBT User BLocK */
};

/*
 * memory block table
 */
struct _mbt {
	struct _mbtblk*   pool;    /**< pool */
	_mbtublk_t**      fbp;     /**< Free Block Pointers */
	int               fbi;     /**< Free Block Index - grow to bottom */
	unsigned int      sz;      /**< number of blocks in pool */
};

static struct _mbt*
_mbt_create(unsigned int sz) {
	struct _mbt* bt = ylmalloc(sizeof(*bt));
	int i;

	if (!bt)
		goto bail_bt;

	bt->pool =  ylmalloc(sizeof(*bt->pool) * sz);
	if (!bt->pool)
		goto bail_pool;

	bt->fbp = ylmalloc(sizeof(_mbtublk_t*) * sz);
	if (!bt->fbp)
		goto bail_fbp;

	bt->fbi = bt->sz = sz;

	for (i = 0; i < sz; i++) {
		bt->fbp[i] = &bt->pool[i].b;
		bt->pool[i].i = i;
	}

	return bt;

 bail_fbp:
	ylfree(bt->pool);
 bail_pool:
	ylfree(bt);
 bail_bt:
	return NULL; /* OOM */
}

static inline void
_mbt_destroy(struct _mbt* bt) {
	ylfree(bt->fbp);
	ylfree(bt->pool);
	ylfree(bt);
}

/*
 * Get free block from block table.
 */
static inline _mbtublk_t*
_mbt_get(struct _mbt* bt) {
	if (bt->fbi <= 0)
		return NULL; /* not enough mem pool */
	else
		return bt->fbp[--bt->fbi];
}

static void
_mbt_put(struct _mbt* bt, _mbtublk_t* b) {
	struct _mbtblk* b1 = container_of(b, struct _mbtblk, b);
	struct _mbtblk* b2 = container_of(bt->fbp[bt->fbi], struct _mbtblk, b);
	unsigned int ti; /* temporal index */

	/* swap fbp index */
	ti = b1->i; b1->i = b2->i; b2->i = ti;

	/* set fbp accordingly */
	bt->fbp[b1->i] = &b1->b;
	bt->fbp[b2->i] = &b2->b;
	bt->fbi++;
}

static inline int
_mbt_nr_used_blk(struct _mbt* bt) {
	return bt->sz - bt->fbi;
}

/*
 * Iterates pool from start to end (ascending in terms of memory address).
 * @bt    : <struct _mbt*>
 * @i     : <int> index to use for interation.
 * @blk   : <_mbtublk_t*> for iteration
 */
#define _mbt_foreach(bt, i, blk)		\
	for (i = 0, blk = &bt->pool[0].b;	\
	     i < bt->sz;			\
	     i++, blk = &bt->pool[i].b)

/*
 * Order is almost ramdom.
 * @bt    : <struct _mbt*>
 * @i     : <int> index to use for interation.
 * @blk   : <_mbtublk_t*> for iteration
 */
#define _mbt_foreach_used(bt, i, blk)		\
	for (i = bt->fbi, blk = bt->fbp[i];	\
	     i < bt->sz;			\
	     ++i, blk = bt->fbp[i])
