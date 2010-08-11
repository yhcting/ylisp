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

#include "pcre.h"
#include "ylsfunc.h"

/* out vector count : should be multiple of 3 */
#define _OVECCNT 60


static int
_change_to_pcre_option(const char* optstr) {
    const char* p = optstr;
    int         opt = 0;
    while(*p) {
        switch(*p) {
            case 'i': opt |= PCRE_CASELESS;     break;
            case 'm': opt |= PCRE_MULTILINE;    break;
            case 's': opt |= PCRE_DOTALL;       break;
            default:
                yllogE(("Unsupported option! [%c]\n", *p));
                ylinterpret_undefined(YLErr_func_fail);
        }
        p++;
    }
    return opt;
}

/*
 * @1 : pattern
 * @2 : subject
 * @3 : option
 */
YLDEFNF(pcre_match, 3, 3) {
    yle_t        *hd, *tl; /* head & tail */
    pcre*         re;
    int           err_offset, rc, opt;
    int           ovect[_OVECCNT];  /* out vector */
    const char   *pattern, *subject, *errmsg;


    ylcheck_chain_atom_type1(itos, e, YLASymbol);

    pattern = ylasym(ylcar(e)).sym;
    subject = ylasym(ylcadr(e)).sym;
    opt = _change_to_pcre_option(ylasym(ylcaddr(e)).sym);

    re = pcre_compile(pattern, opt, &errmsg, &err_offset, NULL);
    if(!re) {
        yllogE(("PCRE compilation failed.\n"
                "offset %d: %s\n", err_offset, errmsg));
        /* This is a kind of function parameter error!! */
        ylinterpret_undefined(YLErr_func_fail);
    }

    rc = pcre_exec(re, NULL, subject, strlen(subject), 0, 0, ovect, _OVECCNT);
    
    /* set head as sentinel */
    hd = tl = ylmp_get_block();
    ylpassign(hd, ylnil(), ylnil());
    if(rc >= 0) {
        unsigned int     i, len;
        yle_t*           se; /* symbol expression */
        char*            substr;
        for(i=0; i<rc; i++) {
            se = ylmp_get_block();
            len = ovect[2*i+1]-ovect[2*i];
            substr = ylmalloc(len + 1); /* +1 for trailing 0 */
            memcpy(substr, subject+ovect[2*i], len);
            substr[len] = 0;
            ylaassign_sym(se, substr);
            ylpsetcdr(tl, ylcons(se, ylnil()));
            tl = ylcdr(tl);
        }
    } else if(PCRE_ERROR_NOMATCH == rc) {
        ; /* nothing to do */
    } else {
        /* error case */
        yllogE(("PCRE error in match [%d]\n", rc));
        ylinterpret_undefined(YLErr_func_fail);
    }
    return ylcdr(hd);
} YLENDNF(pcre-match)



/*
 * @1: pattern
 * @2: string that substitute matched one
 * @3: subject
 * @4: option
 */
YLDEFNF(pcre_replace, 4, 4) {
    yle_t*        r;
    pcre*         re;
    int           err_offset, rc, opt;
    unsigned int  subjlen;
    int           ovect[_OVECCNT];  /* out vector */
    const char   *pattern, *subst, *subject, *errmsg;

    ylcheck_chain_atom_type1(itos, e, YLASymbol);

    pattern = ylasym(ylcar(e)).sym;
    subst = ylasym(ylcadr(e)).sym;
    subject = ylasym(ylcaddr(e)).sym;
    opt = _change_to_pcre_option(ylasym(ylcar(ylcdddr(e))).sym);

    re = pcre_compile(pattern, opt, &errmsg, &err_offset, NULL);
    if(!re) {
        yllogE(("PCRE compilation failed.\n"
                "offset %d: %s\n", err_offset, errmsg));
        /* This is a kind of function parameter error!! */
        ylinterpret_undefined(YLErr_func_fail);
    }

    subjlen = strlen(subject);
    rc = pcre_exec(re, NULL, subject, subjlen, 0, 0, ovect, _OVECCNT);
    if(rc >= 0) {
        unsigned int substlen, newsz;
        char*        newstr = NULL;
        char*        p;    

        substlen = strlen(subst);
        newsz = subjlen - (ovect[1] - ovect[0]) + substlen;
        p = newstr = ylmalloc(newsz +1); /* +1 for trailing NULL */

        memcpy(p, subject, ovect[0]);
        p += ovect[0];
        memcpy(p, subst, substlen);
        p += substlen;
        memcpy(p, subject+ovect[1], subjlen - ovect[1]);
        newstr[newsz] = 0; /* trailing NULL */

        r = ylmp_get_block();
        ylaassign_sym(r, newstr);
        
    } else if(PCRE_ERROR_NOMATCH == rc) {
        ; /* nothing to do */
    } else {
        /* error case */
        yllogE(("PCRE error in match [%d]\n", rc));
        ylinterpret_undefined(YLErr_func_fail);
    }

    return r;
    
} YLENDNF(pcre-replace)
