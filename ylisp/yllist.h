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

/*================================
 *
 * Doubly Linked List
 * (From "include/linux/list.h"
 *    - 'GCC' dependent factors are removed)
 *
 * This is just DATA STRUCTURE HELPER for YILSP
 *================================*/

#ifndef ___YLLISt_h___
#define ___YLLISt_h___

#include "yldef.h"

/*
 * Define primitive operation of linked list!
 */

/**
 * If possible DO NOT access struct directly!.
 */
typedef struct yllist_link {
    struct yllist_link *next, *prev;
} yllist_link_t;

/**
 * initialize list head.
 */
static inline void
yllist_init_link(yllist_link_t* link) {
    link->next = link->prev = link;
}

static inline int
yllist_is_empty(const yllist_link_t* head) {
    return head->next == head;
}

static inline void
yllist_add(yllist_link_t* prev, yllist_link_t* next, yllist_link_t* anew) {
    next->prev = prev->next = anew;
    anew->next = next; anew->prev = prev;
}

static inline void
yllist_add_next(yllist_link_t* link, yllist_link_t* anew) {
    yllist_add(link, link->next, anew);
}

static inline void
yllist_add_prev(yllist_link_t* link, yllist_link_t* anew) {
    yllist_add(link->prev, link, anew);
}

static inline void
yllist_add_first(yllist_link_t* head, yllist_link_t* anew) {
    yllist_add_next(head, anew);
}

static inline void
yllist_add_last(yllist_link_t* head, yllist_link_t* anew) {
    yllist_add_prev(head, anew);
}

static inline void
__yllist_del(yllist_link_t* prev, yllist_link_t* next) {
    prev->next = next;
    next->prev = prev;
}

static inline void
yllist_del(yllist_link_t* link) {
    __yllist_del(link->prev, link->next);
}

static inline void
yllist_replace(yllist_link_t* old, yllist_link_t* anew) {
    anew->next = old->next;
    anew->next->prev = anew;
    anew->prev = old->prev;
    anew->prev->next = anew;
}

/**
 * @pos     : the yllist_link_t* to use as a loop cursor
 * @head    : head of list (yllist_link_t*)
 */
#define yllist_foreach(pos, head)                                     \
    for((pos) = (head)->next; (pos) != (head); (pos) = (pos)->next)

#define yllist_foreach_backward(pos, head)                            \
    for((pos) = (head)->prev; (pos) != (head); (pos) = (pos)->prev)

/**
 * @pos     : the yllist_link_t* to use as a loop cursor
 * @n       : another yllist_link_t* to use as temporary storage
 * @head    : head of list (yllist_link_t*)
 */
#define yllist_foreach_removal_safe(pos, n, head)                     \
    for((pos) = (head), (n) = (pos)->next; (pos) != (head); (pos) = (n), (n) = (pos)->next)

#define yllist_foreach_removal_safe_backward(pos, n, head)            \
    for((pos) = (head), (n) = (pos)->prev; (pos) != (head); (pos) = (n), (n) = (pos)->prev)
/**
 * @pos     : the @type* to use as a loop cursor.
 * @head    : the head for list (yllist_link_t*)
 * @type    : the type of the struct of *@pos
 * @member  : the name of the yllist_link within the struct.
 */
#define yllist_foreach_item(pos, head, type, member)                    \
    for((pos) = container_of((head)->next, type, member);               \
        &(pos)->member != (head);                                       \
        (pos) = container_of((pos)->member.next, type, member))

#define yllist_foreach_item_backward(pos, head, type, member)           \
    for((pos) = container_of((head)->prev, type, member);               \
        &(pos)->member != (head);                                       \
        (pos) = container_of((pos)->member.prev, type, member))

/**
 * @type    : the type of the struct of *@pos
 * @pos     : the @type* to use as a loop cursor.
 * @n       : another @type* to use as temporary storage.
 * @head    : the head for list (yllist_link_t*)
 * @member  : the name of the yllist_link within the struct.
 */
#define yllist_foreach_item_removal_safe(pos, n, head, type, member)    \
    for((pos) = container_of((head)->next, type, member),               \
            (n) = container_of((pos)->member.next, type, member);       \
        &(pos)->member != (head);                                       \
        (pos) = (n), (n) = container_of((pos)->member.next, type, member))

#define yllist_foreach_item_removal_safe_backward(pos, n, head, type, member) \
    for((pos) = container_of((head)->prev, type, member),               \
            (n) = container_of((pos)->member.prev, type, member);       \
        &(pos)->member != (head);                                       \
        (pos) = (n), (n) = container_of((pos)->member.prev, type, member))

static inline unsigned int
yllist_size(const yllist_link_t* head) {
    yllist_link_t*   pos;
    unsigned int size = 0;
    yllist_foreach(pos, head) { size++; }
    return size;
}

#endif /* ___YLLISt_h___ */
