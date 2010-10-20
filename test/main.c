#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <assert.h>
#include <stdarg.h>

#include "ylisp.h"
#include "yllist.h"
#include "ylut.h"

#define _LOGLV YLLogW

#define _NR_MAX_COLUMN 4096

typedef struct {
    yllist_link_t    lk;
    char*            f;
} _rsn_t; /* script file node */

/* =================================
 * To parse script
 * ================================= */
#define _RSPREF "testrs"

static inline void
_append_rsn(yllist_link_t* hd, char* fname) {
    _rsn_t* n = malloc(sizeof(_rsn_t));
    n->f = fname;
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
    while(*p && '=' == *p) { p++; nr++; }
    if(nr >= _NR_MIN_SEP && !*p) { return 1; }
    else { return 0; }
#undef _NR_MIN_SEP
}

static inline int
_is_return_ok(const char* s) {
    if(0 == strcmp(s, "OK")) { return 1; }
    else if(0 == strcmp(s, "FAIL")) { return 0; }
    else {
        printf("Script syntax error. OK or FAIL is expected.\n");
        exit(0);
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
_read_section(FILE* f, int* bOK, yldynb_t* b) {
#define _ST_RET  0 /* state : get return */
#define _ST_EXP  1 /* state : get exp */

    char    ln[_NR_MAX_COLUMN];
    char*   tln; /* 'trim'ed line */
    int     state = _ST_RET;
    while(1) {
        if(!fgets(ln, _NR_MAX_COLUMN, f)) {
            if(ylutstr_len(b) > 0) { return 1; }
            else { return 0; }
        }
        tln = _trim(ln);
         /* 
          * Following two type line is ignored.
          *    - white space line
          *    - line starts with ';' (leading WS is ignored)
          */
        if(!*tln || ';' == *tln) { free(tln); continue; }
        if(_ST_RET == state) {
            *bOK = _is_return_ok(tln);
            state = _ST_EXP;
            /* printf("+++ bOK : %d\n", *bOK); */
        } else {
            if(_is_separator(tln)) { free(tln); break; }
            ylutstr_append(b, "%s", ln);
        }
        free(tln);
    }

    return 1;
#undef _ST_EXP
#undef _ST_RET
}


/* return script file name array (sorted) */
static char**
_testrs_files(unsigned int* nr/*out*/) {
    yllist_link_t     hd;
    _rsn_t           *pos, *n;
    DIR*              dip = NULL;
    struct dirent*    dit;
    char*             p;
    char**            r;
    unsigned int      i;

    yllist_init_link(&hd);
    
    dip = opendir(".");
    while(dit = readdir(dip)) {
        if(DT_REG == dit->d_type &&
           0 == memcmp(dit->d_name, _RSPREF, sizeof(_RSPREF)-1)) {
            p = dit->d_name + sizeof(_RSPREF) - 1;
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
    yllist_foreach_item_removal_safe(pos, n, &hd, _rsn_t, lk) {
        r[i++] = pos->f;
        free(pos);
    }

    /* we need to sort filename - dictionary order */
    qsort(r, *nr, sizeof(char*), _fcmp);

    return r;
}

static void
_start_test_script(const char* fname, FILE* fh) {
    int        bOK;
    int        interp_ret;
    yldynb_t   dyb;
    int        seccnt = 0; /* section count */

    printf("\n\n"
           "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"
           "    Runing Test Script : %s\n"
           "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n"
           "\n\n\n", fname);
    ylutstr_init(&dyb, _NR_MAX_COLUMN*8);
    while(_read_section(fh, &bOK, &dyb)) {
        seccnt++;
        /* printf("** Interpret[%d] **\n%s\n", bOK, ylutstr_string(&dyb)); */
        interp_ret = ylinterpret(ylutstr_string(&dyb), ylutstr_len(&dyb));
        if( !((bOK && YLOk == interp_ret)
              || (!bOK && YLOk != interp_ret)) ) {
            printf("vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"
                   "****        TEST FAILS             *****\n"
                   "* File : %s\n"
                   "* %d-th Section\n"
                   "* Exp\n"
                   "%s\n", fname, seccnt, ylutstr_string(&dyb));
            exit(0);
        }
        ylutstr_reset(&dyb);
    }
    yldynb_clean(&dyb);
}


static void
_start_test() {
    char**       pf;
    unsigned int nr;
    int          i;
    FILE*        fh;
    pf = _testrs_files(&nr);
    for(i=0; i<nr; i++) {
        fh = fopen(pf[i], "r");
        assert(fh);
        _start_test_script(pf[i], fh);
        fclose(fh);
    }
}

/* =================================
 * To use ylisp interpreter
 * ================================= */

static unsigned int _mblk = 0;

void*
_malloc(unsigned int size) {
    _mblk++;
    return malloc(size);
}

void
_free(void* p) {
    assert(_mblk >= 0);
    _mblk--;
    free(p);
}

int
get_mblk_size() {
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

void
_assert(int a) {
    if(!a){ assert(0); }
}

int
main(int argc, char* argv[]) {
    ylsys_t   sys;

    /* ylset system parameter */
    sys.print = printf;
    sys.log = _log;
    sys.assert = _assert;
    sys.malloc = _malloc;
    sys.free = _free;
    sys.mpsz = 16*1024;
    sys.gctp = 1;

    ylinit(&sys);

    _start_test();

    yldeinit();
    assert(0 == get_mblk_size());

    printf("\n\n\n"
           "=======================================\n"
           "!!!!!   Test Success - GOOD JOB   !!!!!\n"
           "=======================================\n"
           "\n\n\n");
}
