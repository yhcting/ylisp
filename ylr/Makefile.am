
bin_PROGRAMS = ylr
ylr_SOURCES = main.c
ylr_LDADD = ../ylisp/libylisp.a

if COND_STATIC
    ylr_LDADD+= ../ylbase/libylbase.la ../ylext/libylext.la
endif
