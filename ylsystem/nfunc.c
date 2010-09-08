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



#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <memory.h>

/* enable logging & debugging */
#define __ENABLE_ASSERT
#define __ENABLE_LOG

#include "ylsfunc.h"
#include "ylut.h"

static char*
_alloc_str(const char* str) {
    char*        p;
    p = ylmalloc(strlen(str)+1);
    /* 
     * This function is used for small-length string. 
     * So, let's skip handling allocation-failure.
     */
    if(p) { strcpy(p, str); }
    else { ylassert(0); }
    return p;
}

/*
 * @outsz: 
 *    in case of fail: 0 means OK, otherwise error!
 */
static void*
_readf(unsigned int* outsz, const char* func, const char* fpath, int btext) {
    char*        buf = NULL;
    unsigned int sz;
    FILE*        fh;

    if(outsz) { *outsz = 1; } /* 1 means, 'not 0' */
    buf = ylutfile_read(&sz, fpath, btext);
    if(buf) { if(outsz){*outsz=sz;} }
    else {
        switch(sz) {
            case YLOk:
                if(outsz){*outsz = 0;}
            break;

            case YLErr_io:
                yllogW2("<!%s!> Error: File IO [%s]\n", func, fpath);
            break;

            case YLErr_out_of_memory:
                yllogW2("<!%s!> Not enough memory to load file [%s]\n", func, fpath);
            break;

            default:
                ylassert(0);
        }
    }
    return buf;
}


/*
 * FIX ME!
 * Using temporally file to redirect is not good solution, in my opinion.
 * I think definitely there is a way of using pipe!
 *
 *     I faced with some issues when I tried to use pipe to re-direct stdout and stderr.
 *     For example, keeping only one-page-data, block when pipe is empty.
 *     I'm not good at linux system programming.
 *     So, until I found solution, using file explicitly to redirect.
 */
YLDEFNF(shell, 1, 1) {
#define __outf_name ".#_______yljfe___outf______#"
#define __restore_redirection()                         \
    /* restore stdout/stderr */                         \
    if(saved_stdout >= 0) {                             \
        close(STDOUT_FILENO);                           \
        dup2(saved_stdout, STDOUT_FILENO);              \
        close(saved_stdout);                            \
        saved_stdout = -1;                              \
    }                                                   \
    if(saved_stderr >= 0) {                             \
        close(STDERR_FILENO);                           \
        dup2(saved_stderr, STDERR_FILENO);              \
        close(saved_stderr);                            \
        saved_stderr = -1;                              \
    }

#define __cleanup_process()                             \
    /* clean resources */                               \
    if(buf) { ylfree(buf); }                            \
    unlink(__outf_name);

    int         saved_stdout, saved_stderr;
    FILE*       ofh = NULL;
    char*       buf = NULL;
    const char* errmsg = NULL;

    /* check input parameter */
    ylnfcheck_atype_chain1(e, YLASymbol);

    /* set to invalid value */
    saved_stdout = saved_stderr = -1; 

    /* save stdout/stderr to restore later*/
    saved_stdout = dup(STDOUT_FILENO);
    saved_stderr = dup(STDERR_FILENO);
    if(saved_stdout < 0 || saved_stderr < 0) { 
        ylnflogE0("Fail to save stdout/stderr\n");
        goto bail; 
    }

    if( NULL == (ofh = fopen(__outf_name, "w")) ) {
        ylnflogE0("Fail to open file to redirect stdout/stderr!\n");
        goto bail;
    }

    /* before redirecting... flush stdout and stderr */
    fflush(stdout); fflush(stderr);

    /* redirect stderr to the pipe */
    if(0 > dup2(fileno(ofh), STDERR_FILENO)) { ylnflogE0("Fail to dup stderr\n"); goto bail; }
    /* redirect stdout to the pipe */
    if(0 > dup2(fileno(ofh), STDOUT_FILENO)) { ylnflogE0("Fail to dup stdout\n"); goto bail; }

    /* 
     * Now, we are ready to redirect 
     * It's time to execute shell command 
     */
    system(ylasym(ylcar(e)).sym);
    /* flush result */
    /*
     * ****** NOTE ******
     *    This SHOULD BE 'fflush(ofh)'!!!
     *    "fflush(stdout); fflush(stderr);" gives unexpected result!!!!
     */
    fflush(ofh);
    fclose(ofh);

    /*
     * stdout/stderr was redirected.
     * So, if system uses stdout/stderr as an outstream of print or log,
     *     this may not be shown because it's already redirected.
     * That's why redirection should be restored as sooon as possible.
     */
    __restore_redirection();

    { /* Just scope */
        FILE*  fh;
        fh = fopen(__outf_name, "r");
        if(fh) {
            /* file exists. So, there is valid output */
            fclose(fh);
            buf = _readf(NULL, "shell", __outf_name, TRUE);
            if(!buf) { ylnflogW0("Cannot read shell output!!\n"); goto bail; }
        } else {
            /* There is no output from command */
            buf = ylmalloc(1);
            buf[0] = 0;
        }
    }

    if(!buf) {
        ylnflogE0("Fail to read output result\n");
        goto bail;
    } else {
        yle_t* r = ylacreate_sym(buf);
        /* buffer is already assigned to expression. So, this SHOULD NOT be freed */
        buf = NULL; 
        __cleanup_process();
        return r;
    }        

 bail:
    __restore_redirection();
    __cleanup_process();
    ylinterpret_undefined(YLErr_func_fail);

#undef __cleanup_process
#undef __outf_name

} YLENDNF(shell)

YLDEFNF(sleep, 1, 1) {
    /* check input parameter */
    ylnfcheck_atype_chain1(e, YLADouble);
    sleep((unsigned int)yladbl(ylcar(e)));
    return ylt();
} YLENDNF(sleep)

YLDEFNF(usleep, 1, 1) {
    /* check input parameter */
    ylnfcheck_atype_chain1(e, YLADouble);
    usleep((unsigned int)yladbl(ylcar(e)));
    return ylt();
} YLENDNF(usleep)

YLDEFNF(getenv, 1, 1) {
    char*  env;
    /* check input parameter */
    ylnfcheck_atype_chain1(e, YLASymbol);
    env = getenv(ylasym(ylcar(e)).sym);
    if(env) {
        unsigned int    sz = strlen(env);
        char*           v = ylmalloc(sz+1);
        memcpy(v, env, sz);
        v[sz] = 0; /* trailing 0 */
        return ylacreate_sym(v);
    } else {
        return ylnil();
    }
} YLENDNF(getenv)

YLDEFNF(setenv, 2, 2) {
    char*  env;
    /* check input parameter */
    ylnfcheck_atype_chain1(e, YLASymbol);
    if(0 == setenv(ylasym(ylcar(e)).sym, ylasym(ylcadr(e)).sym, 1)) {
        return ylt();
    } else {
        return ylnil();
    }
} YLENDNF(setenv)

YLDEFNF(chdir, 1, 1) {
    ylnfcheck_atype_chain1(e, YLASymbol);
    if( 0 > chdir(ylasym(ylcar(e)).sym) ) {
        ylnflogW1("Fail to change directory to [ %s ]\n", ylasym(ylcar(e)).sym);
        return ylnil();
    } else {
        return ylt();
    }
} YLENDNF(chdir)

YLDEFNF(getcwd, 0, 0) {
    /*
     * Passing NULL at getcwd (POSIX.1-2001) - libc4, libc5, glibc
     */
    return ylacreate_sym(getcwd(NULL, 0));
} YLENDNF(getcwd)

YLDEFNF(fstat, 1, 1) {
    /* Not tested yet! */
    
    struct stat    st;
    yle_t         *r, *key, *v;
    ylnfcheck_atype_chain1(e, YLASymbol);
    if(0 > stat(ylasym(ylcar(e)).sym, &st)) {
        ylnflogE1("Cannot get status of file [%s]\n", ylasym(ylcar(e)).sym);
        return ylnil();
    }

    /* make 'type' pair */
    key = ylacreate_sym(_alloc_str("type"));
    switch(st.st_mode & S_IFMT) {
        case S_IFSOCK: v = ylacreate_sym(_alloc_str("s"));  break;
        case S_IFLNK:  v = ylacreate_sym(_alloc_str("l"));  break;
        case S_IFREG:  v = ylacreate_sym(_alloc_str("f"));  break;
        case S_IFBLK:  v = ylacreate_sym(_alloc_str("b"));  break;
        case S_IFDIR:  v = ylacreate_sym(_alloc_str("d"));  break;
        case S_IFCHR:  v = ylacreate_sym(_alloc_str("c"));  break;
        case S_IFIFO:  v = ylacreate_sym(_alloc_str("p"));  break;
        default: v = ylacreate_sym(_alloc_str("u")); /* unknown */
    }
    /* make r as '((key v)) */
    r = ylcons(yllist(key, v), ylnil());
    
    /* make 'size' pair */
    key = ylacreate_sym(_alloc_str("size"));
    v = ylacreate_dbl((double)st.st_size);
    return ylappend(r, ylcons(yllist(key, v), ylnil()));

} YLENDNF(fstat)


YLDEFNF(fread, 1, 1) {
    FILE*    fh = NULL;
    char*    buf = NULL;
    yle_t*   r;

    ylnfcheck_atype_chain1(e, YLASymbol);
    
    buf = _readf(NULL, "fread", ylasym(ylcar(e)).sym, TRUE);
    if(!buf) { goto bail; }

    r = ylacreate_sym(buf);
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
    FILE*        fh = NULL;
    char*        buf = NULL;
    yle_t*       r;
    unsigned int sz;

    ylnfcheck_atype_chain1(e, YLABinary);
    
    buf = _readf(&sz, "freadb", ylasym(ylcar(e)).sym, FALSE);
    if(!buf && 0 != sz) { goto bail; }

    r = ylacreate_bin(buf, sz);
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
        ylnflogE0("invalid parameter type\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
       
    fh = fopen(ylasym(ylcar(e)).sym, "w");
    if(!fh) {
        ylnflogW1("Cannot open file [%s]\n", ylasym(ylcar(e)).sym);
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
        ylnflogW1("Fail to write file [%s]\n", ylasym(ylcar(e)).sym);
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

    ylnfcheck_atype_chain1(e, YLASymbol);
    dpath = ylasym(ylcar(e)).sym;
    dip = opendir(dpath);
    if(!dip) { 
        ylnflogW1("Fail to open directory [%s]\n", dpath);
        goto bail; 
    }
 
    { /* just scope */
        struct dirent*    dit;
        yle_t*            sentinel;
        unsigned int      len;
        char*             fname;
        yle_t            *ne, *t;  /* new exp / tail */
        
        /* initialize sentinel */
        sentinel = ylpcreate(ylnil(), ylnil());
        t = sentinel;
        while(dit = readdir(dip)) {
            /* ignore '.', '..' in the directory */
            if(0 == strcmp(".", dit->d_name)
               || 0 == strcmp("..", dit->d_name)) { continue; }

            len = strlen(dit->d_name);
            fname = ylmalloc(len + 1); /* +1 for trailing 0 */
            memcpy(fname, dit->d_name, len);
            fname[len] = 0; /* trailing 0 */
            ne = ylacreate_sym(fname);
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
