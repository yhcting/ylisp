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
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <memory.h>
#include <pthread.h>

/* enable logging & debugging */
#define CONFIG_ASSERT
#define CONFIG_LOG

#include "ylsfunc.h"
#include "ylut.h"
#include "yldynb.h"

static inline int
_is_integer(double d) {
    return (double)((int)d) == d;
}

static inline int
_is_integer_list(yle_t* e) {
    if(yleis_atom(e)) { return 0; }
    ylelist_foreach(e) {
        if(! (ylais_type(ylcar(e), ylaif_dbl())
              && _is_integer(yladbl(ylcar(e)))) ) {
            return 0;
        }
    }
    return 1;
}

static inline char*
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

static void
_ccb_fclose(void* f) {
    fclose(f);
}

static void
_ccb_pkill(void* pid) {
    kill((pid_t)pid, SIGKILL);
}

YLDEFNF(sh, 1, 1) {
/*
 * We would better put them in tmp directory, because
 *  out file is actually temporally used one!
 */
#define __cleanup()                                     \
    /* clean resources */                               \
    if(buf) { ylfree(buf); }                            \

    char*       buf = NULL;
    FILE*       fout;

    /* check input parameter */
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

    /* before forking flush/clean stdout and stderr */
    fflush(stdout); fflush(stderr);

    fout = tmpfile();

    { /* Just scope */
        pid_t       cpid; /* child process id */

        cpid = fork();

        if(-1 == cpid) {
            ylnflogE0("Fail to fork!\n");
            goto bail;
        }

        if(cpid) { /* parent */
            /*
             * add process resources to thread context.
             */
            ylmt_add_pres(cxt, (void*)cpid, &_ccb_pkill);
            ylmt_add_pres(cxt, fout, &_ccb_fclose);
            /* I'm safe. And may take some time. */
            ylmt_notify_safe(cxt);
            if(-1 == waitpid(cpid, NULL, 0)) {
                if(ECHILD == errno) {
                    /*
                     * child process already killed!.
                     * (This is possible. This is not error.)
                     * Nothing to do.
                     */
                    ;
                } else {
                    ylassert(0);
                    ylmt_notify_unsafe(cxt);
                    ylnflogE0("Fail to wait child process!\n");
                    kill(cpid, SIGKILL);
                    /*ylprocinfo_del(cpid);*/
                    goto bail;
                }
            }
            ylmt_notify_unsafe(cxt);
            ylmt_rm_pres(cxt, fout);
            ylmt_rm_pres(cxt, (void*)cpid);
        } else { /* child */
            /* redirect stderr to the pipe */
            if(0 > dup2(fileno(fout), STDOUT_FILENO )
               || 0 > dup2(fileno(fout), STDERR_FILENO)) {
                perror("Fail to dup stderr or stdout\n");
                exit(0);
            }

            execl("/bin/bash", "/bin/bash", "-c", ylasym(ylcar(e)).sym, (char*)0);
        }
    } /* Just scope */

    buf = ylutfile_fread(NULL, fout, TRUE);
    fclose(fout);

    if(!buf) {
        ylnflogE0("Fail to read output result\n");
        goto bail;
    } else {
        return ylacreate_sym(buf);
    }

 bail:
    if(buf) { ylfree(buf); }
    ylinterpret_undefined(YLErr_func_fail);
    return NULL; /* to make compiler be happy. */

#undef __cleanup
#undef __outf_name

} YLENDNF(sh)

YLDEFNF(sleep, 1, 1) {
    /* check input parameter */
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    ylmt_notify_safe(cxt);
    sleep((unsigned int)yladbl(ylcar(e)));
    ylmt_notify_unsafe(cxt);
    return ylt();
} YLENDNF(sleep)

YLDEFNF(usleep, 1, 1) {
    /* check input parameter */
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    ylmt_notify_safe(cxt);
    usleep((unsigned int)yladbl(ylcar(e)));
    ylmt_notify_unsafe(cxt);
    return ylt();
} YLENDNF(usleep)

YLDEFNF(getenv, 1, 1) {
    char*  env;
    /* check input parameter */
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
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
    /* check input parameter */
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
    if(0 == setenv(ylasym(ylcar(e)).sym, ylasym(ylcadr(e)).sym, 1)) {
        return ylt();
    } else {
        return ylnil();
    }
} YLENDNF(setenv)

YLDEFNF(chdir, 1, 1) {
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
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
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
    if(0 > stat(ylasym(ylcar(e)).sym, &st)) {
        ylnflogW1("Cannot get status of file [%s]\n", ylasym(ylcar(e)).sym);
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
    char*    buf = NULL;
    yle_t*   r;

    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

    buf = _readf(NULL, "fread", ylasym(ylcar(e)).sym, TRUE);
    if(!buf) { goto bail; }

    r = ylacreate_sym(buf);
    buf = NULL; /* to prevent from free */

    if(buf) { ylfree(buf); }
    return r;

 bail:
    if(buf) { ylfree(buf); }

    return ylnil();
} YLENDNF(fread)

YLDEFNF(freadb, 1, 1) {
    unsigned char*   buf = NULL;
    yle_t*           r;
    unsigned int     sz;

    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

    buf = _readf(&sz, "freadb", ylasym(ylcar(e)).sym, FALSE);
    if(!buf && 0 != sz) { goto bail; }

    r = ylacreate_bin(buf, sz);
    buf = NULL; /* to prevent from free */

    if(buf) { ylfree(buf); }

    return r;

 bail:
    if(buf) { ylfree(buf); }

    return ylnil();
} YLENDNF(freadb)

YLDEFNF(fwrite, 2, 2) {
    FILE*    fh = NULL;
    yle_t*   dat = ylcadr(e);
    void*    rawdata;
    unsigned int sz;

    /* parameter check */
    ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym())
                        && (ylais_type(dat, ylaif_sym()) || ylais_type(dat, ylaif_bin())));

    fh = fopen(ylasym(ylcar(e)).sym, "w");
    if(!fh) {
        ylnflogW1("Cannot open file [%s]\n", ylasym(ylcar(e)).sym);
        goto bail;
    }

    if( ylaif_sym() == ylaif(dat) ) {
        sz = strlen(ylasym(dat).sym);
        rawdata = ylasym(dat).sym;
    } else { /* Binary case */
        sz = ylabin(dat).sz;
        rawdata = ylabin(dat).d;
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

    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
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
        sentinel = ylcons(ylnil(), ylnil());
        t = sentinel;
        /* '!!' to make compiler be happy */
        while(!!(dit = readdir(dip))) {
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

static inline int
_is_valid_fd(int fd) {
    return fcntl(fd, F_GETFL) != -1 || errno != EBADF;
}

static inline int
_is_in_string(const char* p, char c) {
    while(*p) {
        if(*p++ == c) { return 1; }
    }
    return 0;
}

YLDEFNF(fdopen, 2, 2) {
    int fd;
    int flags = 0;
    const char* p;
    /* check input parameter */
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
    /* flag string */
    p = ylasym(ylcadr(e)).sym;
    if(_is_in_string(p, 'r')) { flags = O_RDONLY; }
    if(_is_in_string(p, 'w')) {
        flags = (flags & O_RDONLY)? O_RDWR: O_WRONLY;
    }
    if(_is_in_string(p, 'n')) {
        flags |= O_NONBLOCK;
    }

    ylmt_notify_safe(cxt);
    fd = open( ylasym(ylcar(e)).sym, flags);
    ylmt_notify_unsafe(cxt);

    if(0 > fd) {
        ylnflogW1("Fail to open file : %s\n", strerror(errno));
        return ylnil();
    }

    if( (int)((double)fd) != fd ) {
        ylnflogW1("Cannot represent fd as double!. fd : %d\n", fd);
        return ylnil();
    }

    return ylacreate_dbl((double)fd);
} YLENDNF(fdopen)

YLDEFNF(fdclose, 1, 1) {
    double    dfd;
    /* check input parameter */
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));
    dfd = yladbl(ylcar(e));
    if(!_is_integer(dfd)
       || !_is_valid_fd((int)dfd)) {
        ylnflogE1("This is NOT valid file descripter : %f\n", dfd);
        ylinterpret_undefined(YLErr_func_invalid_param);
    }
    return (0 > close((int)dfd))? ylnil(): ylt();
} YLENDNF(fdclose)

YLDEFNF(fdwrite, 2, 2) {
    double    dfd;
    int       bw;
    yle_t*    d = ylcadr(e);
    /* check input parameter */
    ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_dbl())
                        && ylais_type(ylcadr(e), ylaif_bin()));
    dfd = yladbl(ylcar(e));
    if(!_is_integer(dfd)) {
        ylnflogE0("Invalid Parameter\n");
        ylinterpret_undefined(YLErr_func_invalid_param);
    }

    ylmt_notify_safe(cxt);
    bw = write((int)dfd, ylabin(d).d, ylabin(d).sz);
    ylmt_notify_unsafe(cxt);

    if(ylabin(d).sz != bw) {
        ylnflogW1("Fail to write file. : fd : %d\n", (int)dfd);
        ylinterpret_undefined(YLErr_func_fail);
    }
    return ylt();
} YLENDNF(fdwrite)

static void
_ccb_dynb(void* b) {
    yldynb_clean(b);
    ylfree(b);
}

YLDEFNF(fdread, 1, 1) {
    double     dfd;
    yldynb_t*  dynb;
    int        br;
    /* check input parameter */
    ylnfcheck_parameter(ylais_type_chain(e, ylaif_dbl()));

    dfd = yladbl(ylcar(e));
    if(!_is_integer(dfd)){
        ylnflogE0("Invalid Parameter\n");
        ylinterpret_undefined(YLErr_func_fail);
    }

    dynb = ylmalloc(sizeof(yldynb_t)); ylassert(dynb);
    if(0 > yldynb_init(dynb, 4096)) { ylassert(0); }

    ylmt_add_pres(cxt, dynb, &_ccb_dynb);
    ylmt_notify_safe(cxt);
    while(1) {
        br = read((int)dfd, yldynb_ptr(dynb), yldynb_freesz(dynb));
        if(br < 0) {
            if(EAGAIN == errno) { yldynb_reset(dynb); break; }
            else {
                ylnflogE0("Fail to read\n");
                ylinterpret_undefined(YLErr_func_fail);
            }
        } else if(br < yldynb_freesz(dynb)) {
            dynb->sz += br;
            break;
        } else {
            dynb->sz += br;
            if(0 > yldynb_expand(dynb) ) {
                ylnflogE1("Out of memory : sz requested : %d\n", yldynb_limit(dynb));
                ylinterpret_undefined(YLErr_out_of_memory);
            }
        }
    }
    ylmt_notify_unsafe(cxt);
    ylmt_rm_pres(cxt, dynb);

    if(yldynb_sz(dynb) > 0) {
        unsigned char* b = dynb->b;
        unsigned int   sz = dynb->sz;
        /*
         * We don't need to clean 'dynb'.
         * Memory allocated is referend by newly create binary atom.
         * So, responsibility to handle this memory is not lost.
         */
        ylfree(dynb);
        return ylacreate_bin(b, sz);
    } else {
        yldynb_clean(dynb);
        ylfree(dynb);
        return ylnil();
    }
} YLENDNF(fdread)

YLDEFNF(fdselect, 4, 4) {
    fd_set         rfds, wfds, efds;
    int            retval, nfds;
    yle_t         *rl, *wl, *el, *to, *r;
    struct timeval tv;

    rl = ylcar(e);
    wl = ylcadr(e);
    el = ylcaddr(e);
    to = ylcadddr(e);
    nfds = 0;

    ylnfcheck_parameter(
        (yleis_nil(rl) || (!yleis_atom(rl) && _is_integer_list(rl)))
        && (yleis_nil(wl) || (!yleis_atom(wl) && _is_integer_list(wl)))
        && (yleis_nil(el) || (!yleis_atom(el) && _is_integer_list(el)))
        && ylais_type(to, ylaif_dbl()) && _is_integer(yladbl(to))
    );

    FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);

    { /* Just scope */
        int i = (int)yladbl(to);
        /* set time out */
        tv.tv_sec = i/1000;
        i %= 1000;
        tv.tv_usec = i*1000;
    }

#define __FDSET(e, fdset)                                               \
    do {                                                                \
        yle_t* te = e;                                                  \
        int    tfd;                                                     \
        ylelist_foreach(te) {                                           \
            tfd = (int)yladbl(ylcar(te));                               \
            if(nfds < tfd) { nfds = tfd; }                              \
            if(!_is_valid_fd(tfd)) {                                    \
                ylnflogE1("Invalid file descriptor : %d\n", tfd);       \
                ylinterpret_undefined(YLErr_func_fail);                 \
            }                                                           \
            FD_SET(tfd, fdset);                                         \
        }                                                               \
    } while(0)

    __FDSET(rl, &rfds);
    __FDSET(wl, &wfds);
    __FDSET(el, &efds);

#undef __FDSET

    ylmt_notify_safe(cxt);
    retval = select(nfds+1, &rfds, &wfds, &efds, &tv);
    ylmt_notify_unsafe(cxt);

    if (0 > retval) {
        ylnflogE0("Fail to select\n");
        ylinterpret_undefined(YLErr_func_fail);
    }
    /* check timeout */
    if (0 == retval) { return ylnil(); }

#define __FDCHECK(R, e, fdset)                          \
    do {                                                \
        yle_t* te = e;                                  \
        int    tfd;                                     \
        yle_t* l = ylnil();   /* list */                \
        ylelist_foreach(te) {                           \
            tfd = (int)yladbl(ylcar(te));               \
            if(FD_ISSET(tfd, fdset)) {                  \
                l = ylcons(ylacreate_dbl(tfd), l);      \
            }                                           \
        }                                               \
        R = ylcons(l, R);                               \
    } while(0)

    r = ylnil();
    /* inverse order of __FDSET */
    __FDCHECK(r, el, &efds);
    __FDCHECK(r, wl, &wfds);
    __FDCHECK(r, rl, &rfds);

#undef __FDCHECK

    return r;
} YLENDNF(fdselect)
