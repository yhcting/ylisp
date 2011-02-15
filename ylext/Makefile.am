# SHARED LIBRARY
pkglib_LTLIBRARIES = libylext.la
libylext_la_SOURCES = \
    libmain.c crc.c crc.h hash.h hash.c nfunc_arr.c nfunc_bin.c \
    nfunc.in nfunc_map.c nfunc_math.c nfunc_re.c nfunc_str.c nfunc_sys.c

# EXECUTABLE for debugging
noinst_PROGRAMS = ylext
ylext_SOURCES = testmain.c
# Let's assume that following libraries exists...
# This is only for debugging. So, don't worry about compatibility!
AM_LDFLAGS+= -lm
if COND_PCRE
    AM_LDFLAGS+= -lpcre
endif
ylext_LDADD = ../ylisp/libylisp.a libylext.la
