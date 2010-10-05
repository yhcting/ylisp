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



/**
 * Understanding these code is far away from understand 'ylisp'.
 * This is OPTIONAL.
 * Utility functions
 */

#ifndef ___YLUt_h___
#define ___YLUt_h___

#include "yldev.h"


/*================================
 *
 * Macros
 *
 *================================*/

/*================================
 *
 * Doubly Linked List
 * (From "include/linux/list.h" 
 *    - 'GCC' dependent factors are removed)
 *================================*/

/*
 * Define primitive operation of linked list!
 * 
 */

/**
 * If possible DO NOT access struct directly!.
 */
typedef struct ylutlist_link {
    struct ylutlist_link *next, *prev;
} ylutlist_link_t;

/**
 * initialize list head.
 */
static inline void
ylutlist_init_link(ylutlist_link_t* link) {
    link->next = link->prev = link;
}

static inline int
ylutlist_is_empty(const ylutlist_link_t* head) {
    return head->next == head;
}

static inline void
ylutlist_add(ylutlist_link_t* prev, ylutlist_link_t* next, ylutlist_link_t* anew) {
    next->prev = prev->next = anew;
    anew->next = next; anew->prev = prev;
}

static inline void
ylutlist_add_next(ylutlist_link_t* link, ylutlist_link_t* anew) {
    ylutlist_add(link, link->next, anew);
}

static inline void
ylutlist_add_prev(ylutlist_link_t* link, ylutlist_link_t* anew) {
    ylutlist_add(link->prev, link, anew);
}

static inline void
ylutlist_add_first(ylutlist_link_t* head, ylutlist_link_t* anew) {
    ylutlist_add_next(head, anew);
}

static inline void
ylutlist_add_last(ylutlist_link_t* head, ylutlist_link_t* anew) {
    ylutlist_add_prev(head, anew);
}

static inline void
__ylutlist_del(ylutlist_link_t* prev, ylutlist_link_t* next) {
    prev->next = next;
    next->prev = prev;
}

static inline void
ylutlist_del(ylutlist_link_t* link) {
    __ylutlist_del(link->prev, link->next);
}

static inline void
ylutlist_replace(ylutlist_link_t* old, ylutlist_link_t* anew) {
    anew->next = old->next;
    anew->next->prev = anew;
    anew->prev = old->prev;
    anew->prev->next = anew;
}

/**
 * @pos     : the ylutlist_link_t* to use as a loop cursor
 * @head    : head of list (ylutlist_link_t*)
 */
#define ylutlist_foreach(pos, head)                                     \
    for((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

#define ylutlist_foreach_backward(pos, head)                            \
    for((pos) = (head)->prev; (pos) != (head); (pos) = (pos)->prev)

/**
 * @pos     : the ylutlist_link_t* to use as a loop cursor
 * @n       : another ylutlist_link_t* to use as temporary storage
 * @head    : head of list (ylutlist_link_t*)
 */
#define ylutlist_foreach_removal_safe(pos, n, head)                     \
    for((pos) = (head), (n) = (pos)->next; (pos) != (head); (pos) = (n), (n) = (pos)->next)

#define ylutlist_foreach_removal_safe_backward(pos, n, head)            \
    for((pos) = (head), (n) = (pos)->prev; (pos) != (head); (pos) = (n), (n) = (pos)->prev)
/**
 * @pos     : the @type* to use as a loop cursor.
 * @head    : the head for list (ylutlist_link_t*)
 * @type    : the type of the struct of *@pos
 * @member  : the name of the ylutlist_link within the struct.
 */
#define ylutlist_foreach_item(pos, head, type, member)                  \
    for((pos) = container_of((head)->next, type, member);               \
        &(pos)->member != (head);                                       \
        (pos) = container_of((pos)->member.next, type, member))

#define ylutlist_foreach_item_backward(pos, head, type, member)         \
    for((pos) = container_of((head)->prev, type, member);               \
        &(pos)->member != (head);                                       \
        (pos) = container_of((pos)->member.prev, type, member))

/**
 * @type    : the type of the struct of *@pos
 * @pos     : the @type* to use as a loop cursor.
 * @n       : another @type* to use as temporary storage.
 * @head    : the head for list (ylutlist_link_t*)
 * @member  : the name of the ylutlist_link within the struct.
 */
#define ylutlist_foreach_item_removal_safe(pos, n, head, type, member)  \
    for((pos) = container_of((head)->next, type, member),               \
            (n) = container_of((pos)->member.next, type, member);       \
        &(pos)->member != (head);                                       \
        (pos) = (n), (n) = container_of((pos)->member.next, type, member))

#define ylutlist_foreach_item_removal_safe_backward(pos, n, head, type, member) \
    for((pos) = container_of((head)->prev, type, member),               \
            (n) = container_of((pos)->member.prev, type, member);       \
        &(pos)->member != (head);                                       \
        (pos) = (n), (n) = container_of((pos)->member.prev, type, member))

static inline unsigned int
ylutlist_size(const ylutlist_link_t* head) {
    ylutlist_link_t*   pos;
    unsigned int size = 0;
    ylutlist_foreach(pos, head) { size++; }
    return size;
}

/*================================
 *
 * Handling dynamic buffer
 *
 *================================*/
/* DYNmaic Buffer */
typedef struct {
    unsigned int    limit;
    unsigned int    sz;
    unsigned char*  b;
} ylutdynb_t;

static inline unsigned int
ylutdynb_limit(const ylutdynb_t* b) {
    return b->limit;
}

static inline unsigned int
ylutdynb_freesz(const ylutdynb_t* b) {
    return b->limit - b->sz;
}

static inline unsigned int
ylutdynb_sz(const ylutdynb_t* b) {
    return b->sz;
}

static inline unsigned char*
ylutdynb_ptr(const ylutdynb_t* b) {
    return b->b + b->sz;
}

/*
 * @return: 0 if success.
 */
static inline int
ylutdynb_init(ylutdynb_t* b, unsigned int init_limit) {
    b->sz = 0;
    b->b = (unsigned char*)ylmalloc(init_limit);
    if(b->b) { b->limit = init_limit; return 0; }
    else { b->limit = 0; return -1; }
}

static inline void
ylutdynb_reset(ylutdynb_t* b) {
    b->sz = 0;
}

static inline void
ylutdynb_clean(ylutdynb_t* b) {
    if(b->b) { ylfree(b->b); }
    b->b = NULL;
    b->limit = b->sz = 0;
}

/*
 * increase buffer size by two times.
 * due to using memcpy, it cannot be static inline
 * @return: <0 if fails.
 */
extern int
ylutdynb_expand(ylutdynb_t* b);

static inline int
ylutdynb_secure(ylutdynb_t* b, unsigned int sz_required) {
    while( sz_required > ylutdynb_freesz(b)
           && !ylutdynb_expand(b) ) {}
    return sz_required <= ylutdynb_freesz(b);
}

/*
 * due to using memcpy, it cannot be static inline
 */
extern int
ylutdynb_shrink(ylutdynb_t* b, unsigned int sz_to);

/*
 * due to using memcpy, it cannot be static inline
 */
extern int
ylutdynb_append(ylutdynb_t* b, const unsigned char* d, unsigned int dsz);

/*================================
 *
 * Handling C String (Use dynamic buffer)
 *
 *================================*/
/*
 * @return: <0 means, "this is not string buffer!"
 */
static inline unsigned int
ylutstr_len(const ylutdynb_t* b) {
    return b->sz - 1; /* '-1' to exclude trailing 0 */
}

static inline unsigned char*
ylutstr_ptr(const ylutdynb_t* b) {
    return b->b + ylutstr_len(b); 
}

static inline unsigned char*
ylutstr_string(const ylutdynb_t* b) {
    return b->b;
}

static inline void
ylutstr_reset(ylutdynb_t* b) {
    *b->b = 0; /* add trailing 0 */
    b->sz = 1;
}

static inline int
ylutstr_init(ylutdynb_t* b, unsigned int init_limit) {
    if(0 <= ylutdynb_init(b, init_limit+1)) {
        ylutstr_reset(b);
        return 0;
    } 
    return -1;
}

/*
 * @return: 
 *    number of bytes appended.
 *    '0' means nothing appended. may be error?
 */
extern int
ylutstr_append(ylutdynb_t* b, const char* format, ...);

/*================================
 *
 * Misc. utilities
 *
 *================================*/

/**
 * CALLER SHOULD FREE returned MEMORY by calling 'free()'
 * @btext: 
 *    TRUE : return value is string (includes trailing 0)
 *    FALSE: return value is pure binary data.
 * @outsz:
 *    if success, this includes size of file.
 *    if fails, this includes error cause
 *        YLOk: success.
 *        YLErr_io: file I/O error.
 *        YLErr_out_of_memory: not enough memory
 *
 * @return:
 *    NULL : if fails or size of file is 0.
 *    So, reading empty file returns NULL and *outsz == YLUTREADF_OK
 */
extern void*
ylutfile_read(unsigned int* outsz, const char* fpath, int btext);



#endif /* ___YLUt_h___ */
