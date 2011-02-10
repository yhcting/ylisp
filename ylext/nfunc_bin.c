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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* =========================================
 * We may support unicode later.
 * So, 'Binary' type cannot be put here even if
 *  it's almost same in terms of handling 'ascii' string.
 * =========================================*/

#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "ylsfunc.h"
#include "yldynb.h"

static const char _hextbl[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};


YLDEFNF(binlen, 1, 1) {
    ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_bin()));
    return ylacreate_dbl(ylabin(ylcar(e)).sz);
} YLENDNF(binlen)

/*
 * NOT TESTED YET!!!!
 */
YLDEFNF(bin_human_read, 1, 1) {
    unsigned char*          b;  /* buffer */
    const unsigned char*    ps; /* ps : source pointer */
    unsigned char*          pd; /* pd : destination pointer */
    unsigned int            bsz;
    register int            i;

    ylnfcheck_parameter(ylais_type_chain(e, ylaif_bin()));

    /*
     * 2 byte to represent char to hex
     * 1 byte for space between byte.
     */
    bsz = ylabin(ylcar(e)).sz*3;
    /* +1 for trailing 0 */
    b = ylmalloc(bsz+1);
    if (!b)
        ylnfinterp_fail (YLErr_func_fail, "Not enough memory: required [%d]\n", bsz);

    for(i=0, ps=ylabin(ylcar(e)).d, pd = b;
        i<ylabin(ylcar(e)).sz; i++) {
        *pd = _hextbl[(*ps>>4)&0x0f]; pd++;
        *pd = _hextbl[*ps & 0x0f]; pd++; ps++;
        *pd = ' '; pd++; /* add space */
    }
    *(pd-1) = 0; /* replace last space with trailing 0 */

    return ylacreate_sym((char*)b);

} YLENDNF(btos)


YLDEFNF(subbin, 2, 3) {
    long long             bi, ei; /* begin index end index */
    const unsigned char*  p;
    unsigned int          sz;


    /* check parameter type */
    ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_bin())
                        && ylais_type(ylcadr(e), ylaif_dbl())
                        && (pcsz < 3 || ylais_type(ylcaddr(e), ylaif_dbl())) );

    p = ylabin(ylcar(e)).d;
    sz = ylabin(ylcar(e)).sz;

    bi = (long long)yladbl(ylcadr(e));
    if(bi < 0) { bi = 0; }

    if( yleis_nil(ylcddr(e)) ) {
        ei = sz;
    } else {
        ei = (long long)yladbl(ylcaddr(e));
        if(ei > sz) { ei = sz; }
    }

    if(bi > ei) { bi = ei; }

    { /* just scope */
        unsigned char*   tmp;
        /* make sub string */
        tmp = ylmalloc(ei-bi);
        memcpy(tmp, p+bi, ei-bi);
        return ylacreate_bin(tmp, ei-bi);
    }
} YLENDNF(subbin)

YLDEFNF(concat_bin, 2, 9999) {
    yldynb_t   b;
    int        sz; /* size overflow is not handled, yet!. 2GB limit */

    { /* Just scope */
        yle_t* w = e;
        /* parameter check */
        ylelist_foreach(w) {
            ylnfcheck_parameter(ylais_type(ylcar(w), ylaif_sym())
                                || ylais_type(ylcar(w), ylaif_bin()));
        }
    }

    yldynb_init(&b, 4096);
    ylelist_foreach(e) {
        if(ylais_type(ylcar(e), ylaif_sym())) {
            sz = strlen(ylasym(ylcar(e)).sym);
            if(0 > yldynb_append(&b, (unsigned char*)ylasym(ylcar(e)).sym,
                                  (unsigned int)sz)) {
                goto bail;
            }
        } else {
            if(0 > yldynb_append(&b, ylabin(ylcar(e)).d,
                                                   ylabin(ylcar(e)).sz)) {
                goto bail;
            }
        }
    }

    { /* Just scope */
        unsigned char* p;
        sz = yldynb_sz(&b);
        p = ylmalloc(sz);
        if(!p) { goto bail; }
        memcpy(p, yldynb_buf(&b), yldynb_sz(&b));
        yldynb_clean(&b);

        return ylacreate_bin(p, sz);
    }

 bail:
    yldynb_clean(&b);
    ylnfinterp_fail (YLErr_out_of_memory, "Out Of Memory\n");
} YLENDNF(concat_bin)

YLDEFNF(to_bin, 1, 2) {
    ylnfcheck_parameter(yleis_atom(ylcar(e))
                        && (pcsz < 2 || ylais_type(ylcadr(e), ylaif_dbl())));
    if(ylais_type(ylcar(e), ylaif_bin())) {
        /* nothing to do. trivial case. return self. */
        return ylcar(e);

    } else if(ylais_type(ylcar(e), ylaif_sym())) {
        int            sz = strlen(ylasym(ylcar(e)).sym);
        unsigned char* b = ylmalloc(sz);
        if (!b)
            ylnfinterp_fail (YLErr_out_of_memory, "Out Of Memory\n");
        memcpy(b, ylasym(ylcar(e)).sym, sz);
        return ylacreate_bin(b, (unsigned int)sz);

    } else if(ylais_type(ylcar(e), ylaif_dbl())) {
        long long       sz;
        long long       l = (long long)yladbl(ylcar(e));
        unsigned char*  b;

        /* checking parameter!! */
        if((double)l != yladbl(ylcar(e))) {
            ylnflogE ("Double and long long values mismatch.\n");
            goto bail;
        }
        if(pcsz < 2) {
            ylnflogE ("Size in bytes (2nd parameter) is required!.\n");
            goto bail;
        }
        sz = (long long)yladbl(ylcadr(e));
        if(sz < 0 || sz > sizeof(l)) { goto bail; }

        b = ylmalloc(sz);
        ylassert(b);
        memcpy(b, &l, sz);
        return ylacreate_bin(b, (unsigned int)sz);

    } else {
        ylnflogE ("Not Supported Atom Type!\n");
        goto bail;
    }

 bail:
    ylnfinterp_fail (YLErr_func_invalid_param, "invalid parameter type\n");
} YLENDNF(to_bin)

YLDEFNF(bin_to_num, 1, 1) {
    long long n = 0;
    /* usually, double has 52bit mantissa */
    ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_bin())
                        && ylabin(ylcar(e)).sz < sizeof(n));
    memcpy(&n, ylabin(ylcar(e)).d, ylabin(ylcar(e)).sz);
    if ((double)n != n)
        ylnfinterp_fail (YLErr_func_invalid_param, "Double and long long values mismatch.\n");
    return ylacreate_dbl((double)n);
} YLENDNF(bin_to_num)
