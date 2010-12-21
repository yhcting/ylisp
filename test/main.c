#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

#define CONFIG_LOG
#define CONFIG_ASSERT

#include "ylisp.h"
#include "yllist.h"
#include "yldynb.h"
#include "ylut.h"

/*
 * due to 'assert' macro, put "assert.h" at bottom of include list.
 */
#include <assert.h>

#define _LOGLV YLLogW

#define _NR_MAX_COLUMN 1024
#define _MAX_SLEEP_MS  20   /* max sleep in miliseconds */

typedef struct {
    yllist_link_t    lk;
    char*            s;
} _strn_t; /* string node */

typedef struct {
    unsigned char*   b;
    unsigned int     sz;
    int              section;
    char*            fname;
    int              bok;
} _interpthd_arg_t;

static pthread_mutex_t _m = PTHREAD_MUTEX_INITIALIZER;
static unsigned int    __tcnt = 0;

static inline void
_free_thdarg (_interpthd_arg_t* arg) {
    free (arg->b);
    free (arg->fname);
    free (arg);
}

static inline void
_inc_tcnt() {
    pthread_mutex_lock(&_m);
    __tcnt++;
    pthread_mutex_unlock(&_m);
}

static inline void
_dec_tcnt() {
    pthread_mutex_lock(&_m);
    __tcnt--;
    pthread_mutex_unlock(&_m);
}

static inline unsigned int
_tcnt() { return __tcnt; }

/* =================================
 * To parse script
 * ================================= */
#define _TESTRS "testrs" /* test script for single thread */


static inline void
_append_rsn(yllist_link_t* hd, char* fname) {
    _strn_t* n = malloc(sizeof(_strn_t));
    n->s = fname;
    yllist_add_last(hd, &n->lk);
}

static inline int
_is_digit(char c) {
    return '0'<=c && c<='9';
}

static inline int
_is_ws(char c) {
    return ' '== c || '\n' == c || '\t' == c;
}

/*
 * returned string should be freed by caller.
 */
static char*
_trim(char* s) {
    char        *p, *ns, *r;
    unsigned int sz;

    /* printf("trim IN \"%s\"\n", s); */
    p = s;
    while(*p && _is_ws(*p)) { p++; }
    if(!*p) { sz = 0; }
    else {
        ns = p;
        p = s + strlen(s) - 1;
        while(p>=s && _is_ws(*p)) { p--; }
        assert(p>=s && p>=ns);
        sz = p-ns+1;
    }

    r = malloc(sz+1);
    r[sz] = 0;/*trailing 0*/
    memcpy(r, ns, sz);
    /* printf("trim OUT \"%s\"\n", r); */
    return r;
}

/*
 * Separator : /={20,}$/
 */
static inline int
_is_separator(const char* s) {
#define _NR_MIN_SEP  20
    const char*  p = s;
    unsigned int nr = 0;
    while (*p && '=' == *p) { p++; nr++; }
    if (nr >= _NR_MIN_SEP && !*p) { return 1; }
    else { return 0; }
#undef _NR_MIN_SEP
}

static inline int
_is_mt_ok(const char* s) {
    if (0 == strcmp (s, "MT OK")) return 1;
    else if (0 == strcmp (s, "MT NO")) return 0;
    else {
        printf ("Script syntax error. MT OK or MT NO is expected.\n");
        exit (0);
        return 0; /* to make compiler be happy */
    }
}

static inline int
_is_return_ok(const char* s) {
    if (0 == strcmp (s, "OK")) return 1;
    else if (0 == strcmp (s, "FAIL")) return 0;
    else {
        printf ("Script syntax error. OK or FAIL is expected.\n");
        exit (0);
        return 0; /* to make compiler be happy */
    }
}

static int
_fcmp(const void* e0, const void* e1) {
    return strcmp(*((char**)e0), *((char**)e1));
}

/*
 * 0 : EOF
 * 1 : Not EOF
 */
static int
_read_section (FILE* f, int* bmtok, int* bok, yldynb_t* b) {
#define _ST_MT   0 /* state : get MT OK or NO */
#define _ST_RET  1 /* state : get return */
#define _ST_EXP  2 /* state : get exp */

    char    ln[_NR_MAX_COLUMN];
    char*   tln; /* 'trim'ed line */
    int     state = _ST_MT;
    while (1) {
        if (!fgets (ln, _NR_MAX_COLUMN, f)) {
            if (yldynbstr_len (b) > 0)return 1;
            else return 0;
        }
        tln = _trim (ln);
         /*
          * Following two type line is ignored.
          *    - white space line
          *    - line starts with ';' (leading WS is ignored)
          */
        if (!*tln || ';' == *tln) { free (tln); continue; }
        switch (state) {
            case _ST_MT:
                *bmtok = _is_mt_ok(tln);
                state = _ST_RET;
            break;

            case _ST_RET:
                *bok = _is_return_ok(tln);
                state = _ST_EXP;
            break;

            case _ST_EXP:
                if (_is_separator (tln)) { free (tln); goto done; }
                yldynbstr_append (b, "%s", ln);
            break;

            default: assert(0);
        }
        free(tln);
    }
 done:
    return 1;
#undef _ST_EXP
#undef _ST_RET
#undef _ST_MT
}


/* return script file name array (sorted) */
static char**
_testrs_files(unsigned int* nr/*out*/, const char* rsname) {
    yllist_link_t     hd;
    _strn_t          *pos, *n;
    DIR*              dip = NULL;
    struct dirent*    dit;
    char*             p;
    char**            r;
    unsigned int      i;

    yllist_init_link(&hd);

    dip = opendir(".");
    /* '!!' to make compiler be happy. */
    while(!!(dit = readdir(dip))) {
        if(DT_REG == dit->d_type &&
           0 == memcmp(dit->d_name, rsname, strlen(rsname))) {
            p = dit->d_name + strlen(rsname);
            while(*p && _is_digit(*p)) { p++; }
            if(!*p) {
                /* OK! This is script name */
                p = malloc(strlen(dit->d_name)+1);
                strcpy(p, dit->d_name);
                _append_rsn(&hd, p);
            }
        }
    }
    closedir(dip);

    *nr = yllist_size(&hd);
    r = malloc(sizeof(char*) * *nr);
    i = 0;
    yllist_foreach_item_removal_safe (pos, n, &hd, _strn_t, lk) {
        r[i++] = pos->s;
        free (pos);
    }

    /* we need to sort filename - dictionary order */
    qsort(r, *nr, sizeof(char*), _fcmp);

    return r;
}

static void*
_interp_thread(void* arg) {
    int                ret;
    _interpthd_arg_t*  ia = (_interpthd_arg_t*)arg;
    useconds_t usec;

    /*
     * To test MT interpreting at random timing
     */
    usec = (rand() % _MAX_SLEEP_MS) * 1000;
    usleep(usec);

    /* printf("** Interpret[%d] **\n%s\n", bOK, yldynbstr_string(&dyb)); */
    ret = ylinterpret(ia->b, ia->sz);

    if( !((ia->bok && YLOk == ret)
          || (!ia->bok && YLOk != ret)) ) {
        printf("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"
               "****        TEST FAILS             *****\n"
               "* File : %s\n"
               "* %d-th Section\n"
               "* Exp\n"
               "%s\n", ia->fname, ia->section, ia->b);
        exit(0);
    }
    _free_thdarg (ia);
    _dec_tcnt ();
    return NULL;
 }

static void
_start_test_script(const char* fname, FILE* fh, int bmt) {
    int        bok, bmtok;
    yldynb_t   dyb;
    pthread_t  thd;
    int        seccnt = 0; /* section count */
    _interpthd_arg_t*  targ;
    void*      ret;

    printf("\n\n"
           "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"
           "    Runing Test Script : %s\n"
           "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n"
           "\n\n\n", fname);
    yldynbstr_init (&dyb, _NR_MAX_COLUMN*8);
    while (_read_section (fh, &bmtok, &bok, &dyb)) {
        seccnt++;
        targ = malloc(sizeof(_interpthd_arg_t));
        targ->b = malloc(yldynb_sz(&dyb));
        targ->sz = yldynb_sz(&dyb);
        memcpy(targ->b, yldynb_buf(&dyb), yldynb_sz(&dyb));
        targ->section = seccnt;
        targ->bok = bok;
        targ->fname = malloc(strlen(fname)+1);
        strcpy(targ->fname, fname);

        _inc_tcnt();
        if (!bmt) {
            /* ST case */
            pthread_create(&thd, NULL, &_interp_thread, targ);
            pthread_join (thd, &ret);
        } else {
            /* MT case */
            if (bmtok)
                pthread_create(&thd, NULL, &_interp_thread, targ);
            else
                _free_thdarg (targ);
        }
        yldynbstr_reset(&dyb);
    }
    yldynb_clean(&dyb);
}


static void
_start_test(const char* rsname, int bmt) {
    char**       pf;
    unsigned int nr;
    int          i;
    FILE*        fh;
    pf = _testrs_files(&nr, rsname);
    for(i=0; i<nr; i++) {
        fh = fopen(pf[i], "r");
        assert(fh);
        _start_test_script(pf[i], fh, bmt);
        fclose(fh);
    }
}

/* =================================
 * To use ylisp interpreter
 * ================================= */

static unsigned int      _mblk = 0;
static pthread_mutex_t   _msys = PTHREAD_MUTEX_INITIALIZER;
void*
_malloc(unsigned int size) {
    pthread_mutex_lock(&_msys);
    _mblk++;
    pthread_mutex_unlock(&_msys);
    return malloc(size);
}

void
_free(void* p) {
    pthread_mutex_lock(&_msys);
    assert(_mblk > 0);
    _mblk--;
    pthread_mutex_unlock(&_msys);
    free(p);
}

static int
_get_mblk_size() {
    return _mblk;
}

static void
_log(int lv, const char* format, ...) {
    if(lv >= _LOGLV) {
        va_list ap;
        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);
    }
}

static void
_assert(int a) {
    if(!a){ assert(0); }
}

int
main(int argc, char* argv[]) {
    ylsys_t        sys;
    unsigned int   maxwait = 60;

    /* set system parameter */
    sys.print = printf;
    sys.log = _log;
    sys.assert = _assert;
    sys.malloc = _malloc;
    sys.free = _free;
    sys.mpsz = 8*1024;
    sys.gctp = 1;

    ylinit(&sys);

    srand( time(NULL) );

    printf("\n************ Single Thread Test ************\n");
    /* Test for single thread */
    _start_test(_TESTRS, 0);

    printf("\n************ Multi Thread Test *************\n");
    /* Test for multi thread */
    /* _binit_section = 1; *//* HACK */
    _start_test(_TESTRS, 1);

    /* Let's sleep enough to wait all threads are finished! */
    while(_tcnt() && maxwait) {
        sleep(1);
        maxwait--;
    }

    if(!maxwait) {
        printf("Some of interpreting threads are never finished!! DeadLock???\n");
        return 0;
    }

    yldeinit();

    if(_get_mblk_size()) {
        printf("\n=== Leak : count(%d) ===\n", _get_mblk_size());
        assert(0);
    }

    printf("\n\n\n"
           "=======================================\n"
           "!!!!!   Test Success - GOOD JOB   !!!!!\n"
           "=======================================\n"
           "\n\n\n");
    return 0;
}
