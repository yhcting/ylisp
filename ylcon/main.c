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



#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>
#include <malloc.h>
#include <signal.h>
#include <sys/time.h>

#include "ylisp.h"

static int _loglv = YLLogW;

static int _handle_console_command(char* stream);

static struct {
    char*         b;      /* buffer */
    unsigned int  limit;  /* buffer limit */
    unsigned int  sz;     /* buffer size */
} _ib; /* input buffer */


static inline void
_ib_init() {
    _ib.sz = 0;
    _ib.limit = 8*1024; /* 8K buffer */
    _ib.b = malloc(sizeof(char)*_ib.limit);
}

static inline void
_ib_add_char(char c) {
    if(_ib.sz >= _ib.limit) {
        char* n;
        _ib.limit *= 2;
        n = malloc(sizeof(char)*_ib.limit);
        memcpy(n, _ib.b, _ib.sz);
        free(_ib.b);
        _ib.b = n;
    }
    _ib.b[_ib.sz++] = c;
}

static inline void
_ib_clean() {
    _ib.sz = 0;
}

static inline void
_ib_complete() {
    _ib_add_char(0); /* add ylnull-terminator */
}

static inline char*
_ib_stream() {
    return _ib.b;
}

static inline unsigned int
_ib_streamsz() {
    return _ib.sz;
}


static void
_sig_handler_int(int param) {
    printf("------- canceled --------\n");
    _ib_clean();
}


static void
_sig_handler_tstp(int param) {
    if(0 == _ib_streamsz()) {
        return; /* nothing to do */
    }
    _ib_complete();

    printf("\n");
    if( !_handle_console_command(_ib_stream()) ) {
        /* this is ylnot console command */
        /* 
         * '-1' to exclude ylnull-terminator.
         */
        if(YLOk != ylinterpret(_ib_stream(), _ib_streamsz()-1)) {
            ; /* nothing to do ...*/
        }
    }

    _ib_clean();
    printf("\n");
}

static void
_assert(int a) {
    if(!a) {
        printf("ylisp meets unrecoverable internal error!\n"
               "ylcon will be terminated!\n");
        exit(0);
    }
}

static int
_set_signal_handler() {
    void    (*sh)(int);  /* sig handler */

    sh = signal(SIGINT, _sig_handler_int);
    if(SIG_ERR == sh) {
        printf("Error to set ctrl-c signal handler\n");
        return 0;
    }

    sh = signal(SIGTSTP, _sig_handler_tstp);
    if(SIG_ERR == sh) {
        signal(SIGINT, SIG_DFL); /* back to default */
        printf("Error to set ctrl-z signal handler\n");
        return 0;
    }

    return 1;
}

static int
_set_loglv(char* opt) {
    int   ret = 1;
    if(strlen(opt) > 1) {
        ret = 0;
    } else {
        switch(opt[0]) {
            case 'v': _loglv = YLLogV; break;
            case 'd': _loglv = YLLogD; break;
            case 'i': _loglv = YLLogI; break;
            case 'w': _loglv = YLLogW; break;
            case 'e': _loglv = YLLogE; break;
            default:
                printf("unknown option\n");
                ret = 0;
        }
    }
    if(ret) { printf("Log level is set\n"); }
    return ret;
}


static int
_handle_console_command(char* s) {
    static const char*  __loglvprefix = "loglv ";
    static const int    __loglvprefixsz = 6;

    if(0 == strcmp(s, "exit\n")) {
        printf("Bye...\n");
        exit(0);
        return 1;
    } else if(0 == memcmp(s, __loglvprefix, __loglvprefixsz)) {
        if(*(s+__loglvprefixsz+1) == '\n') {
            *(s+__loglvprefixsz+1) = 0;
            _set_loglv(s+__loglvprefixsz);
            return 1;
        }
    }
    return 0;
}



static int
_parse_option(int argc, char* argv[]) {
    if(1 == argc) {
        ; /* nothing to do */
    } else if (2 == argc) {
        if(!_set_loglv(argv[1])) {
            goto bail;
        }
    } else {
        goto bail;
    }

    return 1;

 bail:
    printf("usage: ylcon [log level]\n"
           "   'log level' is optional yland should be one of 'v', 'd', 'i', 'w', 'e'\n");
    return 0;
}

static void
_log(int lv, const char* format, ...) {
    if(lv >= _loglv) {
        va_list ap;
        va_start(ap, format);
        vprintf(format, ap);
        va_end(ap);
    }
}

int
main(int argc, char* argv[]) {
    ylsys_t   sys;
    char      c;
    struct timeval tv0, tv1;

    /* ylset system parameter */
    sys.print = printf;
    sys.log = _log;
    sys.assert = _assert;
    sys.malloc = malloc;
    sys.free = free;
    if(YLOk != ylinit(&sys)) {
        printf("Fail to initailize ylisp!\n");
        goto bail;
    }

    if(!_parse_option(argc, argv)) { goto bail; }
    if(!_set_signal_handler()) { goto bail; }


#define __INIT_TEST_CMD                                                 \
    "(load-cnf '../lib/libylbase.so 'yllibylbase_register)\n"           \
        "(interpret-file '../yls/base.yl)\n"                            \
        "(interpret-file '../yls/test_base.yl)\n"                       \
        "(interpret-file '../yls/ylcon_initrc.yl)\n"

    gettimeofday(&tv0, NULL);
    if(YLOk != ylinterpret(__INIT_TEST_CMD, strlen(__INIT_TEST_CMD))) {
        printf("Test Fails!!!\n");
        return 0;
    }
    gettimeofday(&tv1, NULL);

    printf("Sanity Test is passed!\n"
           "Time taken : %ld sec + %ld microsec\n"
           "Enjoy it!\n", tv1.tv_sec - tv0.tv_sec, tv1.tv_usec - tv0.tv_usec);

    _ib_init();
    while(1) {
        _ib_add_char(fgetc(stdin));
    }

    return 0;

 bail:
    return 0;
}
