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



#include <stdio.h>
#include <string.h>
#include <memory.h>

#include "ylsfunc.h"


/*
 * Size of string buffer that represents number.
 * This should be enough size.
 * There is NO error check routine for this!
 */
#define _NUMSTRSZ  31


YLDEFNF(length, 1, 1) {
    yle_t*    r;

    ylcheck_chain_atom_type1(itos, e, YLASymbol);

    r = ylmp_get_block();
    ylaassign_dbl( r, strlen(ylasym(ylcar(e)).sym) );
    return r;
    
} YLENDNF(length)

YLDEFNF(itos, 1, 1) {
    long      l;
    char*     b; /* buffer */
    yle_t*    r;

    ylcheck_chain_atom_type1(itos, e, YLADouble);

    l = (long)yladbl(ylcar(e));
    b = ylmalloc(_NUMSTRSZ);
    sprintf(b, "%ld", l);
    r = ylmp_get_block();
    ylaassign_sym(r, b);
    return r;
    
} YLENDNF(itos)

YLDEFNF(dtos, 1, 1) {
    char*     b; /* buffer */
    yle_t*    r;

    ylcheck_chain_atom_type1(itos, e, YLADouble);

    b = ylmalloc(_NUMSTRSZ);
    sprintf(b, "%f", yladbl(ylcar(e)));
    r = ylmp_get_block();
    ylaassign_sym(r, b);
    return r;
} YLENDNF(dtos)


static const char _hextbl[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

/*
 * NOT TESTED YET!!!!
 */
YLDEFNF(btos, 1, 1) {
    yle_t*         r;
    char*          b;  /* buffer */
    const char*    ps; /* ps : source pointer */
    char*          pd; /* pd : destination pointer */
    unsigned int   bsz;
    register int   i;
    ylcheck_chain_atom_type1(btos, e, YLABinary);

    /*
     * 2 byte to represent char to hex
     * 1 byte for space between byte.
     */
    bsz = ylabin(ylcar(e)).sz*3;
    /* +1 for trailing 0 */
    b = ylmalloc(bsz+1); 
    if(!b) {
        yllogE(("Not enough memory: required [%d]\n", bsz));
        ylinterpret_undefined(YLErr_func_fail);
    }

    for(i=0, ps=ylabin(ylcar(e)).d, pd = b;
        i<ylabin(ylcar(e)).sz; i++) {
        *pd = _hextbl[(*ps>>4)&0x0f]; pd++;
        *pd = _hextbl[*ps & 0x0f]; pd++; ps++;
        *pd = ' '; pd++; /* add space */
    }
    *(pd-1) = 0; /* replace last space with trailing 0 */

    r = ylmp_get_block();
    ylaassign_sym(r, b);
    return r;

} YLENDNF(btos)


YLDEFNF(split_to_line, 1, 1) {
    char     *p, *ps, *ln;
    yle_t*   lne; /* pair E, line E */
    yle_t    *rh, *rt;  /* return head, return tail */
    unsigned int len;

    ylcheck_chain_atom_type1(itos, e, YLASymbol);

    /* get dummy pair head */
    rt = rh = ylmp_get_block();
    ylpassign(rh, ylnil(), ylnil());

    p = ylasym(ylcar(e)).sym;;
    while(1) {
        len = 0;
        ps = p;

        /* count line length */
        while(0 != *p && '\n' != *p) { p++; len++; }

        /* remove last carrage return */
        if('\r' == *(p-1)) { len--; }

        ln = ylmalloc(len+1); /* '+1' for trailing 0 */
        ylassert(ln); /* TODO: exception handling! - But not critical.. this happends rarely */
        memcpy(ln, ps, len);
        ln[len] = 0; /* add trailing 0 */

        /* now we got line. Let's make expression and append to the list*/
        lne = ylmp_get_block();
        ylaassign_sym(lne, ln);
        ylpsetcdr(rt, ylcons(lne, ylnil()));
        rt = ylcdr(rt);

        if(0 == *p || 0 == *(p+1)) { break; } /* it's end */
        else { p++; } /* move to next character */
    }

    return ylcdr(rh);

} YLENDNF(split_to_line)


YLDEFNF(concat, 2, 9999) {
    char*            buf = NULL;
    unsigned int     len;
    yle_t*           pe;
    char*            p;

    /* check input parameter */
    ylcheck_chain_atom_type1(concat, e, YLASymbol);

    /* calculate total string length */
    len = 0; pe = e;
    while(!yleis_nil(pe)) {
        len += strlen(ylasym(ylcar(pe)).sym);
        pe = ylcdr(pe);
    }

    /* alloc memory and start to copy */
    buf = ylmalloc(len*sizeof(char) +1); /* '+1' for trailing 0 */
    if(!buf) {
        yllogE(("Not enough memory: required [%d]\n", len));
        ylinterpret_undefined(YLErr_func_fail);
    }

    pe = e; p = buf;
    while(!yleis_nil(pe)) {
        strcpy(p, ylasym(ylcar(pe)).sym);
        p += strlen(ylasym(ylcar(pe)).sym);
        pe = ylcdr(pe);
    }
    *p = 0; /* trailing 0 */

    pe = ylmp_get_block();
    ylaassign_sym(pe, buf);
    return pe;

} YLENDNF(concat)
