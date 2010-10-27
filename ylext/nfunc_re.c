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

/* enable logging & debugging */
#define CONFIG_ASSERT
#define CONFIG_LOG

#include "ylsfunc.h"

/* out vector count : should be multiple of 3 */
#define _OVECCNT 60


enum {
    _OPT_GLOBAL   = 0x01,
};

static int
_get_pcre_option(const char* optstr) {
    int         opt = 0;
    while(*optstr) {
        switch(*optstr) {
            case 'i': opt |= PCRE_CASELESS;     break;
            case 'm': opt |= PCRE_MULTILINE;    break;
            case 's': opt |= PCRE_DOTALL;       break;
            case 'g': break; /* do nothing.. this is custom option */
            default:
                yllogE1("<!pcre_xxx!> Unsupported option! [%c]\n", *optstr);
                ylinterpret_undefined(YLErr_func_fail);
        }
        optstr++;
    }
    return opt;
}

static int
_get_custom_option(const char* optstr) {
    int         opt = 0;
    while(*optstr) {
        switch(*optstr) {
            case 'g': opt |= _OPT_GLOBAL;     break;
            /* skip error check intentionally - option string is already verified at '_get_pcre_option' */
        }
        optstr++;
    }
    return opt;
}

/*
 * @1 : pattern
 * @2 : subject
 * @3 : option
 */
YLDEFNF(re_match, 3, 3) {
    yle_t        *hd, *tl; /* head & tail */
    pcre*         re;
    int           err_offset, rc, opt;
    int           ovect[_OVECCNT];  /* out vector */
    const char   *pattern, *subject, *errmsg;


    ylnfcheck_atype_chain1(e, ylaif_sym());

    pattern = ylasym(ylcar(e)).sym;
    subject = ylasym(ylcadr(e)).sym;
    opt = _get_pcre_option(ylasym(ylcaddr(e)).sym);

    re = pcre_compile(pattern, opt, &errmsg, &err_offset, NULL);
    if(!re) {
        ylnflogE2("PCRE compilation failed.\n"
                  "    offset %d: %s\n", err_offset, errmsg);
        /* This is a kind of function parameter error!! */
        ylinterpret_undefined(YLErr_func_fail);
    }

    rc = pcre_exec(re, NULL, subject, strlen(subject), 0, 0, ovect, _OVECCNT);

    /* set head as sentinel */
    hd = tl = ylmp_block();
    ylpassign(hd, ylnil(), ylnil());
    if(rc >= 0) {
        unsigned int     i, len;
        char*            substr;
        for(i=0; i<rc; i++) {
            len = ovect[2*i+1]-ovect[2*i];
            substr = ylmalloc(len + 1); /* +1 for trailing 0 */
            memcpy(substr, subject+ovect[2*i], len);
            substr[len] = 0;
            ylpsetcdr(tl, ylcons(ylacreate_sym(substr), ylnil()));
            tl = ylcdr(tl);
        }
    } else if(PCRE_ERROR_NOMATCH == rc) {
        ; /* nothing to do */
    } else {
        /* error case */
        ylnflogE1("PCRE error in match [%d]\n", rc);
        ylinterpret_undefined(YLErr_func_fail);
    }
    return ylcdr(hd);
} YLENDNF(re_match)



/*
 * @1: pattern
 * @2: string that substitute matched one
 * @3: subject
 * @4: option
 */
YLDEFNF(re_replace, 4, 4) {
    ylerr_t       interp_err = YLErr_func_fail;
    pcre*         re;
    char*         subject = NULL;

    ylnfcheck_atype_chain1(e, ylaif_sym());

    { /* Just scope */
        const char   *pattern, *errmsg;
        int           err_offset, opt;
        pattern = ylasym(ylcar(e)).sym;
        /* get pcre option */
        opt = _get_pcre_option(ylasym(ylcar(ylcdddr(e))).sym);

        re = pcre_compile(pattern, opt, &errmsg, &err_offset, NULL);
        if(!re) {
            ylnflogE2("PCRE compilation failed.\n"
                      "    offset %d: %s\n", err_offset, errmsg);
            /* This is a kind of function parameter error!! */
            goto bail;
        }
    }

    { /* Just scope */
        int           rc, opt;
        unsigned int  subjlen, substlen;
        unsigned int  offset; /* starting offset */
        int           ovect[_OVECCNT];  /* out vector */
        const char*   subst;

        subst = ylasym(ylcadr(e)).sym;
        /* get custom option */
        opt = _get_custom_option(ylasym(ylcar(ylcdddr(e))).sym);

        /* use copied one */
        subject = ylmalloc(strlen(ylasym(ylcaddr(e)).sym)+1);
        if(!subject) {
            ylnflogE0("Out of memory\n");
            interp_err = YLErr_out_of_memory;
            goto bail;
        }
        strcpy(subject, ylasym(ylcaddr(e)).sym);
        subjlen = strlen(subject);
        offset = 0;

        substlen = strlen(subst);
        /* start replacing */
        do {
            rc = pcre_exec(re, NULL, subject+offset, subjlen-offset, 0, 0, ovect, _OVECCNT);
            if(rc >= 0) {
                unsigned int newsz;
                char*        newstr = NULL;
                char*        p;

                newsz = subjlen - (ovect[1] - ovect[0]) + substlen;
                p = newstr = ylmalloc(newsz +1); /* +1 for trailing NULL */
                if(!p) {
                    ylnflogE0("Out of memory\n");
                    interp_err = YLErr_out_of_memory;
                    goto bail;
                }
                memcpy(p, subject, ovect[0]+offset);
                p += ovect[0]+offset;
                memcpy(p, subst, substlen);
                p += substlen;
                memcpy(p, subject+ovect[1]+offset, subjlen-ovect[1]-offset);
                newstr[newsz] = 0; /* trailing NULL */

                ylfree(subject);
                subject = newstr;
                subjlen = strlen(subject);
                offset = ovect[0]+offset+substlen;
            } else if(PCRE_ERROR_NOMATCH == rc) {
                /* nothing to do anymore */
                break;
            } else {
                /* error case */
                ylnflogE1("PCRE error in match [%d]\n", rc);
                goto bail;
            }
        } while (opt & _OPT_GLOBAL);
    }

    return ylacreate_sym(subject);

 bail:
    if(subject) { ylfree(subject); }
    ylinterpret_undefined(interp_err);
    return NULL; /* to make compiler be happy. */
} YLENDNF(re_replace)
