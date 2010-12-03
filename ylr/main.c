#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <malloc.h>
#include <stdarg.h>
#include <assert.h>

#include "ylisp.h"
#include "ylut.h"

#define _LOGLV YLLogW

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
    int            i;
    void*          d = NULL;
    unsigned int   dsz;

    /* ylset system parameter */
    sys.print  = printf;
    sys.log    = _log;
    sys.assert = _assert;
    sys.malloc = malloc;
    sys.free   = free;
    sys.mpsz   = 1024*1024;
    sys.gctp   = 80;

    if (YLOk != ylinit(&sys)) {
        printf("Fail to initialize ylisp\n");
        exit(1);
    }

    for (i=1; i<argc; i++) {
        d = ylutfile_read(&dsz, argv[i], 1);
        if (!d && YLOk != dsz) {
            printf("Fail to read file : %s\n", argv[i]);
            exit(1);
        }
        if (YLOk != ylinterpret(d, dsz)) {
            printf("Fail to interpret!\n"
                   "    file : %s\n", argv[i]);
            exit(1);
        }
        free(d);
    }
    return 0;
}
