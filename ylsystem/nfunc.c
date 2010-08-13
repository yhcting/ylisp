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
#include <unistd.h>
#include <memory.h>

/* enable logging & debugging */
#define __ENABLE_ASSERT
#define __ENABLE_LOG

#include "ylsfunc.h"

/*
 * Return newly allocated(by malloc) memory buffer that contains file data.
 * (free needed to be called to release memory)
 * @return NULL if fails.
 */
static char*
_read_file(FILE* fin) {
#define __INITBSZ 4096 /* initial buffer size - usually page size is 4KB*/
    unsigned int  rsz, /* rsz : size read */
                  asz; /* asz : available size */
    char         *pbuf, *pb, *pbend;

    /* 4KB is initial size of buffer */
    pb = pbuf = ylmalloc(__INITBSZ+1); /* '+1' for trailing NULL */
    pbend = pb + __INITBSZ;
    do {
        asz = pbend - pb;
        rsz = fread(pb, 1, asz, fin);
        if(0 > rsz) {
            goto bail;
        } else if(rsz < asz) {
            /* OK. Success - add trailing NULL*/
            *(pb + rsz) = 0;
            break;
        } else {
            /* not enough buffer. double it! */
            char*          tmp;
            unsigned int   bsz; /* buffer size */
            bsz = pbend - pbuf;
            tmp = ylmalloc(bsz*2+1);/* '+1' for trailing NULL */
            if(!tmp) { goto bail; }
            pb = tmp + bsz;
            memcpy(tmp, pbuf, bsz);
            ylfree(pbuf);
            pbuf = tmp;
            pbend = pbuf + bsz*2;
        }
    } while(1);
    return pbuf;

 bail:
    if(pbuf) { ylfree(pbuf); }
    return NULL;

#undef __INITBSZ
}


YLDEFNF(shell, 1, 1) {
#define __cleanup_process()                                     \
    do {                                                        \
        /* clean resources */                                   \
        if(data) { ylfree(data); }                              \
        if(pout) { pclose(pout); }                              \
    } while(0)

    FILE*  pout = NULL;
    char*  data = NULL;
    int    svstderr = -1;


    /* check input parameter */
    ylcheck_chain_atom_type1(system.shell, e, YLASymbol);

    pout = popen(ylasym(ylcar(e)).sym, "r");
    if(!pout) { goto bail; }
    
    data = _read_file(pout);
    if(!data) {
        yllog((YLLogE, "<!system.shell!> Not enough memory\n"));
        goto bail;
    }

    { /* Just scope */
        yle_t*  r = ylmp_get_block();
        ylaassign_sym(r, data);
        /* buffer is already assigned to expression. So, this SHOULD NOT be freed */
        data = NULL; 
        __cleanup_process();
        return r;
    }

 bail:
    __cleanup_process();

    yllog((YLLogE, "<!system.shell!> Could not run : %s\n", ylasym(ylcar(e)).sym));
    ylinterpret_undefined(YLErr_func_fail);

} YLENDNF(shell)

YLDEFNF(sleep, 1, 1) {
    /* check input parameter */
    ylcheck_chain_atom_type1(system.sleep, e, YLADouble);
    sleep((unsigned int)yladbl(ylcar(e)));
    return ylt();
} YLENDNF(sleep)

YLDEFNF(usleep, 1, 1) {
    /* check input parameter */
    ylcheck_chain_atom_type1(system.usleep, e, YLADouble);
    usleep((unsigned int)yladbl(ylcar(e)));
    return ylt();
} YLENDNF(usleep)

YLDEFNF(getenv, 1, 1) {
    char*  env;
    /* check input parameter */
    ylcheck_chain_atom_type1(system.getenv, e, YLASymbol);
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
    ylcheck_chain_atom_type1(system.setenv, e, YLASymbol);
    if(0 == setenv(ylasym(ylcar(e)).sym, ylasym(ylcadr(e)).sym, 1)) {
        return ylt();
    } else {
        return ylnil();
    }
} YLENDNF(setenv)

YLDEFNF(chdir, 1, 1) {
    ylcheck_chain_atom_type1(system.chdir, e, YLASymbol);
    if( 0 > chdir(ylasym(ylcar(e)).sym) ) {
        yllog((YLLogW, "<!system.chdir!> Fail to change directory to [ %s ]\n", ylasym(ylcar(e)).sym));
        return ylnil();
    } else {
        return ylt();
    }
} YLENDNF(chdir)

YLDEFNF(getcwd, 0, 0) {
    yle_t*   r = ylmp_get_block();
    /*
     * Passing NULL at getcwd (POSIX.1-2001) - libc4, libc5, glibc
     */
    ylaassign_sym(r, getcwd(NULL, 0));
    return r;
} YLENDNF(getcwd)


YLDEFNF(fread, 1, 1) {
    FILE*    fh = NULL;
    char*    buf = NULL;
    yle_t*   r;
    long int sz;

    ylcheck_chain_atom_type1(system.fread, e, YLASymbol);
    
    fh = fopen(ylasym(ylcar(e)).sym, "r");
    if(!fh) {
        yllog((YLLogW, "<!system.fread!> Cannot open file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }

    /* do ylnot check error.. very rare to fail!! */
    fseek(fh, 0, SEEK_END);
    sz = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    buf = ylmalloc((unsigned int)sz+1); /* +1 for trailing 0 */
    if(!buf) {
        yllog((YLLogW, "<!system.fread!> Not enough memory to load file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }

    if(sz != fread(buf, 1, sz, fh)) {
        yllog((YLLogW, "<!system.fread!> Fail to read file [%s]\n", ylasym(ylcar(e)).sym));
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

    ylcheck_chain_atom_type1(system.freadb, e, YLABinary);
    
    fh = fopen(ylasym(ylcar(e)).sym, "r");
    if(!fh) {
        yllog((YLLogW, "<!system.freadb!> Cannot open file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }

    /* do ylnot check error.. very rare to fail!! */
    fseek(fh, 0, SEEK_END);
    sz = ftell(fh);
    fseek(fh, 0, SEEK_SET);

    buf = ylmalloc((unsigned int)sz);
    if(!buf) {
        yllog((YLLogW, "<!system.freadb!> Not enough memory to load file [%s]\n", ylasym(ylcar(e)).sym));
        goto bail;
    }

    if(sz != fread(buf, 1, sz, fh)) {
        yllog((YLLogW, "<!system.fread!> Fail to read file [%s]\n", ylasym(ylcar(e)).sym));
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
        yllog((YLLogE, "<!system.fwrite!> invalid parameter type\n"));
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
       
    fh = fopen(ylasym(ylcar(e)).sym, "w");
    if(!fh) {
        yllog((YLLogW, "<!system.fwrite!> Cannot open file [%s]\n", ylasym(ylcar(e)).sym));
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
        yllog((YLLogW, "<!system.fwrite!> Fail to write file [%s]\n", ylasym(ylcar(e)).sym));
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

    ylcheck_chain_atom_type1(system.readdir, e, YLASymbol);
    dpath = ylasym(ylcar(e)).sym;
    dip = opendir(dpath);
    if(!dip) { 
        yllog((YLLogW, "<!system.readdir!> Fail to open directory [%s]\n", dpath));
        goto bail; 
    }
 
    { /* just scope */
        struct dirent*    dit;
        yle_t*            sentinel;
        unsigned int      len;
        char*             fname;
        yle_t            *ne, *t;  /* new exp / tail */
        
        /* initialize sentinel */
        sentinel = ylmp_get_block();
        ylpassign(sentinel, ylnil(), ylnil());
        t = sentinel;
        while(dit = readdir(dip)) {
            /* ignore '.', '..' in the directory */
            if(0 == strcmp(".", dit->d_name)
               || 0 == strcmp("..", dit->d_name)) { continue; }

            len = strlen(dit->d_name);
            fname = ylmalloc(len + 1); /* +1 for trailing 0 */
            memcpy(fname, dit->d_name, len);
            fname[len] = 0; /* trailing 0 */
            ne = ylmp_get_block();
            ylaassign_sym(ne, fname);
            ylpsetcdr(t, ylcons(ne, ylnil()));
            t = ylcdr(t);
        }
        re = ylcdr(sentinel);
    }
    
    closedir(dip);
    return re;
    
 bail:
    if(dip) { closedir(dip); }
    return ylnil();
    
} YLENDNF(readdir)
