# This is just for test... NOT for distribution!!!
# Helper executable & Main executable
noinst_PROGRAMS = __procia_test test
__procia_test_SOURCES = __procia_test.c
test_SOURCES = main.c
test_LDADD = ../ylisp/libylisp.a
if COND_STATIC
    test_LDADD+= ../ylbase/libylbase.la ../ylext/libylext.la
endif
