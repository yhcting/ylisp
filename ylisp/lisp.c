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



#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "lisp.h"


/*=======================
 * Local varaible
 *=======================*/
static ylsys_t _sysv;

/*
 * static variable is initialized as 0!
 * So, we don't need to 'memset(&.., 0, ...) explicitly.
 */
static yle_t   _predefined_true;
static yle_t   _predefined_nil;
static yle_t   _predefined_quote;

/* to improve performance... global symbol is used */
const yle_t* const ylg_predefined_true   = &_predefined_true;
const yle_t* const ylg_predefined_nil    = &_predefined_nil;
const yle_t* const ylg_predefined_quote  = &_predefined_quote;

/*=======================
 * Globally used one
 *=======================*/
static pthread_mutexattr_t _mattr;

/*---------------------------------
 * For debugging !
 *---------------------------------*/


/* =======================
 * aif interfaces - START
 * =======================*/

#define _DEFAIF_EQ_START(sUFFIX)                                        \
    static int                                                          \
    _aif_##sUFFIX##_eq(const yle_t* e0, const yle_t* e1) {              \
        do

#define _DEFAIF_EQ_END while(0); ylassert(0); }

#define _DEFAIF_COPY_START(sUFFIX)                                      \
    static int                                                          \
    _aif_##sUFFIX##_copy(void* map, yle_t* n, const yle_t* e) {         \
            do

#define _DEFAIF_COPY_END while(0); return 0; }

#define _DEFAIF_TO_STRING_START(sUFFIX)                                 \
    static int                                                          \
    _aif_##sUFFIX##_to_string(const yle_t* e, char* b, unsigned int sz) { \
    do

#define _DEFAIF_TO_STRING_END while(0); ylassert(0); }

#define _DEFAIF_CLEAN_START(sUFFIX)                                     \
    static void                                                         \
    _aif_##sUFFIX##_clean(yle_t* e) {                                   \
        do

#define _DEFAIF_CLEAN_END while(0); }

/* --- aif sym --- */
_DEFAIF_EQ_START(sym) {
    /* trivial case : compare pointed address first */
    if(ylasym(e0).sym == ylasym(e1).sym) {
        /* predefined nil can be compared here */
        return 1;
    } else {
        return (0 == strcmp(ylasym(e0).sym, ylasym(e1).sym))? 1: 0;
    }
} _DEFAIF_EQ_END

#if 0 /* Keep it for future use! */
_DEFAIF_COPY_START(sym) {
    /* deep clone */
    ylasym(n).sym = ylmalloc(strlen(ylasym(e).sym)+1);
    if(!ylasym(n).sym) {
        yllogE1("Out Of Memory : [%d]!\n", strlen(ylasym(e).sym)+1);
        ylinterpret_undefined(YLErr_out_of_memory);
    }
    strcpy(ylasym(n).sym, ylasym(e).sym);
} _DEFAIF_COPY_END
#endif /* Keep it for future use! */

_DEFAIF_TO_STRING_START(sym) {
    int slen = strlen(ylasym(e).sym);
    if(slen > sz) { return -1; }
    memcpy(b, ylasym(e).sym, slen);
    return slen;
} _DEFAIF_TO_STRING_END

_DEFAIF_CLEAN_START(sym) {
    if(ylasym(e).sym) { ylfree(ylasym(e).sym); }
} _DEFAIF_CLEAN_END


/* --- aif nfunc --- */
_DEFAIF_EQ_START(nfunc) {
    if(ylanfunc(e0).f == ylanfunc(e1).f) { return 1; }
    else { return 0; }
} _DEFAIF_EQ_END

#if 0 /* Keep it for future use! */
_DEFAIF_COPY_START(nfunc) {
    /* nothing to do */
} _DEFAIF_COPY_END
#endif /* Keep it for future use! */

_DEFAIF_TO_STRING_START(nfunc) {
    int bw = snprintf(b, sz, "<!%s!>", ylanfunc(e).name);
    if(bw >= sz)  { return -1; }  /* buffer size is not enough */
    return bw;
} _DEFAIF_TO_STRING_END

_DEFAIF_CLEAN_START(nfunc) {
    /* nothing to do */
} _DEFAIF_CLEAN_END

/* --- aif sfunc --- */
_DEFAIF_EQ_START(sfunc) {
    if(ylanfunc(e0).f == ylanfunc(e1).f) { return 1; }
    else { return 0; }
} _DEFAIF_EQ_END

#if 0 /* Keep it for future use! */
_DEFAIF_COPY_START(sfunc) {
    /* nothing to do */
} _DEFAIF_COPY_END
#endif /* Keep it for future use! */

_DEFAIF_TO_STRING_START(sfunc) {
    return _aif_nfunc_to_string(e, b, sz);
} _DEFAIF_TO_STRING_END

_DEFAIF_CLEAN_START(sfunc) {
    /* nothing to do */
} _DEFAIF_CLEAN_END


/* --- aif sfunc --- */
/* This SHOULD BE SAME with NFUNC.. So, let's skip it! */

/* --- aif double --- */
_DEFAIF_EQ_START(dbl) {
    return (yladbl(e0) == yladbl(e1))? 1: 0;
} _DEFAIF_EQ_END

#if 0 /* Keep it for future use! */
_DEFAIF_COPY_START(dbl) {
    /* nothing to do */
} _DEFAIF_COPY_END
#endif /* Keep it for future use! */

_DEFAIF_TO_STRING_START(dbl) {
    int bw;
    if( (double)((long long)yladbl(e)) == yladbl(e)) {
        bw = snprintf(b, sz, "%lld", (long long)(yladbl(e)));
    } else {
        bw = snprintf(b, sz, "%f", yladbl(e));
    }
    if(bw >= sz) { return -1; } /* not enough buffer */
    return bw;
} _DEFAIF_TO_STRING_END

_DEFAIF_CLEAN_START(dbl) {
    /* nothing to do */
} _DEFAIF_CLEAN_END

/* --- aif binary --- */
_DEFAIF_EQ_START(bin) {
    if(ylabin(e0).sz == ylabin(e1).sz
       && (0 == memcmp(ylabin(e0).d, ylabin(e1).d, ylabin(e0).sz)) ) {
        return 1;
    } else {
        return 0;
    }
} _DEFAIF_EQ_END

#if 0 /* Keep it for future use! */
_DEFAIF_COPY_START(bin) {
    /* deep clone */
    ylabin(n).d = ylmalloc(ylabin(e).sz);
    if(!ylabin(n).d) {
        yllogE1("Out Of Memory : [%d]!\n", ylabin(e).sz);
        ylinterpret_undefined(YLErr_out_of_memory);
    }
    memcpy(ylabin(n).d, ylabin(e).d, ylabin(e).sz);
} _DEFAIF_COPY_END
#endif /* Keep it for future use! */

_DEFAIF_TO_STRING_START(bin) {
    /* make binary data to string directly! */
    if(ylabin(e).sz > sz) { return -1; }
    else { memcpy(b, ylabin(e).d, ylabin(e).sz); return ylabin(e).sz; }
#undef S
} _DEFAIF_TO_STRING_END

_DEFAIF_CLEAN_START(bin) {
    if(ylabin(e).d) { ylfree(ylabin(e).d); }
} _DEFAIF_CLEAN_END

/* --- aif nil --- */
_DEFAIF_EQ_START(nil) {
    return (e0 == e1 && e0 == ylnil())? 1: 0;
} _DEFAIF_EQ_END

#if 0 /* Keep it for future use! */
_DEFAIF_COPY_START(nil) {
    yllogE0("NIL is NOT ALLOWED TO COPY\n");
    ylinterpret_undefined(YLErr_eval_undefined);
} _DEFAIF_COPY_END
#endif /* Keep it for future use! */

_DEFAIF_TO_STRING_START(nil) {
#define S "NIL"
    if(sizeof(S) > sz) { return -1; }
    else { memcpy(b, S, sizeof(S)-1); return sizeof(S)-1; }
#undef S
} _DEFAIF_TO_STRING_END

_DEFAIF_CLEAN_START(nil) {
    ylassert(0); /* un-recovable error!! */
    yllogE0("NIL SHOULD NOT BE CLEANED\n");
    ylinterpret_undefined(YLErr_eval_undefined);
} _DEFAIF_CLEAN_END


#undef _MAX_ATOM_PR_BUFSZ

#undef _DEFAIF_EQ_START
#undef _DEFAIF_EQ_END
#undef _DEFAIF_COPY_START
#undef _DEFAIF_COPY_END
#undef _DEFAIF_TO_STRING_START
#undef _DEFAIF_TO_STRING_END
#undef _DEFAIF_CLEAN_START
#undef _DEFAIF_CLEAN_END



#define _DEFAIF_VAR(sUFFIX)                                             \
    static const ylatomif_t _aif_##sUFFIX = {                           \
        &_aif_##sUFFIX##_eq,                                            \
        NULL,                                                           \
        &_aif_##sUFFIX##_to_string,                                     \
        NULL,                                                           \
        &_aif_##sUFFIX##_clean                                          \
    };                                                                  \
    const ylatomif_t* const ylg_predefined_aif_##sUFFIX = &_aif_##sUFFIX


_DEFAIF_VAR(sym);
_DEFAIF_VAR(sfunc);
_DEFAIF_VAR(nfunc);
_DEFAIF_VAR(dbl);
_DEFAIF_VAR(bin);
_DEFAIF_VAR(nil);

#undef _DEFAIF_VAR


void
yleclean(yle_t* e) {
    if(yleis_atom(e)) {
        if(ylaif(e)->clean) { (*ylaif(e)->clean)(e); }
        else { yllogW0("There is an atom that doesn't support/allow CLEAN!\n"); }
    } else {
        /*
         * Let'a think of memory block M that is taken from pool, but not initialized.
         * Value of M is invald. But, All cleaned memory block has NULL car and cdr value.
         * (see bottom of this funcion and ylmp_block().)
         * If there is interpreting error before M is initialized, M is GCed.
         * In this case, car and cdr of M can be NULL!.
         * But, case that only one of car and cdr is NULL, is unexpected!
         * This is just 'SANITY CHECK'
         */
        ylassert( (ylpcar(e) && ylpcdr(e)) || (!ylpcar(e) && !ylpcdr(e)) );
    }
    /*
     * And after cleaning, we need to be in clean-state.
     * -> See comments of 'ylmp_clean_block'
     */
    ylmp_clean_block(e);
}

ylerr_t
ylregister_nfunc(unsigned int version,
                 const char* sym, ylnfunc_t nfunc,
                 const ylatomif_t* aif, /* AIF. can be ylaif_sfunc / ylaif_nfunc */
                 const char* desc) {
    yle_t*       e;

    if(yldev_ver_major(version) < yldev_ver_major(YLDEV_VERSION)) {
        yllogE4("Version of CNF library is lower than ylisp!!\n"
              "  ylisp[%d.%d], cnf[%d.%d]\n",
              yldev_ver_major(YLDEV_VERSION), yldev_ver_minor(YLDEV_VERSION),
              yldev_ver_major(version), yldev_ver_minor(version));
        return YLErr_cnf_register;
    }

    if(ylaif_nfunc() == aif) {
        e = ylacreate_nfunc(nfunc, sym);
    } else if(ylaif_sfunc() == aif) {
        e = ylacreate_sfunc(nfunc, sym);
    } else {
        yllogE0("Function type should be one of ATOM_NFUNC, ylor ATOM_RAW_NFUNC");
        return YLErr_cnf_register;
    }

    /* default symbol type is 0 */
    if(1 == ylgsym_insert(sym, 0, e) ) {
        /*
         * logging warning is not needed here. (This is duplicated.)
         * Warning is shown at symlookup.c when nfunc or sfunc type symbol is overwritten.

        yllogW1("WARN! : [Native Function] Symbol Overwritten!\n"
                "    symbol : %s\n", sym);
        */
    }
    ylgsym_set_description(sym, desc);

    return YLOk;
}

void
ylunregister_nfunc(const char* sym) {
    ylgsym_delete(sym);
}


const ylsys_t*
ylsysv() {
    return &_sysv;
}

pthread_mutexattr_t*
ylmutexattr() {
    return &_mattr;
}

struct yldynb*
ylethread_buf(yletcxt_t* cxt) {
    return &cxt->dynb;
}

int
ylelist_size(const yle_t* e) {
    register int i;
    const yle_t* p;
    ylassert(e);
    /* ylnil() is special case... */
    if(ylnil() == e) { return 0; }
    else if(yleis_atom(e)) { return 1; }

    i = 1; p = ylcdr((yle_t*)e);
    while(!yleis_atom(p)) {
        p = ylcdr((yle_t*)p);
        i++;
    }
    return i;
}

/*
 * Actually, this print-stuffs are ugly to me...
 * But, it works well without external dependency to handle string..
 * ...Hmm... Let's keep it at this moment...
 * But... Re-factoring may be required at future.
 */

/*
 * Simple macro used for printing functions(_aprint, _eprint, yleprint)
 */
#define _fcall(fEXP) do { if(0 > fEXP) { goto bail; } } while(0)

static int
_aprint(yldynb_t* b, yle_t* e) {
    int  cw; /* character written */
    if(ylaif(e)->to_string) {
        while(1) {
            cw = ylaif(e)->to_string(e, (char*)yldynbstr_ptr(b), yldynb_freesz(b));
            if( cw >= 0) {
                b->sz += cw;
                b->b[b->sz-1] = 0; /* add trailing 0 */
                break; /* success */
            } else {
                if(0 > yldynb_expand(b)) { goto bail; }
            }
        }
    } else {
        yllogW0("There is an atom that doesn't support/allow PRINT!\n");
        /* !X! is special notation to represet 'it's not printable' */
        _fcall(yldynbstr_append(b, "!X!"));
    }
    return 0;

 bail:
    return -1;
}

static int
_eprint(yldynb_t* b, yle_t* e, yltrie_t* map) {
    if(yleis_atom(e)) {
        _fcall(_aprint(b, e));
    } else {
        yle_t* car_e = ylcar(e);
        yle_t* cdr_e = ylcdr(e);

        /*
         * When cdr/car can be NULL? Free block in memory pool has NULL car/cdr value.
         * Tring to print garbage expr. may lead to the case of printing free block.
         * To handle this exceptional case, NULL car/cdr should be treated well.
         */
        if(car_e) {
            if(!yleis_atom(car_e)) {
                if(1 == yltrie_insert(map, (unsigned char*)&car_e,
                                      sizeof(yle_t*), ylnil())) {
                    /* Overwritten! cycle detected ! */
                    _fcall(yldynbstr_append(b, "!CYCLE DETECTED!"));
                    /*
                     * car_e already exists in map.
                     * So, it should not be deleted from map
                     */
                    car_e = NULL;
                } else {
                    _fcall(yldynbstr_append(b, "("));
                    _fcall(_eprint(b, car_e, map));
                    _fcall(yldynbstr_append(b, ")"));
                }
            } else {
                _fcall(_aprint(b, car_e));
            }
        } else {
            _fcall(yldynbstr_append(b, "NULL "));
        }

        if(cdr_e) {
            if( yleis_atom(cdr_e) ) {
                if(!yleis_nil(cdr_e)) {
                    _fcall(yldynbstr_append(b, "."));
                    _fcall(_aprint(b, cdr_e));
                }
            } else {
                if(1 == yltrie_insert(map, (unsigned char*)&cdr_e,
                                      sizeof(yle_t*), ylnil())) {
                    /* Overwritten! cycle detected ! */
                    _fcall(yldynbstr_append(b, "!CYCLE DETECTED!"));
                    /*
                     * car_e already exists in map.
                     * So, it should not be deleted from map
                     */
                    cdr_e = NULL;
                } else {
                    _fcall(yldynbstr_append(b, " "));
                    _fcall(_eprint(b, cdr_e, map));
                }
            }
        } else {
            _fcall(yldynbstr_append(b, "NULL"));
        }

        /*
         * At upper node, car/cdr of this node are not considered to detect cycle.
         */
        if(car_e) { yltrie_delete(map, (unsigned char*)&car_e, sizeof(yle_t*)); }
        if(cdr_e) { yltrie_delete(map, (unsigned char*)&cdr_e, sizeof(yle_t*)); }
    }

    return 0;

 bail:
    return -1;
}

static int
_echain_print(const yle_t* e, yldynb_t* b, yltrie_t* map) {
    if(yleis_atom(e)) {
        _fcall(_aprint(b, (yle_t*)e));
    } else {
        _fcall(yldynbstr_append(b, "("));
        _fcall(_eprint(b, (yle_t*)e, map));
        _fcall(yldynbstr_append(b, ")"));
    }
    return 0;

 bail:
    return -1;
}

/**
 * We need to detect cycle in case of print!...
 * Print is used for debugging perforce too.
 * So, it should handle circular list....
 * And we don't need to worry about performance too much for printing.
 */
const char*
ylechain_print(struct yldynb* dynb, const yle_t* e) {
#define __ERRMSG       "print error: out of memory\n"
    /* Trie for address map */
    yltrie_t*  map = yltrie_create(NULL);

    yldynbstr_reset(dynb);
    /* new allocates or shrinks */
    _fcall(_echain_print(e, dynb, map));
    yltrie_destroy(map);
    return (char*)yldynbstr_string(dynb);

 bail:
    yltrie_destroy(map);
    ylassert(yldynb_sz(dynb) > sizeof(__ERRMSG));
    memcpy(yldynb_buf(dynb), (unsigned char*)__ERRMSG, sizeof(__ERRMSG));
    return (char*)yldynbstr_string(dynb);
#undef __ERRMSG
}

#undef _fcall


#define _YLREADV_PROLOGUE                                               \
    { /* Just scope */                                                  \
    int      sv, ty;                                                    \
    ylerr_t  ret = YLOk;                                                \
    yle_t*   e;                                                         \
                                                                        \
    /*                                                                  \
     * GC should be disabled temporarily.                               \
     * If not, returned yle_t* value may be GCed before used.           \
     * Keep it in mind that 'ylisp' supports MT(Multi-threading!)       \
     */                                                                 \
    sv = ylmp_gc_enable (0);                                            \
    e = ylgsym_get (&ty, sym);                                          \
    { /* Just scope */

#define _YLREADV_EPILOGUE                       \
    }                                           \
    ylmp_gc_enable (sv);                        \
    return ret;                                 \
    }

ylerr_t
ylreadv_ptr (const char* sym, void** out) {
    /* check input parameter */
    if (!sym || !out) return YLErr_invalid_param;

    _YLREADV_PROLOGUE;

    if (!e) ret = YLErr_invalid_param;
    else *out = ylacd (e);

    _YLREADV_EPILOGUE;
}

ylerr_t
ylreadv_str (const char* sym, char* buf, unsigned int bsz) {
    /* check input parameter */
    if (!sym || !buf || 0 == bsz) return YLErr_invalid_param;

    _YLREADV_PROLOGUE;

    if (!e || ylaif (e) != ylaif_sym ()) ret = YLErr_invalid_param;
    else {
        int len = strlen (ylasym (e).sym) + 1;
        len = (bsz < len)? bsz: len;
        memcpy (buf, ylasym (e).sym, len);
    }

    _YLREADV_EPILOGUE;
}

ylerr_t
ylreadv_bin (const char* sym, unsigned char* buf, unsigned int bsz) {
    /* check input parameter */
    if (!sym || !buf || 0 == bsz) return YLErr_invalid_param;

    _YLREADV_PROLOGUE;

    if (!e || ylaif (e) != ylaif_bin ()) ret = YLErr_invalid_param;
    else {
        unsigned int len = ylabin (e).sz;
        len = (bsz < len)? bsz: len;
        memcpy (buf, ylasym (e).sym, len);
    }

    _YLREADV_EPILOGUE;
}

ylerr_t
ylreadv_dbl (const char* sym, double* out) {
    /* check input parameter */
    if (!sym || !out) return YLErr_invalid_param;

    _YLREADV_PROLOGUE;

    if (!e || ylaif (e) != ylaif_dbl ()) ret = YLErr_invalid_param;
    else *out = yladbl (e);

    _YLREADV_EPILOGUE;
}

#undef _YLREADV_PROLOGUE
#undef _YLREADV_EPILOGUE


int
ylsym_nr_candidates(const char* start_with, unsigned int* max_symlen) {
    return ylgsym_nr_candidates(start_with, max_symlen);
}

/**
 * @return: <0: error. Otherwise number of candidates found.
 */
int
ylsym_candidates(const char* start_with,
                 char** ppbuf,       /* in/out */
                 unsigned int ppbsz, /* size of ppbuf - regarding 'ppbuf[i]' */
                 unsigned int pbsz) {/* size of pbuf - regarding 'ppbuf[0][x]' */
    return ylgsym_candidates(start_with, ppbuf, ppbsz, pbsz);
}

int
ylsym_auto_complete(const char* start_with, char* buf, unsigned int bufsz) {
    return ylgsym_auto_complete(start_with, buf, bufsz);
}

ylerr_t
ylinit_thread_context(yletcxt_t* cxt) {
    /* TODO -- Error check!! */
    yllist_init_link(&cxt->lk);
    cxt->base_id = pthread_self();
    pthread_mutex_init(&cxt->m, ylmutexattr());
    cxt->sig = 0;
    cxt->state = 0;
    cxt->thdstk = ylstk_create(0, NULL);
    cxt->evalstk = ylstk_create(0, NULL);
    yllist_init_link(&cxt->pres);
    cxt->slut = ylslu_create();
    yldynb_init(&cxt->dynb, 4096);
    return YLOk;
}

void
yldeinit_thread_context(yletcxt_t* cxt) {
    yldynb_clean(&cxt->dynb);
    ylslu_destroy(cxt->slut);
    ylstk_destroy(cxt->evalstk);
    ylstk_destroy(cxt->thdstk);
    pthread_mutex_destroy(&cxt->m);
}



/* =======================
 * aif interfaces - END
 * =======================*/

#define NFUNC(n, s, type, desc) extern YLDECLNF(n);
#include "nfunc.in"
#undef NFUNC

/* init this system */
/* this SHOULD BE called first */
ylerr_t
ylinit(ylsys_t* sysv) {
    /* Check system parameter! */
    if(!(sysv && sysv->print
         && sysv->assert && sysv->malloc
         && sysv->free
         && sysv->gctp > 0 && sysv->gctp < 100)) {
        goto bail;
    }

    memcpy(&_sysv, sysv, sizeof(_sysv));

    if(pthread_mutexattr_init(&_mattr)) { ylassert(0); }
    if(pthread_mutexattr_settype(&_mattr, PTHREAD_MUTEX_ERRORCHECK)) { ylassert(0); }


    /* Basic Arhitecture Assumption!! */
    if(!(8 == sizeof(double)
         && 4 == sizeof(int)
         && 8 == sizeof(long long))) {
        ylprint(("!!!!! WARNING !!!!!\n"
                 "    Some operations may assumes that\n"
                 "        sizeof(double) == 8 && sizeof(long long) == 8 && sizeof(int) == 4\n"
                 "    But, host environment is different.\n"
                 "        double[%d], long long[%d], int[%d]\n"
                 "    So, some numeric operation may return unexpected value.!\n",
                 sizeof(double), sizeof(long long), sizeof(int)));
    }

    if( YLOk != ylsfunc_init()
        || YLOk != ylmt_init()
        || YLOk != ylgsym_init()
        || YLOk != ylnfunc_init()
        || YLOk != ylmp_init()
        || YLOk != ylinterp_init()) {
        goto bail;
    }

    /*
     * '_predefined_xxxx' SHOULD NOT be freed!!!!
     * So, passing data pointer is OK
     */
    ylaassign_sym(ylt(),  "t");
    ylaassign_sym(ylq(),  "quote");

    /* set nil's type as YLAUnknown -- for easier-programming */
    yleset_type(ylnil(), YLEAtom);
    ylaif(ylnil()) = &_aif_nil;

#define NFUNC(n, s, aif, desc)                                         \
    if(YLOk != ylregister_nfunc(YLDEV_VERSION, s, (ylnfunc_t)YLNFN(n), aif, ">> lib: ylisp <<\n" desc)) { goto bail; }
#include "nfunc.in"
#undef NFUNC

    return YLOk;

 bail:
    return YLErr_init;
}

void
yldeinit() {
    pthread_mutexattr_destroy(&_mattr);
    ylinterp_deinit();
    ylmp_deinit();
    ylgsym_deinit();
    ylmt_deinit();
    ylsfunc_deinit();
}
