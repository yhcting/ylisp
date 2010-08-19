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

#include "ylisp.h"
#include "lisp.h"
#include "trie.h"
#include "mempool.h"

/*=======================
 * Local varaible 
 *=======================*/
static ylsys_t* _sysv = NULL;
static ylerr_t  _err  = YLOk;

/* 
 * print buffer. This is used in yleprint 
 * (DO NOT USE THIS ELSEWHERE)
 */
static char*    _prbuf = NULL; 

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

/**
 * ylcons requires GC. So, code becomes complicated.
 */

void
yleclean(yle_t* e) {
    if(yleis_atom(e)) {
        switch(ylatype(e)) {
            case YLASymbol: {
                if(ylasym(e).sym) { ylfree(ylasym(e).sym); }
            } break;

            case YLASfunc:
            case YLANfunc: {
                ; /* nothing to do */
            } break;

            case YLADouble: {
                ; /* nothing to do */
            } break;

            case YLABinary: {
                if(ylabin(e).d) { ylfree(ylabin(e).d); }
            } break;

            default:
                ylassert(FALSE);
        }
    } else {
        /*
         * cdr and car may be already collected by GC.
         * So, we need to check it!
         */
        ylassert( ylercnt(ylpcar(e)) && ylercnt(ylpcdr(e)) );
        if( !ylmp_is_free_block(ylpcar(e)) ) { yleunref(ylpcar(e)); }
        if( !ylmp_is_free_block(ylpcdr(e)) ) { yleunref(ylpcdr(e)); }
    }
}


yle_t*
yleclone(const yle_t* e) {
    yle_t* n; /* new one */

    ylassert(e);

    /* handle special case "nil" */
    if(ylfalse() != ylnull((yle_t*)e)) {
        return ylnil();
    }

    n = ylmp_get_block();
    memcpy(n, e, sizeof(yle_t));
    ylercnt(n) = 0; /* this is not referenced yet */
    if(yleis_atom(e)) {
        switch(ylatype(e)) {
            case YLASymbol: {
                /* deep clone */
                ylasym(n).sym = ylmalloc(strlen(ylasym(e).sym)+1);
                if(!ylasym(n).sym) {
                    yllogE1("Out Of Memory : [%d]!\n", strlen(ylasym(e).sym));
                    ylinterpret_undefined(YLErr_out_of_memory);
                }
                strcpy(ylasym(n).sym, ylasym(e).sym);
            } break;

            case YLANfunc:
            case YLASfunc: {
                ; /* nothing to do */
            } break;

            case YLADouble: {
                ; /* nothing to do */
            } break;

            case YLABinary: {
                /* deep clone */
                ylabin(n).d = ylmalloc(ylabin(e).sz);
                if(!ylabin(n).d) {
                    yllogE1("Out Of Memory : [%d]!\n", ylabin(e).sz);
                    ylinterpret_undefined(YLErr_out_of_memory);
                }
                memcpy(ylabin(n).d, ylabin(e).d, ylabin(e).sz);
            } break;

            default:
                ylassert(FALSE);
        }
    } else {
        /* 
         * we should increase reference count of each car/cdr.
         * exp, is cloned. So, one more exp refers car/cdr.
         */
        yleref(ylpcar(n));
        yleref(ylpcdr(n));
        ;/* nothing to do */
    }
    return n;
}


yle_t*
yleclone_chain(const yle_t* e) {
    yle_t* n;
    if(yleis_atom(e)) {
        n = yleclone(e);
    } else {
        n = yleclone(e);
        ylpsetcar(n, yleclone_chain(ylpcar(e)));
        ylpsetcdr(n, yleclone_chain(ylpcdr(e)));
    }
    return n;
}


ylerr_t
ylregister_nfunc(unsigned int version,
                 const char* sym, ylnfunc_t nfunc, 
                 int   ftype, /* function type. can be ATOM_NFUNC ylor ATOM_RAW_NFUNC */
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

    if(YLANfunc != ftype && YLASfunc != ftype) {
        yllogE0("Function type should be one of ATOM_NFUNC, ylor ATOM_RAW_NFUNC");
        return YLErr_cnf_register;
    }
    
    e = ylmp_get_block();
    yleset_type(e, YLEAtom | ftype);
    ylanfunc(e).f = nfunc;
    ylanfunc(e).name = sym;

    yltrie_insert(sym, TRIE_VType_set, e);
    yltrie_set_description(sym, desc);

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
ylchain_size(const yle_t* e) {
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

static char*
_aprint(char* p, char* pend, yle_t* e) {
    int  cw; /* character written */
    switch(ylatype(e)) {
        case YLASymbol: {
            cw = snprintf(p, pend-p, "%s", yleis_nil(e)? "nil": ylasym(e).sym);
        } break;

        case YLANfunc:
        case YLASfunc: {
            cw = snprintf(p, pend-p, "<!%s!>", ylanfunc(e).name);
        } break;

        case YLADouble: {
            cw = snprintf(p, pend-p, "%f", yladbl(e));
        } break;

        case YLABinary: {
            cw = snprintf(p, pend-p, ">>BIN DATA<<");
        } break;

        default:
            ylassert(FALSE);
    }
    if(cw < 0 || cw >= (pend-p)) { return NULL; }
    else { return p + cw; }
}

static char*
_eprint(char* p, char* pend, yle_t* e) {

#define __call(func) do { p = func; if(!p) { return NULL; } } while(0)
#define __addchar(c) if(p<pend) { *p++ = c; } else { return NULL; }

    if(yleis_atom(e)) {
        _aprint(p, pend, e);
    } else {
        /*
         * When cdr/car can be NULL? Free block in memory pool has NULL car/cdr value.
         * Tring to print garbage expr. may lead to the case of printing free block.
         * To handle this exceptional case, NULL car/cdr should be treated well.
         */
        if(ylcar(e)) {
            if(!yleis_atom(ylcar(e))) {
                __addchar('(');
                __call(_eprint(p, pend, ylcar(e)));
                __addchar(')');
            } else {
                __call(_aprint(p, pend, ylcar(e)));
            }
        } else {
            __addchar('N'); __addchar('U'); __addchar('L'); __addchar('L');
            __addchar(' ');
        }

        if(ylcdr(e)) {
            if( yleis_atom(ylcdr(e)) ) {
                if(!yleis_nil(ylcdr(e))) {
                    __addchar('.');
                    __call(_aprint(p, pend, ylcdr(e)));
                }
            } else {
                __addchar(' ');
                __call(_eprint(p, pend, ylcdr(e)));
            }
        } else {
            __addchar('N'); __addchar('U'); __addchar('L'); __addchar('L');
        }
    }
    return p;

#undef __addchar
#undef __call
}

/**
 * @return: buffer pointer. MAX buffer size is 1KB
 */
const char*
yleprint(const yle_t* e) {

#define __call(func) do { if(p) { p = func; } } while(0)
#define __addchar(c) if(p && p<pend) { *p++ = c; } else { p = NULL; }

    char* p;
    char* pend;
    int   sz;

    if(_prbuf) { ylfree(_prbuf); }
    sz = 4096; /* initial size 4KB */
    _prbuf = ylmalloc(sz); /* initial size */
    if(!_prbuf) { goto bail;  }
    while(1) {
        p = _prbuf; pend = p + sz -1;
        if(yleis_atom(e)) {
            __call(_aprint(p, pend, (yle_t*)e));
        } else {
            __addchar('(');
            __call(_eprint(p, pend, (yle_t*)e));
            __addchar(')');
        }
        /* add trailing NULL */
        if(p) { 
            *p++ = 0; 
            break; /* end! */
        } else { 
            /* 
             * not engouth buffer size 
             * increase buffer and retry!
             */
            sz *= 2;
            ylfree(_prbuf);
            _prbuf = ylmalloc(sz);
            if(!_prbuf) { goto bail; }
        }
    }
    return _prbuf;
    
 bail:
    /* Out of memory */
    return "print error: out of memory\n";
}

#undef __addchar
#undef __call



int
ylget_candidates_num(const char* prefix, unsigned int* max_symlen) {
    return yltrie_get_candidates_num(prefix, max_symlen);
}

/**
 * @return: <0: error. Otherwise number of candidates found.
 */
int
ylget_candidates(const char* prefix, 
                 char** ppbuf,       /* in/out */
                 unsigned int ppbsz, /* size of ppbuf - regarding 'ppbuf[i]' */
                 unsigned int pbsz) {/* size of pbuf - regarding 'ppbuf[0][x]' */
    return yltrie_get_candidates(prefix, ppbuf, ppbsz, pbsz);
}

int
ylget_more_possible_prefix(const char* prefix, char* buf, unsigned int bufsz) {
    return yltrie_get_more_possible_prefix(prefix, buf, bufsz);
}


extern ylerr_t ylnfunc_init();
extern ylerr_t ylsfunc_init();

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
        || YLOk != yltrie_init()
        || YLOk != ylnfunc_init()
        || YLOk != ylmp_init() ) {
        goto bail;
    }


    /* init predefined expression */
    yleset_type(&_predefined_true, YLEAtom | YLASymbol | YLEReachable);
    yleset_type(&_predefined_nil, YLEAtom | YLASymbol | YLEReachable);
    yleset_type(&_predefined_quote, YLEAtom | YLASymbol | YLEReachable);

    /* 
     * '_predefined_xxxx' SHOULD NOT be freed!!!! 
     * So, passing data pointer is OK
     */
    ylaassign_sym(&_predefined_true,   "t");
    ylaassign_sym(&_predefined_nil,    NULL);
    ylaassign_sym(&_predefined_quote,  "quote");

    /*
     * There predefined exp. should not be freed. So, it's initial reference count is 1.
     */
    ylercnt(&_predefined_true) = 1;
    ylercnt(&_predefined_nil) = 1;
    ylercnt(&_predefined_quote) = 1;

#define NFUNC(n, s, type, desc)                                         \
    if(YLOk != ylregister_nfunc(YLDEV_VERSION, s, (ylnfunc_t)YLNFN(n), type, ">> lib: ylisp <<\n" desc)) { goto bail; }
#include "nfunc.in"
#undef NFUNC



    return YLOk;

 bail:
    return YLErr_init; 
}

void
yldeinit() {
    yltrie_deinit();
    ylmp_deinit();
    if(_prbuf) { ylfree(_prbuf); }
}
