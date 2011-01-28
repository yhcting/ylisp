# SHARED LIBRARY
pkglib_LTLIBRARIES = libylbase.la
libylbase_la_SOURCES = libmain.c nfunc.c nfunc.in

# EXECUTABLE for debugging
noinst_PROGRAMS = ylbase
ylbase_SOURCES = testmain.c
ylbase_LDADD = ../ylisp/libylisp.a libylbase.la
