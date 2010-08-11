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
#include <stdlib.h>
#include <dirent.h>
#include <memory.h>

#include "ylsfunc.h"


YLDEFNF(shell, 1, 1) {
#define __INITBSZ 4096 /* initial buffer size - usually page size is 4KB*/
#define __cleanup_process()                             \
    do {                                                \
        /* clean resources */                           \
        if(buf) { ylfree(buf); }                        \
        if(pout) { pclose(pout); }                      \
    } while(0)

    FILE*  pout = NULL;
    char*  buf = NULL;

    /* check input parameter */
    ylcheck_chain_atom_type1(shell, e, YLASymbol);

    pout = popen(ylasym(ylcar(e)).sym, "r");
    if(!pout) { goto bail; }
    
    /* Read data from redirected stdout */
    { /* just scope */
        unsigned int rsz, available_sz;
        char  *p, *pend;

        /* 4KB is initial size of buffer */
        p = buf = ylmalloc(__INITBSZ+1); /* '+1' for trailing NULL */
        pend = p + __INITBSZ;
        do {
            available_sz = pend - p;
            rsz = fread(p, 1, available_sz, pout);
            if(0 > rsz) {
                goto bail;
            } else if(rsz < available_sz) {
                /* OK. Success - add trailing NULL*/
                *(p + rsz) = 0;
                break;
            } else {
                /* not enough buffer. double it! */
                char*          tmp;
                unsigned int   bsz; /* buffer size */
                bsz = pend - buf;
                tmp = ylmalloc(bsz*2+1);/* '+1' for trailing NULL */
                if(!tmp) {
                    /* Not enough memory */
                    yllogE(("Not enough memory: required [%d]\n", bsz*2));
                    goto bail;
                }
                p = tmp + bsz;
                memcpy(tmp, buf, bsz);
                ylfree(buf);
                buf = tmp;
                pend = buf + bsz*2;
            }
        } while(1);
    } /* End of 'Just scope' */    


    { /* Just scope */
        yle_t*  r = ylmp_get_block();
        ylaassign_sym(r, buf);
        /* buffer is already assigned to expression. So, this SHOULD NOT be freed */
        buf = NULL; 
        __cleanup_process();
        return r;
    }

 bail:
    __cleanup_process();

    yllogE(("Could not run : %s\n", ylasym(ylcar(e)).sym));
    ylinterpret_undefined(YLErr_func_fail);

} YLENDNF(shell)

YLDEFNF(sleep, 1, 1) {
    /* check input parameter */
    ylcheck_chain_atom_type1(sleep, e, YLADouble);
    sleep((unsigned int)yladbl(ylcar(e)));
    return ylt();
} YLENDNF(sleep)

YLDEFNF(usleep, 1, 1) {
    /* check input parameter */
    ylcheck_chain_atom_type1(usleep, e, YLADouble);
    usleep((unsigned int)yladbl(ylcar(e)));
    return ylt();
} YLENDNF(usleep)

YLDEFNF(getenv, 1, 1) {
    char*  env;
    /* check input parameter */
    ylcheck_chain_atom_type1(getenv, e, YLASymbol);
    env = getenv(ylasym(ylcar(e)).sym);
    if(env) {
        yle_t*          r = ylmp_get_block();
        unsigned int    sz = strlen(env);
        char*           v = ylmalloc(sz+1);
        memcpy(v, env, sz);
        v[sz] = 0; /* trailing 0 */
        ylaassign_sym(r, v);
        return r;
    } else {
        return ylnil();
    }
} YLENDNF(getenv)


YLDEFNF(setenv, 2, 2) {
    char*  env;
    /* check input parameter */
    ylcheck_chain_atom_type1(setenv, e, YLASymbol);
    if(0 == setenv(ylasym(ylcar(e)).sym, ylasym(ylcadr(e)).sym, 1)) {
        return ylt();
    } else {
        return ylnil();
    }
} YLENDNF(setenv)

YLDEFNF(fread, 1, 1) {
    FILE*    fh = NULL;
    char*    buf = NULL;
    yle_t*   r;
    long int sz;

    ylcheck_chain_atom_type1(fread, e, YLASymbol);
    
    fh = fopen(ylasym(ylcar(e)).sym, "r");
    if(!fh) {
        yllogW(("<!fread!> Cannot open file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }

    /* do ylnot check error.. very rare to fail!! */
    fseek(fh, 0, SEEK_END);
    sz = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    buf = ylmalloc((unsigned int)sz+1); /* +1 for trailing 0 */
    if(!buf) {
        yllogW(("<!fread!> Not enough memory to load file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }

    if(sz != fread(buf, 1, sz, fh)) {
        yllogW(("<!fread!> Fail to read file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }
    buf[sz] = 0; /* add trailing 0 */
    
    r = ylmp_get_block();
    ylaassign_sym(r, buf);
    buf = NULL; /* to prevent from free */

    if(fh) { fclose(fh); }
    if(buf) { ylfree(buf); }
    return r;

 bail:
    if(fh) { fclose(fh); }
    if(buf) { ylfree(buf); }

    return ylnil();
} YLENDNF(fread)

YLDEFNF(freadb, 1, 1) {
    FILE*    fh = NULL;
    char*    buf = NULL;
    yle_t*   r;
    long int sz;

    ylcheck_chain_atom_type1(freadb, e, YLABinary);
    
    fh = fopen(ylasym(ylcar(e)).sym, "r");
    if(!fh) {
        yllogW(("<!freadb!> Cannot open file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }

    /* do ylnot check error.. very rare to fail!! */
    fseek(fh, 0, SEEK_END);
    sz = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    buf = ylmalloc((unsigned int)sz);
    if(!buf) {
        yllogW(("<!freadb!> Not enough memory to load file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }

    if(sz != fread(buf, 1, sz, fh)) {
        yllogW(("<!fread!> Fail to read file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }
    
    r = ylmp_get_block();
    ylaassign_bin(r, buf, sz);
    buf = NULL; /* to prevent from free */

    if(fh) { fclose(fh); }
    if(buf) { ylfree(buf); }

    return r;

 bail:
    if(fh) { fclose(fh); }
    if(buf) { ylfree(buf); }

    return ylnil();
} YLENDNF(freadb)

YLDEFNF(fwrite, 2, 2) {
    FILE*    fh = NULL;
    yle_t*   dat = ylcadr(e);
    void*    rawdata;
    unsigned int sz;

    /* parameter check */
    if( !(ylais_type(ylcar(e), YLASymbol)
          && (ylais_type(dat, YLASymbol) || ylais_type(dat, YLABinary))) ) {
        yllogE(("<!fwrite!> invalid parameter type\n"));
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
       
    fh = fopen(ylasym(ylcar(e)).sym, "w");
    if(!fh) {
        yllogW(("<!fwrite!> Cannot open file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }

    
    switch(ylatype(dat)) {
        case YLASymbol: {
            sz = strlen(ylasym(dat).sym);
            rawdata = ylasym(dat).sym;
        } break;
        case YLABinary: {
            sz = ylabin(dat).sz;
            rawdata = ylabin(dat).d;
        } break;
        default:
            ylassert(0);
    }

    if(sz != fwrite(rawdata, 1, sz, fh)) {
        yllogW(("<!fwrite!> Fail to write file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }
    
    if(fh) { fclose(fh); }
    return ylt();

 bail:
    if(fh) { fclose(fh); }
    return ylnil();
} YLENDNF(fwrite)


YLDEFNF(readdir, 1, 1) {
    DIR*              dip = NULL;
    yle_t*            re;      /* expression to return */
    const char*       dpath;

    ylcheck_chain_atom_type1(readdir, e, YLASymbol);
    dpath = ylasym(ylcar(e)).sym;
    dip = opendir(dpath);
    if(!dip) { 
        yllogW(("<!readdir!> Fail to open directory [%s]\n", dpath));
        goto bail; 
    }
 
    { /* just scope */
        struct dirent*    dit;
        yle_t             sentinel;
        unsigned int      len;
        char*             fname;
        yle_t            *ne, *t;  /* new exp / tail */
        
        /* initialize sentinel */
        ylpassign(&sentinel, ylnil(), ylnil());
        t = &sentinel;
        while(dit = readdir(dip)) {
            /* ignore '.', '..' in the directory */
            if(0 == strcmp(".", dit->d_name)
               || 0 == strcmp("..", dit->d_name)) { continue; }

            len = strlen(dit->d_name);
            fname = ylmalloc(len + sizeof(char));
            memcpy(fname, dit->d_name, len);
            fname[len] = 0; /* trailing 0 */
            ne = ylmp_get_block();
            ylaassign_sym(ne, fname);
            ylpsetcdr(t, ylcons(ne, ylnil()));
            t = ylcdr(t);
        }
        re = ylcdr(&sentinel);
    }
    
    closedir(dip);
    return re;
    
 bail:
    if(dip) { closedir(dip); }
    return ylnil();
    
} YLENDNF(readdir)
