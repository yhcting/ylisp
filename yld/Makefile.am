
bin_PROGRAMS = yld
yld_SOURCES = main.c cmd.c pdu.c socket.c
yld_LDADD = ../ylisp/libylisp.a

if COND_STATIC
    yld_LDADD+= ../ylbase/libylbase.la ../ylext/libylext.la
endif
