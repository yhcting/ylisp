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
#include "ylsfunc.h"
#include "ylut.h"
#include "yldynb.h"
#include "yltrie.h"


/*
 * I hate this pipe issue!!
 * See comments at 'procia-create'
 */
/* #define _SYS_DBG_PIPE 1 */

#ifdef _SYS_DBG_PIPE
#       define _dbgpipe(x) do { x } while (0)
#else /* _SYS_DBG_PIPE */
#       define _dbgpipe(x) do {} while (0)
#endif /* _SYS_DBG_PIPE */

#define _INVALID_FD -1


/* ------------------
 * Common & Basic inline functions
 * ------------------*/
static inline int
_is_integer(double d) {
	return (double)((int)d) == d;
}

static inline int
_killproc(pid_t pid) {
	int status;
	kill((pid_t)pid, SIGKILL);
	/* get exit status not to remain zombie! */
	waitpid(pid, &status, 0);
	return status;
}

static inline char*
_strclone(const char* str) {
	char*        p;
	p = ylmalloc(strlen(str) +1);
	/*
	 * This function is used for small-length string.
	 * So, let's skip handling allocation-failure.
	 */
	if (p)
		strcpy(p, str);
	else
		ylassert(0);
	return p;
}

/* ------------------
 * For fraw-xxx
 * ------------------*/
static int  _aif_fraw_eq(const yle_t* e0, const yle_t* e1) { return e0 == e1; }
static void
_aif_fraw_clean(yle_t* e) {
	int fd = (long)ylacd(e);
	if (_INVALID_FD != fd)
		close(fd);
	ylacd(e) = (void*)_INVALID_FD;
	return;
}
static int
_aif_fraw_to_string(const yle_t* e, char* b, unsigned int sz) {
	int bw = snprintf(b, sz, "<fd:%ld>", (long)ylacd(e));
	if (bw >= sz)
		return -1; /* not enough buffer */
	return bw;
}

static ylatomif_t _aif_fraw = {
	&_aif_fraw_eq,
	NULL, /* copy is not allowed! - duplicated fd is not expected! */
	&_aif_fraw_to_string,
	NULL,
	&_aif_fraw_clean
};


#ifdef HAVE_LIBSTDBUF
/* ------------------
 * For procia-xxx
 * ------------------*/
struct procia_cust {
	pid_t    cpid;
	yle_t*   wp;  /* write pipe - fraw type */
	yle_t*   rp;  /* read pipe  - fraw type */
};

static int  _aif_procia_eq(const yle_t* e0, const yle_t* e1) {
	return e0 == e1;
}

static void
_aif_procia_clean(yle_t* e) {
	struct procia_cust* pc = (struct procia_cust*)ylacd(e);
	if (pc) {
		_aif_fraw_clean(pc->wp);
		_aif_fraw_clean(pc->rp);
		_killproc(pc->cpid);
		ylfree(pc);
		ylacd(e) = NULL;
	}
	return;
}
static int
_aif_procia_to_string(const yle_t* e, char* b, unsigned int sz) {
	struct procia_cust* pc = (struct procia_cust*)ylacd(e);
	int bw = snprintf(b, sz, "<cpid:%d>", pc? pc->cpid: -1);
	if (bw >= sz)
		return -1; /* not enough buffer */
	return bw;
}

static int
_aif_procia_visit(yle_t* e, void* user, int(*cb)(void*, yle_t*)) {
	struct procia_cust* pc = (struct procia_cust*)ylacd(e);
	if (pc) {
		(*cb)(user, pc->wp);
		(*cb)(user, pc->rp);
	}
	return 1;
}

static ylatomif_t _aif_procia = {
	&_aif_procia_eq,
	NULL, /* copy is not allowed! */
	&_aif_procia_to_string,
	&_aif_procia_visit,
	&_aif_procia_clean
};

#endif /* HAVE_LIBSTDBUF */


static inline int
_is_fraw_data_list(yle_t* e) {
	if (yleis_atom(e))
		return 0;
	ylelist_foreach(e)
		if (!(ylais_type(ylcar(e), &_aif_fraw)
		      && (_INVALID_FD != (long)ylacd(ylcar(e)))))
			return 0;
	return 1;
}

/*
 * @outsz:
 *    in case of fail: 0 means OK, otherwise error!
 */
static void*
_readf(unsigned int* outsz, const char* func, const char* fpath, int btext) {
	char*        buf = NULL;
	unsigned int sz;

	if (outsz)
		*outsz = 1; /* 1 means, 'not 0' */
	buf = ylutfile_read(&sz, fpath, btext);

	if (buf) {
                if (outsz)
                        *outsz=sz;
	} else {
		switch(sz) {
		case YLOk:
			if (outsz)
				*outsz = 0;
			break;

		case YLErr_io:
			yllogW("<!%s!> Error: File IO [%s]\n", func, fpath);
			break;

		case YLErr_out_of_memory:
			yllogW("<!%s!> Not enough memory to load file [%s]\n",
			       func,
			       fpath);
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
	_killproc((pid_t)(long)pid);
}

static void _sh_child_process(yle_t* e, FILE* fout) __attribute__ ((noreturn));
static void
_sh_child_process(yle_t* e, FILE* fout) {
#ifdef HAVE_BASH
#       define __shell "/bin/bash"
#else /* HAVE_BASH */
#       define __shell "/bin/sh"
#endif /* HAVE_BASH */

	yllogD(">> sh >> forked >>>\n");
	/* redirect stderr to the pipe */
	if (0 > dup2(fileno(fout), STDOUT_FILENO )
	    || 0 > dup2(fileno(fout), STDERR_FILENO)) {
		perror("Fail to dup stderr or stdout\n");
		exit(0);
	}
	fclose(fout); /* dupped */
	execl(__shell, __shell, "-c", ylasym(ylcar(e)).sym, (char*)0);
	perror("failed to run command\n");
	exit(1);

#undef __shell
}


YLDEFNF(sh, 1, 1) {

/*
 * We would better put them in tmp directory, because
 *  out file is actually temporally used one!
 */
#define __cleanup()                             \
	/* clean resources */			\
		if (buf) ylfree(buf);		\

	char*       buf = NULL;
	FILE*       fout;
	pid_t       cpid; /* child process id */

	/* check input parameter */
	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	/* before forking flush/clean stdout and stderr */
	fflush(stdout); fflush(stderr);

	fout = tmpfile();

	if (!fout) {
		ylnflogE("Fail to get tmpfile for redirection!\n"
			 "  => %s\n", strerror(errno));
		ylinterpret_undefined(YLErr_func_fail);
	}

	cpid = fork();

	if (-1 == cpid) {
		ylnflogE("Fail to fork!\n");
		goto bail;
	}

	if (!cpid)
		_sh_child_process(e, fout);

	/* Parent */

	/*
	 * add process resources to thread context.
	 */
	ylmt_add_pres(cxt, itoptr(cpid), &_ccb_pkill);
	ylmt_add_pres(cxt, fout, &_ccb_fclose);
	/* I'm safe. And may take some time. */
	ylmt_notify_safe(cxt);
	if (-1 == waitpid(cpid, NULL, 0)) {
		if (ECHILD == errno)
			/*
			 * child process already killed!.
			 * (This is possible. This is not error.)
			 * Nothing to do.
			 */
			;
		else {
			ylassert(0);
			ylmt_notify_unsafe(cxt);
			ylnflogE("Fail to wait child process!\n");
			_killproc(cpid);
			goto bail;
		}
	}
	ylmt_notify_unsafe(cxt);
	ylmt_rm_pres(cxt, fout);
	ylmt_rm_pres(cxt, itoptr(cpid));


	buf = ylutfile_fread(NULL, fout, TRUE);
	fclose(fout);

	if (!buf) {
		ylnflogE("Fail to read output result\n");
		goto bail;
	} else
		return ylacreate_sym(buf);

 bail:
	if (buf) ylfree(buf);
	ylinterpret_undefined(YLErr_func_fail);

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
	if (env) {
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
	if (0 == setenv(ylasym(ylcar(e)).sym, ylasym(ylcadr(e)).sym, 1))
		return ylt();
	else
		return ylnil();
} YLENDNF(setenv)

YLDEFNF(getpid, 0, 0) {
	return ylacreate_dbl(getpid());
} YLENDNF(getpid)

YLDEFNF(chdir, 1, 1) {
	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
	if (0 > chdir(ylasym(ylcar(e)).sym)) {
		ylnflogW("Fail to change directory to [ %s ]\n",
			 ylasym(ylcar(e)).sym);
		return ylnil();
	} else
		return ylt();
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
	if (0 > stat(ylasym(ylcar(e)).sym, &st)) {
		ylnflogW("Cannot get status of file [%s]\n",
			 ylasym(ylcar(e)).sym);
		return ylnil();
	}

	/* make 'type' pair */
	key = ylacreate_sym(_strclone("type"));
	switch(st.st_mode & S_IFMT) {
        case S_IFSOCK: v = ylacreate_sym(_strclone("s"));  break;
        case S_IFLNK:  v = ylacreate_sym(_strclone("l"));  break;
        case S_IFREG:  v = ylacreate_sym(_strclone("f"));  break;
        case S_IFBLK:  v = ylacreate_sym(_strclone("b"));  break;
        case S_IFDIR:  v = ylacreate_sym(_strclone("d"));  break;
        case S_IFCHR:  v = ylacreate_sym(_strclone("c"));  break;
        case S_IFIFO:  v = ylacreate_sym(_strclone("p"));  break;
        default: v = ylacreate_sym(_strclone("u")); /* unknown */
	}
	/* make r as '((key v)) */
	r = ylcons(yllist(key, v), ylnil());

	/* make 'size' pair */
	key = ylacreate_sym(_strclone("size"));
	v = ylacreate_dbl((double)st.st_size);
	return ylappend(r, ylcons(yllist(key, v), ylnil()));

} YLENDNF(fstat)


YLDEFNF(fread, 1, 1) {
	  char*    buf = NULL;
	  yle_t*   r;

	  ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	  buf = _readf(NULL, "fread", ylasym(ylcar(e)).sym, TRUE);
	  if (!buf)
		  goto bail;

	  r = ylacreate_sym(buf);
	  buf = NULL; /* to prevent from free */

	  if (buf)
		  ylfree(buf);
	  return r;

 bail:
	  if (buf)
		  ylfree(buf);

	  return ylnil();
} YLENDNF(fread)

YLDEFNF(freadb, 1, 1) {
	unsigned char*   buf = NULL;
	yle_t*           r;
	unsigned int     sz;

	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	buf = _readf(&sz, "freadb", ylasym(ylcar(e)).sym, FALSE);
	if (!buf && 0 != sz)
		goto bail;

	r = ylacreate_bin(buf, sz);
	buf = NULL; /* to prevent from free */

	if (buf)
		ylfree(buf);

	return r;

 bail:
	if (buf)
		ylfree(buf);

	return ylnil();
} YLENDNF(freadb)

YLDEFNF(fwrite, 2, 2) {
	FILE*    fh = NULL;
	yle_t*   dat = ylcadr(e);
	void*    rawdata;
	unsigned int sz;

	/* parameter check */
	ylnfcheck_parameter(ylais_type(ylcar(e), ylaif_sym())
			    && (ylais_type(dat, ylaif_sym())
				|| ylais_type(dat, ylaif_bin())));

	fh = fopen(ylasym(ylcar(e)).sym, "w");
	if (!fh) {
		ylnflogW("Cannot open file [%s]\n", ylasym(ylcar(e)).sym);
		goto bail;
	}

	if ( ylaif_sym() == ylaif(dat) ) {
		sz = strlen(ylasym(dat).sym);
		rawdata = ylasym(dat).sym;
	} else { /* Binary case */
		sz = ylabin(dat).sz;
		rawdata = ylabin(dat).d;
	}

	if (sz != fwrite(rawdata, 1, sz, fh)) {
		ylnflogW("Fail to write file [%s]\n", ylasym(ylcar(e)).sym);
		goto bail;
	}

	if (fh)
		fclose(fh);
	return ylt();

 bail:
	if (fh)
		fclose(fh);
	return ylnil();
} YLENDNF(fwrite)


YLDEFNF(readdir, 1, 1) {
	DIR*              dip = NULL;
	yle_t*            re;      /* expression to return */
	const char*       dpath;

	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
	dpath = ylasym(ylcar(e)).sym;
	dip = opendir(dpath);
	if (!dip) {
		ylnflogW("Fail to open directory [%s]\n", dpath);
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
		while (!!(dit = readdir(dip))) {
			/* ignore '.', '..' in the directory */
			if (0 == strcmp(".", dit->d_name)
			    || 0 == strcmp("..", dit->d_name))
				continue;

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
	if (dip)
		closedir(dip);
	return ylnil();
} YLENDNF(readdir)

static inline int
_is_valid_fd(int fd) {
	return fcntl(fd, F_GETFL) != -1 || errno != EBADF;
}

static inline int
_is_in_string(const char* p, char c) {
	while (*p)
		if (*p++ == c)
			return 1;
	return 0;
}

#ifdef HAVE_LIBSTDBUF

static int
_procia_create_prepare_libstdbuf(void) {
	/* FIXME! : don't hardcode */
#define __MAX_PATH_LEN         1024
#define __MAX_LD_PRELOAD_LEN   4096

#define __DIR_PATH             "/usr/lib/coreutils/"
#define __LIB_NAME             "libstdbuf.so"

	/* prepare to use libstdbuf */
	int         ret;
	char        libstdbuf[__MAX_PATH_LEN];
	char        ld_preload[__MAX_LD_PRELOAD_LEN];
	const char* dirpath = __DIR_PATH;
	const char* libname = __LIB_NAME;
	const char* oldlibs;

	ret = snprintf(libstdbuf, __MAX_PATH_LEN, "%s%s", dirpath, libname);
	if (ret < 0 || __MAX_PATH_LEN <= ret) {
		perror("Too long path...\n");
		exit(0);
	}

	oldlibs = getenv("LD_PRELOAD");
	if (oldlibs)
		ret = snprintf(ld_preload,
			       __MAX_LD_PRELOAD_LEN,
			       "LD_PRELOAD=%s:%s",
			       oldlibs,
			       libstdbuf);
	else
		ret = snprintf(ld_preload,
			       __MAX_LD_PRELOAD_LEN,
			       "LD_PRELOAD=%s",
			       libstdbuf);

	ret = putenv(ld_preload);
	if (ret) {
		perror("Fail to update LD_PRELOAD environment!\n");
		return -1;
	}

	/* set redirected standard out to line-buffered mode by convention */
	ret = putenv("_STDBUF_O=L");
	/* set redirected standard error to unbuffered mode by convention */
	ret = putenv("_STDBUF_E=0");
	if (ret) {
		perror("Fail to update LD_PRELOAD environment!\n");
		return -1;
	}

	return 0;

#undef __DIR_PATH
#undef __LIB_NAME
#undef __MAX_LD_PRELOAD_LEN
#undef __MAX_PATH_LEN

}

static void
_procia_create_child_process(yle_t* e,
			     int wp[],
			     int rp[],
			     int pcsz)
	__attribute__ ((noreturn));

static void
_procia_create_child_process(yle_t* e,
			     int wp[],
			     int rp[],
			     int pcsz) {
	_dbgpipe(yllogD(">> proc >> forked >>>\n"););
	_dbgpipe(
		 if (!_is_valid_fd(STDIN_FILENO))
			 fprintf(stderr,
				 "!!! INVALID STDIN [-1] !!!\n");
		 );

	if (0 > _procia_create_prepare_libstdbuf())
		exit(1);

	/* redirect stdin to the pipe */
	if (0 > dup2(wp[0], STDIN_FILENO)) {
		perror("Fail to dup stdin\n");
		exit(0);
	}

	_dbgpipe(
		 if (!_is_valid_fd(STDIN_FILENO))
			 fprintf(stderr,
				 "!!! INVALID STDIN [0] !!!\n");
		 );

	/* redirect stderr/stdout to the pipe */
#ifdef _SYS_DBG_PIPE
	if (0 > dup2(rp[1], STDOUT_FILENO )) {
		perror("Fail to dup stderr or stdout\n");
		exit(0);
	}
#else /* _SYS_DBG_PIPE */
	if (0 > dup2(rp[1], STDOUT_FILENO )
	    || 0 > dup2(rp[1], STDERR_FILENO)) {
		perror("Fail to dup stderr or stdout\n");
		exit(0);
	}
#endif /* _SYS_DBG_PIPE */

	close(wp[1]);
	close(rp[0]);
	/* close dupped fds */

	/*
	 * !! FIXME !!
	 * temporal-walk-around for pipe issue.
	 * (commenting out codes that close dupped file descriptor)
	 *
	 * ********************* PIPE ISSUE **********************
	 * STDIN_FILENO is valid until now.
	 * And file descriptor wp[0] and rp[1] is dupped
	 *   to STDIN and STDOUT.
	 * But, STDIN_FILENO becomes invalid descripter at below check
	 *  - "INVALID STDIN [1]"
	 * I have no idea why this happends.
	 * But, commenting out below 'close' codes seems to be successful
	 *   walk-around for this issue.
	 * Very interesting!
	 * But this is not, definitely, right solution.
	 * wp[0] and rp[1] are never closed!
	 * Any idea????
	 */
	/*
	  close(wp[0]);
	  close(rp[1]);
	*/

	_dbgpipe(
		 if (!_is_valid_fd(STDIN_FILENO))
			 fprintf(stderr, "!!! INVALID STDIN [1] !!!\n");
		 );

	{ /* Just scope */
		int    i;
		char** argv = ylmalloc(sizeof(char*)*(pcsz+1));
		ylassert(argv);
		for (i=0; i<pcsz; i++) {
			argv[i] = ylasym(ylcar(e)).sym;
			e = ylcdr(e);
		}
		argv[pcsz] = (char*)0;

		_dbgpipe(
			 if (!_is_valid_fd(STDIN_FILENO))
				 fprintf(stderr,
					 "!!! INVALID STDIN [2] !!!\n");
			 );
		execvp(argv[0], &argv[1]);

		perror("failed to run command\n");
		exit(1);
	}
}

YLDEFNF(procia_create, 1, 9999) {
	struct procia_cust* pc = NULL;
	pid_t     cpid; /* child process id */
	int       wp[] = {_INVALID_FD, _INVALID_FD};
	int       rp[] = {_INVALID_FD, _INVALID_FD};

	/* check input parameter */
	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));

	/* before forking flush/clean stdout and stderr */
	fflush(stdout); fflush(stderr);

	if (0 > pipe(wp))
		goto bail;
	if (0 > pipe(rp))
		goto bail;

	cpid = fork();

	if (-1 == cpid) {
		ylnflogE("Fail to fork!\n");
		goto bail;
	}

	if (!cpid)
		_procia_create_child_process(e, wp, rp, pcsz);

	/* parent */
	close(wp[0]);
	close(rp[1]);
	pc = (struct procia_cust*)ylmalloc(sizeof(*pc));
	pc->cpid = cpid;
	pc->wp = ylacreate_cust(&_aif_fraw, itoptr(wp[1]));
	pc->rp = ylacreate_cust(&_aif_fraw, itoptr(rp[0]));

	return ylacreate_cust(&_aif_procia, pc);

 bail:
	if (_INVALID_FD != wp[0])
		close(wp[0]);
	if (_INVALID_FD != wp[1])
		close(wp[1]);
	if (_INVALID_FD != rp[0])
		close(rp[0]);
	if (_INVALID_FD != rp[1])
		close(rp[1]);
	if (pc)
		ylfree(pc);
	ylinterpret_undefined(YLErr_func_fail);

} YLENDNF(procia_create)


YLDEFNF(procia_destroy, 1, 1) {
	ylnfcheck_parameter(ylais_type_chain(e, &_aif_procia)
			    && ylacd(ylcar(e)) );
	_aif_procia_clean(ylcar(e));
	return ylt();
} YLENDNF(procia_destroy)

YLDEFNF(procia_wfd, 1, 1) {
	ylnfcheck_parameter(ylais_type_chain(e, &_aif_procia)
			    && ylacd(ylcar(e)) );
	return ((struct procia_cust*)ylacd(ylcar(e)))->wp;
} YLENDNF(procia_wfd)

YLDEFNF(procia_rfd, 1, 1) {
	ylnfcheck_parameter(ylais_type_chain(e, &_aif_procia)
			    && ylacd(ylcar(e)) );
	return ((struct procia_cust*)ylacd(ylcar(e)))->rp;
} YLENDNF(procia_rfd)

#endif /* HAVE_LIBSTDBUF */

YLDEFNF(fraw_open, 2, 2) {
	int fd;
	int flags = 0;
	const char* p;
	/* check input parameter */
	ylnfcheck_parameter(ylais_type_chain(e, ylaif_sym()));
	/* flag string */
	p = ylasym(ylcadr(e)).sym;
	if (_is_in_string(p, 'r'))
		flags = O_RDONLY;
	if (_is_in_string(p, 'w'))
		flags =(flags & O_RDONLY)? O_RDWR: O_WRONLY;

	if (_is_in_string(p, 'n'))
		flags |= O_NONBLOCK;

	ylmt_notify_safe(cxt);
	fd = open( ylasym(ylcar(e)).sym, flags);
	ylmt_notify_unsafe(cxt);

	if (0 > fd) {
		ylnflogW("Fail to open file : %s\n", strerror(errno));
		return ylnil();
	}

	return ylacreate_cust(&_aif_fraw, itoptr(fd));
} YLENDNF(fraw_open)

YLDEFNF(fraw_close, 1, 1) {
	int fd, ret;
	/* check input parameter */
	ylnfcheck_parameter(ylais_type_chain(e, &_aif_fraw)
			    &&(_INVALID_FD != (long)ylacd(ylcar(e))) );
	fd = (long)ylacd(ylcar(e));
	if (!_is_valid_fd(fd))
		ylnfinterp_fail(YLErr_func_invalid_param,
				"This is NOT valid file descripter : %d\n",
				fd);
	ret = close(fd);
	if (0 > ret)
		return ylnil();
	else {
		ylacd(ylcar(e)) = (void*)_INVALID_FD;
		return ylt();
	}
} YLENDNF(fraw_close)

YLDEFNF(fraw_sysfd, 1, 1) {
	ylnfcheck_parameter(ylais_type_chain(e, &_aif_fraw)
			    && (_INVALID_FD != (long)ylacd(ylcar(e))) );
	return ylacreate_dbl((long)ylacd(ylcar(e)));
} YLENDNF(fraw_sysfd)

YLDEFNF(fraw_is_valid, 1, 1) {
	ylnfcheck_parameter(ylais_type_chain(e, &_aif_fraw)
			    && (_INVALID_FD != (long)ylacd(ylcar(e))) );
	return _is_valid_fd((long)ylacd(ylcar(e)))? ylt(): ylnil();
} YLENDNF(fraw_sysfd)


YLDEFNF(fraw_write, 2, 2) {
	int       fd, bw;
	yle_t*    d = ylcadr(e);
	/* check input parameter */
	ylnfcheck_parameter(ylais_type(ylcar(e), &_aif_fraw)
			    && (_INVALID_FD != (long)ylacd(ylcar(e)))
			    && ylais_type(ylcadr(e), ylaif_bin()));
	fd = (long)ylacd(ylcar(e));
	if (!_is_valid_fd(fd))
		ylnfinterp_fail(YLErr_func_invalid_param,
				"This is NOT valid file descripter : %d\n",
				fd);

	ylmt_notify_safe(cxt);
	bw = write(fd, ylabin(d).d, ylabin(d).sz);
	fsync(fd); /* Is it really required? */
	ylmt_notify_unsafe(cxt);

	if (ylabin(d).sz != bw)
		ylnfinterp_fail(YLErr_func_fail,
				"Fail to write file. : fd : %d\n",
				fd);
	return ylt();
} YLENDNF(fraw_write)

static void
_ccb_dynb(void* b) {
	yldynb_clean(b);
	ylfree(b);
}

YLDEFNF(fraw_read, 1, 1) {
	yldynb_t*  dynb;
	int        fd, br;
	/* check input parameter */
	ylnfcheck_parameter(ylais_type_chain(e, &_aif_fraw)
			    && (_INVALID_FD != (long)ylacd(ylcar(e))) );

	fd = (long)ylacd(ylcar(e));

	if (!_is_valid_fd(fd))
		ylnfinterp_fail(YLErr_func_invalid_param,
				"This is NOT valid file descripter : %d\n",
				fd);

	dynb = ylmalloc(sizeof(yldynb_t)); ylassert(dynb);
	if (0 > yldynb_init(dynb, 4096))
		ylassert(0);

	ylmt_add_pres(cxt, dynb, &_ccb_dynb);
	ylmt_notify_safe(cxt);
	while (1) {
		br = read(fd, yldynb_ptr(dynb), yldynb_freesz(dynb));
		if (br < 0) {
			if (EAGAIN == errno) {
				yldynb_reset(dynb);
				break;
			} else
				ylnfinterp_fail(YLErr_func_fail,
						"Fail to read\n");
		} else if (br < yldynb_freesz(dynb)) {
			dynb->sz += br;
			break;
		} else {
			dynb->sz += br;
			if (0 > yldynb_expand(dynb) )
				ylnfinterp_fail(
					YLErr_out_of_memory,
					"Out of memory : sz requested : %d\n",
					yldynb_limit(dynb));
		}
	}
	ylmt_notify_unsafe(cxt);
	ylmt_rm_pres(cxt, dynb);

	if (yldynb_sz(dynb) > 0) {
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
} YLENDNF(fraw_read)

YLDEFNF(fraw_select, 4, 4) {
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
			    (yleis_nil(rl)
			     || (!yleis_atom(rl) && _is_fraw_data_list(rl)))
			    && (yleis_nil(wl)
				|| (!yleis_atom(wl) && _is_fraw_data_list(wl)))
			    && (yleis_nil(el)
				|| (!yleis_atom(el) && _is_fraw_data_list(el)))
			    && ylais_type(to, ylaif_dbl())
			    && _is_integer(yladbl(to))
			    );

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);

	{ /* Just scope */
		int i = (int)yladbl(to);
		/* set time out */
		tv.tv_sec = i/1000;
		i %= 1000;
		tv.tv_usec = i*1000;
	}

#define __FDSET(e, fdset)                                               \
	do {								\
		yle_t* te = e;						\
		int    tfd;						\
		ylelist_foreach(te) {					\
			tfd = (long)ylacd(ylcar(te));			\
			if (nfds < tfd)					\
				nfds = tfd;				\
			if (!_is_valid_fd(tfd))				\
				ylnfinterp_fail(			\
					YLErr_func_fail,		\
					"Invalid file descriptor : %d\n", \
					tfd);				\
			FD_SET(tfd, fdset);				\
		}							\
	} while (0)

	__FDSET(rl, &rfds);
	__FDSET(wl, &wfds);
	__FDSET(el, &efds);

#undef __FDSET

	ylmt_notify_safe(cxt);
	retval = select(nfds+1, &rfds, &wfds, &efds, &tv);
	ylmt_notify_unsafe(cxt);

	if (0 > retval)
		ylnfinterp_fail(YLErr_func_fail, "Fail to select\n");
	/* check timeout */
	if (0 == retval)
		return ylnil();

#define __FDCHECK(R, e, fdset)					\
	do {							\
		yle_t* te = e;					\
		int    tfd;					\
		yle_t* l = ylnil();   /* list */		\
		ylelist_foreach(te) {				\
			tfd = (long)ylacd(ylcar(te));		\
			if (FD_ISSET(tfd, fdset))		\
				l = ylcons(ylcar(te), l);	\
		}						\
		R = ylcons(l, R);				\
	} while (0)

	r = ylnil();
	/* inverse order of __FDSET */
	__FDCHECK(r, el, &efds);
	__FDCHECK(r, wl, &wfds);
	__FDCHECK(r, rl, &rfds);

#undef __FDCHECK

	return r;
} YLENDNF(fraw_select)
