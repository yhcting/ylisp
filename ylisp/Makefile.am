# STATIC LIBRARY
pkglib_LIBRARIES = libylisp.a
include_HEADERS = ylisp.h
libylisp_a_SOURCES = \
    yldef.h ylut.h yllist.h yltrie.h yldynb.h yldev.h ylsfunc.h \
    lisp.c sfunc.c mempool.c mthread.c parser.c interpret.c nfunc.c \
    nfunc_mt.c symlookup.c gsym.c trie.c ut.c

if !COND_STATIC
    # EXECUTABLE for debugging
    noinst_PROGRAMS = ylisp
    ylisp_SOURCES = testmain.c
    ylisp_LDADD = libylisp.a
endif