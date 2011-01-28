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


#include <stdio.h>
#include <string.h>
#include <memory.h>

#ifdef HAVE_LIBPCRE
#   include "pcre.h"
#else /* HAVE_LIBPCRE */
#   include <regex.h>
#endif /* HAVE_LIBPCRE */

#include "ylsfunc.h"


enum {
    _OPT_GLOBAL   = 0x01,
};

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


#ifdef HAVE_LIBPCRE
/*
 * !!! NOTE !!!
 * PCRE functions are 'Thread Safe'!
 * So, we don't need to worry about Multi-Threaded-Evaluation.
 * (See MULTITHREADING section in "http://www.pcre.org/pcre.txt")
 */

/* out vector count : should be multiple of 3 */
#define _OVECCNT 60


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
                yllogE ("<!pcre_xxx!> Unsupported option! [%c]\n", *optstr);
                ylinterpret_undefined(YLErr_func_fail);
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

    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

    pattern = ylasym(ylcar(e)).sym;
    subject = ylasym(ylcadr(e)).sym;
    opt = _get_pcre_option(ylasym(ylcaddr(e)).sym);

    re = pcre_compile(pattern, opt, &errmsg, &err_offset, NULL);
    if(!re) {
        ylnflogE ("PCRE compilation failed.\n"
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
        ylnflogE ("PCRE error in match [%d]\n", rc);
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

    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

    { /* Just scope */
        const char   *pattern, *errmsg;
        int           err_offset, opt;
        pattern = ylasym(ylcar(e)).sym;
        /* get pcre option */
        opt = _get_pcre_option(ylasym(ylcar(ylcdddr(e))).sym);

        re = pcre_compile(pattern, opt, &errmsg, &err_offset, NULL);
        if(!re) {
            ylnflogE ("PCRE compilation failed.\n"
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
            ylnflogE ("Out of memory\n");
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
                    ylnflogE ("Out of memory\n");
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
                ylnflogE ("PCRE error in match [%d]\n", rc);
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

#else /* HAVE_LIBPCRE */

static int
_get_re_option (const char* optstr) {
    int         opt = REG_EXTENDED | REG_NEWLINE;
    while (*optstr) {
        switch (*optstr) {
            case 'i': opt |= REG_ICASE;       break;
            case 'm': opt &= ~REG_NEWLINE;    break;
            case 'g': break; /* do nothing.. this is custom option */
            default:
                yllogE ("<!re_xxx!> Unsupported option! [%c]\n", *optstr);
                ylinterpret_undefined (YLErr_func_fail);
        }
        optstr++;
    }
    return opt;
}



#define __ERRBUFSZ 4096

/*
 * @1 : pattern
 * @2 : subject
 * @3 : option
 */
YLDEFNF(re_match, 3, 3) {
    yle_t       *hd, *tl; /* head & tail */
    char         b[__ERRBUFSZ];
    regex_t*     re = NULL;
    regmatch_t*  rm = NULL;
    int          r, opt;
    const char  *pattern, *subject;

    ylnfcheck_parameter (ylais_type_chain (e, ylaif_sym ()));

    pattern = ylasym (ylcar (e)).sym;
    subject = ylasym (ylcadr (e)).sym;
    opt = _get_re_option (ylasym (ylcaddr (e)).sym);

    re = ylmalloc (sizeof (*re));
    r = regcomp (re, pattern, opt);
    if (r < 0) {
        regerror (r, re, b, __ERRBUFSZ);
        ylnflogE ("RE compilation failed.\n"
                  "    %s\n", b);
        goto bail;
    }

    rm = ylmalloc (sizeof (*rm) * (re->re_nsub + 1));
    ylassert (rm);
    r = regexec (re, subject, re->re_nsub + 1, rm, 0);

    /* set head as sentinel */
    hd = tl = ylmp_block ();
    ylpassign(hd, ylnil (), ylnil ());
    if (!r) {
        /* Matched!! */
        unsigned int     i, len;
        char*            substr;
        for (i=0; i<re->re_nsub + 1; i++) {
            len = rm[i].rm_eo - rm[i].rm_so;
            substr = ylmalloc (len + 1); /* +1 for trailing 0 */
            memcpy (substr, subject + rm[i].rm_so, len);
            substr[len] = 0;
            ylpsetcdr (tl, ylcons (ylacreate_sym (substr), ylnil ()));
            tl = ylcdr (tl);
        }
    } else if (REG_NOMATCH == r) {
        ;/* nothing to do at this case */
    } else {
        regerror (r, re, b, __ERRBUFSZ);
        ylnflogE ("RE match failed.\n"
                  "    %s\n", b);
        goto bail;
    }

    regfree (re); ylfree (re);
    ylfree (rm);
    return ylcdr (hd);

 bail:
    if (re) { regfree (re); ylfree (re); }
    if (rm) ylfree (rm);
    ylinterpret_undefined (YLErr_func_fail);
    return NULL; /* to make compiler be happy */

} YLENDNF(re_match)

/*
 * @1: pattern
 * @2: string that substitute matched one
 * @3: subject
 * @4: option
 */
YLDEFNF(re_replace, 4, 4) {
    ylerr_t       interp_err = YLErr_func_fail;
    int           r;
    char          b[__ERRBUFSZ];
    regex_t*      re = NULL;
    char*         subject = NULL;

    ylnfcheck_parameter (ylais_type_chain (e, ylaif_sym ()));

    { /* Just scope */
        const char   *pattern;
        int           opt;
        pattern = ylasym (ylcar (e)).sym;
        /* get pcre option */
        opt = _get_re_option (ylasym (ylcar (ylcdddr (e))).sym);
        re = ylmalloc (sizeof (*re));

        r = regcomp (re, pattern, opt);
        if (r < 0) {
            regerror (r, re, b, __ERRBUFSZ);
            ylnflogE ("RE compilation failed.\n"
                      "    %s\n", b);
            goto bail;
        }
    }

    { /* Just scope */
        int           opt;
        unsigned int  subjlen, substlen;
        unsigned int  offset; /* starting offset */
        regmatch_t    rm;
        const char*   subst;

        subst = ylasym (ylcadr (e)).sym;
        /* get custom option */
        opt = _get_custom_option (ylasym (ylcar (ylcdddr (e))).sym);

        /* use copied one */
        subject = ylmalloc (strlen (ylasym (ylcaddr (e)).sym) + 1);
        if (!subject) {
            ylnflogE ("Out of memory\n");
            interp_err = YLErr_out_of_memory;
            goto bail;
        }
        strcpy (subject, ylasym (ylcaddr (e)).sym);
        subjlen = strlen (subject);
        offset = 0;

        substlen = strlen (subst);
        /* start replacing */
        do {
            r = regexec (re, subject + offset, 1, &rm, 0);
            if (!r) {
                unsigned int newsz;
                char*        newstr = NULL;
                char*        p;

                newsz = subjlen - (rm.rm_eo - rm.rm_so) + substlen;
                p = newstr = ylmalloc (newsz + 1); /* +1 for trailing NULL */
                if (!p) {
                    ylnflogE ("Out of memory\n");
                    interp_err = YLErr_out_of_memory;
                    goto bail;
                }
                memcpy (p, subject, rm.rm_so + offset);
                p += rm.rm_so + offset;
                memcpy (p, subst, substlen);
                p += substlen;
                memcpy (p, subject + rm.rm_eo + offset, subjlen - rm.rm_eo - offset);
                newstr[newsz] = 0; /* trailing NULL */

                ylfree (subject);
                subject = newstr;
                subjlen = strlen (subject);
                offset = rm.rm_so + offset + substlen;
            } else if(REG_NOMATCH == r) {
                /* nothing to do anymore */
                break;
            } else {
                /* error case */
                regerror (r, re, b, __ERRBUFSZ);
                ylnflogE ("RE compilation failed.\n"
                          "    %s\n", b);
                goto bail;
            }
        } while (opt & _OPT_GLOBAL);
    }

    regfree (re); ylfree (re);
    return ylacreate_sym (subject);

 bail:
    if (re) { regfree (re); ylfree (re); }
    if (subject) ylfree (subject);
    ylinterpret_undefined (interp_err);
    return NULL; /* to make compiler be happy. */

} YLENDNF(re_replace)


#undef __ERRBUFSZ




#endif /* HAVE_LIBPCRE */



