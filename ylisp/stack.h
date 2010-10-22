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



/*====================
 * Simple Stack!
 *====================*/

#ifndef ___STACk_h___
#define ___STACk_h___

#include <memory.h>
#include "yldev.h"



typedef struct {
    void**          item;
    unsigned int    limit;         /**< stack limit */
    unsigned int    sz;            /**< stack size */
    void          (*fcb)(void*);   /**< call back for free item: can be NULL */
} ylstk_t;

static inline ylstk_t*
ylstk_create(unsigned int init_limit, void(*freecb)(void* item)) {
    /* In most case, we cannot keep executing interpreter if we fail to allocate stack */
    ylstk_t*   s = ylmalloc(sizeof(ylstk_t));
    if(!s) { ylassert(0); }
    if(0 == init_limit) {
        init_limit = 16; /* default initial stack limit size */
    }
    s->item = ylmalloc(sizeof(void*)*init_limit);
    if(!s->item) { ylassert(0); }
    s->limit = init_limit;
    s->sz = 0;
    s->fcb = freecb;
    return s;
}

static inline void
ylstk_clean(ylstk_t* s) {
    if(s->fcb) {
        register int    i;
        for(i=0; i<s->sz; i++) {
            (*s->fcb)(s->item[i]);
        }
    }
    s->sz = 0;
}

static inline void
ylstk_destroy(ylstk_t* s) {
    ylstk_clean(s);
    ylfree(s->item);
    ylfree(s);
}



static inline unsigned int
ylstk_size(ylstk_t* s) {
    return s->sz;
}

static inline void
ylstk_push(ylstk_t* s, void* item) {
    if(s->sz >= s->limit) {
        /* doubling stack size */
        void** tmp = (void**)ylmalloc(s->limit * sizeof(void*) * 2);
        if(!tmp) {
            yllogE1("Internal Error in Stack(OOM). Fail to doubling! : Limit: %d\n", s->limit);
            ylinterpret_undefined(YLErr_internal);
        }
        memcpy(tmp, s->item, sizeof(void*) * s->sz);
        ylfree(s->item);
        s->item = tmp;
        s->limit *= 2;
    }
    s->item[s->sz++] = item;
}

static inline void*
ylstk_pop(ylstk_t* s) {
    if(s->sz) {
        return s->item[--s->sz];
    } else {
        yllogE0("Internal Error in Stack. Try to pop on empty stack!\n");
        ylinterpret_undefined(YLErr_internal);
    }
    return NULL; /* to make compiler happy */
}

static inline void*
ylstk_peek(ylstk_t* s) {
    if(s->sz) {
        return s->item[s->sz-1];
    } else {
        yllogE0("Internal Error in Stack. Try to peek on empty stack!\n");
        ylinterpret_undefined(YLErr_internal);
    }
    return NULL; /* to make compiler happy */
}

#define stack_foreach(s, p, i)                          \
    for(i=0, p=s->item[i]; i<s->sz; i++, p=s->item[i])


#endif /* ___STACk_h___ */
