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

#include "gsym.h"
#include "mempool.h"
#include "lisp.h"


/*=======================
 * Local varaible 
 *=======================*/
static ylsys_t* _sysv = NULL;
static ylerr_t  _err  = YLOk;

/* 
 * print buffer. This is used in ylechain_print 
 * (DO NOT USE THIS ELSEWHERE)
 */
static yldynb_t _prdynb = {0, 0, NULL};

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
 * Global functions 
 *=======================*/


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
    static const char*                                                  \
    _aif_##sUFFIX##_to_string(const yle_t* e) {                         \
        do

#define _DEFAIF_TO_STRING_END while(0); ylassert(0); }

#define _DEFAIF_CLEAN_START(sUFFIX)                                     \
    static void                                                         \
    _aif_##sUFFIX##_clean(yle_t* e) {                                   \
        do

#define _DEFAIF_CLEAN_END while(0); }


#define _MAX_ATOM_PR_BUFSZ   256
static char _atom_prbuf[_MAX_ATOM_PR_BUFSZ];

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

_DEFAIF_COPY_START(sym) {
    /* deep clone */
    ylasym(n).sym = ylmalloc(strlen(ylasym(e).sym)+1);
    if(!ylasym(n).sym) {
        yllogE1("Out Of Memory : [%d]!\n", strlen(ylasym(e).sym)+1);
        ylinterpret_undefined(YLErr_out_of_memory);
    }
    strcpy(ylasym(n).sym, ylasym(e).sym);
} _DEFAIF_COPY_END

_DEFAIF_TO_STRING_START(sym) {
    return ylasym(e).sym;
} _DEFAIF_TO_STRING_END

_DEFAIF_CLEAN_START(sym) {
    if(ylasym(e).sym) { ylfree(ylasym(e).sym); }
} _DEFAIF_CLEAN_END


/* --- aif nfunc --- */
_DEFAIF_EQ_START(nfunc) {
    if(ylanfunc(e0).f == ylanfunc(e1).f) { return 1; }
    else { return 0; }
} _DEFAIF_EQ_END

_DEFAIF_COPY_START(nfunc) {
    /* nothing to do */
} _DEFAIF_COPY_END

_DEFAIF_TO_STRING_START(nfunc) {
    if(_MAX_ATOM_PR_BUFSZ <= snprintf(_atom_prbuf, _MAX_ATOM_PR_BUFSZ, 
                                      "<!%s!>", ylanfunc(e).name)) {
        /* atom pr buf is too small.. */
        yllogE1("Atom print buffer size is not enough!: Current [%d]\n", _MAX_ATOM_PR_BUFSZ);
        ylassert(0);
    }
    return _atom_prbuf;
} _DEFAIF_TO_STRING_END

_DEFAIF_CLEAN_START(nfunc) {
    /* nothing to do */
} _DEFAIF_CLEAN_END

/* --- aif sfunc --- */
_DEFAIF_EQ_START(sfunc) {
    if(ylanfunc(e0).f == ylanfunc(e1).f) { return 1; }
    else { return 0; }
} _DEFAIF_EQ_END

_DEFAIF_COPY_START(sfunc) {
    /* nothing to do */
} _DEFAIF_COPY_END

_DEFAIF_TO_STRING_START(sfunc) {
    if(_MAX_ATOM_PR_BUFSZ <= snprintf(_atom_prbuf, _MAX_ATOM_PR_BUFSZ, 
                                      "<!%s!>", ylanfunc(e).name)) {
        /* atom pr buf is too small.. */
        yllogE1("Atom print buffer size is not enough!: Current [%d]\n", _MAX_ATOM_PR_BUFSZ);
        ylassert(0);
    }
    return _atom_prbuf;
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

_DEFAIF_COPY_START(dbl) {
    /* nothing to do */
} _DEFAIF_COPY_END

_DEFAIF_TO_STRING_START(dbl) {
    int cnt;
    if((double)((int)yladbl(e)) == yladbl(e)) {
        cnt = snprintf(_atom_prbuf, _MAX_ATOM_PR_BUFSZ, "%d", (int)(yladbl(e)));
    } else {
        cnt = snprintf(_atom_prbuf, _MAX_ATOM_PR_BUFSZ, "%f", yladbl(e));
    }
    if(_MAX_ATOM_PR_BUFSZ <= cnt) {
        /* atom pr buf is too small.. */
        yllogE1("Atom print buffer size is not enough!: Current [%d]\n", _MAX_ATOM_PR_BUFSZ);
        ylassert(0);
    }
    return _atom_prbuf;
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

_DEFAIF_COPY_START(bin) {
    /* deep clone */
    ylabin(n).d = ylmalloc(ylabin(e).sz);
    if(!ylabin(n).d) {
        yllogE1("Out Of Memory : [%d]!\n", ylabin(e).sz);
        ylinterpret_undefined(YLErr_out_of_memory);
    }
    memcpy(ylabin(n).d, ylabin(e).d, ylabin(e).sz);
} _DEFAIF_COPY_END

_DEFAIF_TO_STRING_START(bin) {
    return ">>BIN DATA<<";
} _DEFAIF_TO_STRING_END

_DEFAIF_CLEAN_START(bin) {
    if(ylabin(e).d) { ylfree(ylabin(e).d); }
} _DEFAIF_CLEAN_END

/* --- aif nil --- */
_DEFAIF_EQ_START(nil) {
    return (e0 == e1 && e0 == ylnil())? 1: 0;
} _DEFAIF_EQ_END

_DEFAIF_COPY_START(nil) {
    yllogE0("NIL is NOT ALLOWED TO COPY\n");
    ylinterpret_undefined(YLErr_eval_undefined);
} _DEFAIF_COPY_END

_DEFAIF_TO_STRING_START(nil) {
    return "NIL";
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
        &_aif_##sUFFIX##_copy,                                          \
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

static yle_t*
_eclone(const yle_t* e) {
    yle_t* n; /* new one */

    ylassert(e);

    /* handle special case "nil" */
    if(yleis_nil(e)) { return ylnil(); }

    n = ylmp_block();
    memcpy(n, e, sizeof(yle_t));
    if(yleis_atom(e)) {
        if( ylaif(e)->copy && (0 > ylaif(e)->copy(NULL, n, e)) ) {
            yllogE0("There is an error at COPYING atom\n");
            ylinterpret_undefined(YLErr_eval_undefined);
        }
    }
    return n;
}

static yle_t*
_echain_clone(const yle_t* e) {
    yle_t* n;
    if(yleis_atom(e)) {
        n = _eclone(e);
    } else {
        n = _eclone(e);
        ylpsetcar(n, _echain_clone(ylpcar(e)));
        ylpsetcdr(n, _echain_clone(ylpcdr(e)));
    }
    return n;
}

yle_t*
ylechain_clone(const yle_t* e) {
    return _echain_clone(e);
}

ylerr_t
ylregister_nfunc(unsigned int version,
                 const char* sym, ylnfunc_t nfunc, 
                 const ylatomif_t* aif, /* AIF. can be ylaif_sfunc / ylaif_nfunc */
                 const char* desc) {
    yle_t*       e;
    char*        s;
    
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
    ylgsym_insert(sym, 0, e);
    ylgsym_set_description(sym, desc);

    return YLOk;
}

void
ylunregister_nfunc(const char* sym) {
    yltrie_delete(sym);
}


const ylsys_t*
ylsysv() {
    return _sysv;
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
        _fcall(ylutstr_append(b, "%s", (*ylaif(e)->to_string)(e)));
    } else {
        yllogW0("There is an atom that doesn't support/allow PRINT!\n");
        /* !X! is special notation to represet 'it's not printable' */
        _fcall(ylutstr_append(b, "!X!"));
    }
    return 0;

 bail:
    return -1;
}

static int
_eprint(yldynb_t* b, yle_t* e) {
    if(yleis_atom(e)) {
        _fcall(_aprint(b, e));
    } else {
        /*
         * When cdr/car can be NULL? Free block in memory pool has NULL car/cdr value.
         * Tring to print garbage expr. may lead to the case of printing free block.
         * To handle this exceptional case, NULL car/cdr should be treated well.
         */
        if(ylcar(e)) {
            if(!yleis_atom(ylcar(e))) {
                _fcall(ylutstr_append(b, "("));
                _fcall(_eprint(b, ylcar(e)));
                _fcall(ylutstr_append(b, ")"));
            } else {
                _fcall(_aprint(b, ylcar(e)));
            }
        } else {
            _fcall(ylutstr_append(b, "NULL "));
        }

        if(ylcdr(e)) {
            if( yleis_atom(ylcdr(e)) ) {
                if(!yleis_nil(ylcdr(e))) {
                    _fcall(ylutstr_append(b, "."));
                    _fcall(_aprint(b, ylcdr(e)));
                }
            } else {
                _fcall(ylutstr_append(b, " "));
                _fcall(_eprint(b, ylcdr(e)));
            }
        } else {
            _fcall(ylutstr_append(b, "NULL"));
        }
    }
    return 0;

 bail:
    return -1;
}

int
ylechain_print_internal(const yle_t* e, yldynb_t* b) {
    if(yleis_atom(e)) {
        _fcall(_aprint(b, (yle_t*)e));
    } else {
        _fcall(ylutstr_append(b, "("));
        _fcall(_eprint(b, (yle_t*)e));
        _fcall(ylutstr_append(b, ")"));
    }
    return 0;

 bail:
    return -1;
}

/**
 * @return: buffer pointer. MAX buffer size is 1KB
 */
const char*
ylechain_print(const yle_t* e) {

#define __DEFAULT_BSZ         4096
    /*
     * newly allocates or shrinks
     */
    if(!_prdynb.b || ylutstr_len(&_prdynb) > __DEFAULT_BSZ) {
        yldynb_clean(&_prdynb);
        _fcall(ylutstr_init(&_prdynb, __DEFAULT_BSZ));
    } else {
        ylutstr_reset(&_prdynb);
    }

    _fcall(ylechain_print_internal(e, &_prdynb));
    return ylutstr_string(&_prdynb);
    
 bail:
    return "print error: out of memory\n";

#undef __DEFAULT_BSZ
}

#undef _fcall

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
    ylerr_t      ret = YLOk;
    register int i;

    /* Check system parameter! */
    if(!(sysv && sysv->print
         && sysv->assert && sysv->malloc 
         && sysv->free )) { 
        goto bail;
    }
    
    _sysv = sysv;

    if( YLOk != ylsfunc_init()
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
    ylgsym_deinit();
    ylmp_deinit();
    ylinterp_deinit();
    yldynb_clean(&_prdynb);
}
