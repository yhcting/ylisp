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

/* enable logging & debugging */
#define __ENABLE_ASSERT
#define __ENABLE_LOG

#include "ylsfunc.h"


/*
 * Size of string buffer that represents number.
 * This should be enough size.
 * There is NO error check routine for this!
 */
#define _NUMSTRSZ  31


YLDEFNF(length, 1, 1) {
    yle_t*    r;

    ylnfcheck_atype_chain1(e, YLASymbol);

    r = ylmp_get_block();
    ylaassign_dbl( r, strlen(ylasym(ylcar(e)).sym) );
    return r;
    
} YLENDNF(length)

YLDEFNF(itos, 1, 1) {
    long      l;
    char*     b; /* buffer */
    yle_t*    r;

    ylnfcheck_atype_chain1(e, YLADouble);

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

    ylnfcheck_atype_chain1(e, YLADouble);

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
    ylnfcheck_atype_chain1(e, YLABinary);

    /*
     * 2 byte to represent char to hex
     * 1 byte for space between byte.
     */
    bsz = ylabin(ylcar(e)).sz*3;
    /* +1 for trailing 0 */
    b = ylmalloc(bsz+1); 
    if(!b) {
        ylnflogE1("Not enough memory: required [%d]\n", bsz);
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

    ylnfcheck_atype_chain1(e, YLASymbol);

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
    ylnfcheck_atype_chain1(e, YLASymbol);

    /* calculate total string length */
    len = 0; pe = e;
    while(!yleis_nil(pe)) {
        len += strlen(ylasym(ylcar(pe)).sym);
        pe = ylcdr(pe);
    }

    /* alloc memory and start to copy */
    buf = ylmalloc(len*sizeof(char) +1); /* '+1' for trailing 0 */
    if(!buf) {
        ylnflogE1("Not enough memory: required [%d]\n", len);
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


YLDEFNF(at, 2, 2) {
    int         idx;
    int         len;
    const char* p;
    char*       ch;
    yle_t*      r;

    /* check parameter type */
    ylnfcheck_atype1(ylcar(e), YLASymbol);
    ylnfcheck_atype1(ylcadr(e), YLADouble);

    /* check index range */
    idx = (int)yladbl(ylcadr(e));
    p = ylasym(ylcar(e)).sym;
    len = strlen(p);
    if(0 > idx || idx >= len) {
        ylnflogE0("invalid index value\n"); 
        ylinterpret_undefined(YLErr_func_invalid_param);
    }

    p += idx;
    r = ylmp_get_block();
    ch = ylmalloc(sizeof(char)*2); /* 1 for tailing NULL */
    *ch = *p;
    ch[1] = 0; /* trailing NULL */
    ylaassign_sym(r, ch);
    return r;

} YLENDNF(at)


YLDEFNF(compare, 2, 2) {
    yle_t* r = ylmp_get_block();

    /* check input parameter */
    ylnfcheck_atype_chain1(e, YLASymbol);

    ylaassign_dbl(r, strcmp(ylasym(ylcar(e)).sym, ylasym(ylcadr(e)).sym));
    return r;
} YLENDNF(compare)


YLDEFNF(end_with, 2, 2) {
    unsigned int  strsz, subsz;
    const char   *pstr, *psub;

    /* check input parameter */
    ylnfcheck_atype_chain1(e, YLASymbol);

    pstr = ylasym(ylcar(e)).sym;
    psub = ylasym(ylcadr(e)).sym;
    strsz = strlen(pstr);
    subsz = strlen(psub);

    /* filter trivial case */
    if(0 == subsz || 0 == strsz || subsz > strsz) { return ylnil(); }

    if(0 == memcmp(pstr+(strsz-subsz), psub, subsz)) { return ylt(); }
    else { return ylnil(); }
} YLENDNF(end_with)


YLDEFNF(start_with, 2, 3) {
    unsigned int  strsz, subsz;
    const char   *pstr, *psub;
    int           fromi;

    /* check parameter type */
    ylnfcheck_atype1(ylcar(e), YLASymbol);
    ylnfcheck_atype1(ylcadr(e), YLASymbol);
    if(2 == pcsz) { fromi = 0; } 
    else {
        ylnfcheck_atype1(ylcaddr(e), YLADouble);
        fromi = (int)yladbl(ylcaddr(e));
    }
    
    pstr = ylasym(ylcar(e)).sym;
    psub = ylasym(ylcadr(e)).sym;
    strsz = strlen(pstr);
    subsz = strlen(psub);

    /* filter trivial case */
    if(0 == subsz || 0 == strsz || subsz > strsz) { return ylnil(); }

    /* check index range */
    if(0 > fromi || fromi >= strsz) {
        ylnflogE1("invalid index value : %d\n", fromi); 
        return ylnil();
    }

    if(0 == memcmp(pstr+fromi, psub, subsz)) { return ylt(); }
    else { return ylnil(); }
} YLENDNF(start_with)

YLDEFNF(index_of, 2, 3) {
    unsigned int  strsz, subsz;
    const char   *pstr, *psub;
    int           fromi;

    /* check parameter type */
    ylnfcheck_atype1(ylcar(e), YLASymbol);
    ylnfcheck_atype1(ylcadr(e), YLASymbol);
    if(2 == pcsz) { fromi = 0; } 
    else {
        ylnfcheck_atype1(ylcaddr(e), YLADouble);
        fromi = (int)yladbl(ylcaddr(e));
    }

    pstr = ylasym(ylcar(e)).sym;
    psub = ylasym(ylcadr(e)).sym;
    strsz = strlen(pstr);
    subsz = strlen(psub);

    /* filter trivial case */
    if(0 == subsz || 0 == strsz || subsz > strsz
       || fromi > (strsz - subsz)) { return ylnil(); }
    if(fromi < 0) { fromi = 0; }
    
    { /* just scope */
        int         remainsz = strsz - fromi;
        const char* p = pstr + fromi;
        while(remainsz >= subsz) {
            if(*p == *psub
               && 0 == memcmp(p, psub, subsz)) {
                break;
            }
            p++;
            remainsz--;
        }
        if(remainsz >= subsz) {
            yle_t*      r;
            r = ylmp_get_block();
            ylaassign_dbl(r, strsz-remainsz);
            return r;
        } else {
            return ylnil();
        }
    }
} YLENDNF(index_of)


YLDEFNF(last_index_of, 2, 3) {
    unsigned int  strsz, subsz;
    const char   *pstr, *psub;
    int           fromi;

    /* check parameter type */
    ylnfcheck_atype1(ylcar(e), YLASymbol);
    ylnfcheck_atype1(ylcadr(e), YLASymbol);
    if(2 == pcsz) { fromi = 0xfffffff; } /* set to bit enough */
    else {
        ylnfcheck_atype1(ylcaddr(e), YLADouble);
        fromi = (int)yladbl(ylcaddr(e));
    }
    
    pstr = ylasym(ylcar(e)).sym;
    psub = ylasym(ylcadr(e)).sym;
    strsz = strlen(pstr);
    subsz = strlen(psub);

    /* filter trivial case */
    if(0 == subsz || 0 == strsz || subsz > strsz
       || fromi < (subsz-1)) { return ylnil(); }
    if(fromi > strsz-1) { fromi = strsz-1; }
    
    { /* just scope */
        const char* p = pstr + fromi - subsz + 1; /* BE CAREFUL : +1 is essential! */
        while(p >= pstr) {
            if(*p == *psub
               && 0 == memcmp(p, psub, subsz)) {
                break;
            }
            p--;
        }
        if(p >= pstr) {
            yle_t*      r;
            r = ylmp_get_block();
            ylaassign_dbl(r, (int)(p - pstr));
            return r;
        } else {
            return ylnil();
        }
    }
} YLENDNF(last_index_of)


YLDEFNF(replace, 3, 3) {

#define __check_buf(sz)                                                 \
    while( (pbend-pb) < (sz) ) {                                        \
        char* tmp = ylmalloc((pbend-pbuf)*2 + 1); /* +1 for tailing 0 */ \
        memcpy(tmp, pbuf, pb-pbuf);                                     \
        ylfree(pbuf);                                                   \
        pbend = tmp + (pbend-pbuf)*2; /* exlude space for tailing 0 */  \
        pb = tmp + (pb - pbuf);                                         \
        pbuf = tmp;                                                     \
    }

    const char  *p, *pstr, *pold, *pnew, *pe;
    char        *pbuf, *pb, *pbend;
    unsigned int lenstr, lennew, lenold;
    yle_t*       r;
    /* check input parameter */
    ylnfcheck_atype_chain1(e, YLASymbol);
    
    pstr = p = ylasym(ylcar(e)).sym;
    lenstr = strlen(p);
    pold = ylasym(ylcadr(e)).sym;
    lenold = strlen(pold);
    pnew = ylasym(ylcaddr(e)).sym;
    lennew = strlen(pnew);

    r = ylmp_get_block();
    /* filter trivial case */
    if(0 == lenold || lenstr < lenold) { 
        /* nothing to replace! copy it and return! */
        pbuf = ylmalloc(lenstr+1); /* =1 for tailing NULL */
        strcpy(pbuf, p);
        ylaassign_sym(r, pbuf);
    } else {
        pe = pstr + lenstr - lenold + 1; 
        pb = pbuf = ylmalloc(lenstr + 1); /* initial size of buffer is same with string length */
        pbend = pb + lenstr; /* exclude space for trailing 0 */
        while(p < pe) {
            if(0 == memcmp(p, pold, lenold)) {
                __check_buf(lennew);
                memcpy(pb, pnew, lennew);
                pb += lennew; p += lenold;
            } else {
                *pb = *p;
                pb++; p++;
            }
        }
        { /* just scope */
            unsigned int remainsz = pstr - p + lenstr;
            __check_buf(remainsz);
            memcpy(pb, p, remainsz);
            pb += remainsz;
        }
        *pb = 0; /* add trailing NULL */
        ylaassign_sym(r, pbuf);
    }
    
    return r;

#undef __check_buf
    
} YLENDNF(replace)

YLDEFNF(substring, 2, 3) {
    int          bi, ei; /* begin index end index */
    const char*  pstr;
    unsigned int lenstr;
    

    /* check parameter type */
    ylnfcheck_atype1(ylcar(e), YLASymbol);
    ylnfcheck_atype1(ylcadr(e), YLADouble);
    if(pcsz > 2) { ylnfcheck_atype1(ylcaddr(e), YLADouble); }

    pstr = ylasym(ylcar(e)).sym;
    lenstr = strlen(pstr);

    bi = (int)yladbl(ylcadr(e));
    if(bi < 0) { bi = 0; }

    if( yleis_nil(ylcddr(e)) ) {
        ei = lenstr;
    } else {
        ei = (int)yladbl(ylcaddr(e));
        if(ei > lenstr) { ei = lenstr; }
    }

    if(bi > ei) { bi = ei; }

    { /* just scope */
        yle_t*  r;
        char*   tmp;     

        /* make sub string */
        tmp = ylmalloc(ei-bi+1); /* +1 for tailing 0 */
        tmp[ei-bi] = 0; /* add trailing 0 */
        memcpy(tmp, pstr+bi, ei-bi);
        
        r = ylmp_get_block();
        ylaassign_sym(r, tmp);
        return r;
    }
} YLENDNF(substring)

YLDEFNF(to_lower_case, 1, 1) {
    const char   *p, *pe;
    char         *pbuf, *pb;
    int           delta;
    yle_t*        r;
    
    ylnfcheck_atype_chain1(e, YLASymbol);
    
    p = ylasym(ylcar(e)).sym;
    pe = p + strlen(p);

    pb = pbuf = ylmalloc(pe - p + 1);
    pbuf[pe-p] = 0; /* add trailing 0 */

    delta = 'a' - 'A';
    while(p<pe) {
        if( 'A' <= *p && 'Z' >= *p ) { *pb = *p + delta; }
        else { *pb = *p; }
        pb++; p++;
    }
    
    r = ylmp_get_block();
    ylaassign_sym(r, pbuf);
    return r;
    
} YLENDNF(to_lower_case)

YLDEFNF(to_upper_case, 1, 1) {
    const char   *p, *pe;
    char         *pbuf, *pb;
    int           delta;
    yle_t*        r;
    
    ylnfcheck_atype_chain1(e, YLASymbol);
    
    p = ylasym(ylcar(e)).sym;
    pe = p + strlen(p);

    pb = pbuf = ylmalloc(pe - p + 1);
    pbuf[pe-p] = 0; /* add trailing 0 */

    delta = 'A' - 'a';
    while(p<pe) {
        if( 'a' <= *p && 'z' >= *p ) { *pb = *p + delta; }
        else { *pb = *p; }
        pb++; p++;
    }
    
    r = ylmp_get_block();
    ylaassign_sym(r, pbuf);
    return r;
    
} YLENDNF(to_upper_case)

YLDEFNF(trim, 1, 1) {
    const char    *ps, *pe, *p, *pend; /* p start / pend */
    char*          pbuf;
    yle_t*         r;

#define __is_ws(c) (' ' == (c) || '\n' == (c) || '\r' == (c) || '\t' == (c))
    
    ylnfcheck_atype_chain1(e, YLASymbol);

    p = ylasym(ylcar(e)).sym;
    pend = p + strlen(p);

    /* check leading white space */
    ps = p;
    while( ps < pend) {
        if(!__is_ws(*ps)) { break; }
        ps++;
    }

    /* check trailing white space */
    pe = pend;
    while(pe >= p) {
        pe--;
        if(!__is_ws(*pe)) { break; }
    }

    if(ps > pe) { pe = ps - 1; } /* check : all are white space */
    
    pbuf = ylmalloc(pe-ps+2); /* pe is inclusive(+1) and trailing 0(+1) */
    pbuf[pe-ps+1] = 0; /* add trailing 0 */
    
    memcpy(pbuf, ps, pe-ps+1);
    
    r = ylmp_get_block();
    ylaassign_sym(r, pbuf);
    return r;

#undef __is_ws
} YLENDNF(trim)
