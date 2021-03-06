#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([ylisp], [1.0], [yhcting77@gmail.com])
AC_CONFIG_SRCDIR([ylisp/yldynb.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall -Werror])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET

AC_GNU_SOURCE

# libtool is used for shared library
AC_PROG_LIBTOOL


# Checks for libraries.
AC_SEARCH_LIBS([dlopen],           [dl],
    [AC_DEFINE([HAVE_LIBDL],      [1],    [libdl supported?])],
    [AC_MSG_ERROR(['libdl' not found.])])
AC_SEARCH_LIBS([pthread_create],   [pthread],
    [AC_DEFINE([HAVE_LIBPTHREAD], [1],    [libpthread supported?])],
    [AC_MSG_ERROR(['libpthread' not foud.])])

#### Check dynamically loaded libraries
####   - AC_SEARCH_LIBS should NOT be used for this.
AC_CHECK_FILE([/usr/lib/libm.so],
    [AC_DEFINE([HAVE_LIBM],       [1],    [/usr/lib/libm.so exists?])]
    [have_m=true],
    [AC_MSG_NOTICE([math ext. functions aren't available!])])
AC_CHECK_FILE([/usr/local/lib/libpcre.so],
    [AC_DEFINE([HAVE_LIBPCRE],    [1],    [/usr/local/lib/libpcre.so exists?])]
    [have_pcre=true]
    [AC_MSG_NOTICE(['pcre' is used for regular expression.])],
    [AC_MSG_NOTICE([POSIX regex is used for regular expression.])])
AM_CONDITIONAL([COND_PCRE], [test "x${have_pcre}" = xtrue])
AC_CHECK_FILE([/usr/lib/coreutils/libstdbuf.so],
    [AC_DEFINE([HAVE_LIBSTDBUF],
               [1],
	       [/usr/lib/coreutils/libstdbuf.so exists?])],
    [AC_MSG_NOTICE(['procia_xxxx' ext. functions aren't available.])])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h malloc.h memory.h \
		  stddef.h stdlib.h string.h sys/time.h unistd.h],
    [],
    [AC_MSG_ERROR([sorry, can't do anything without these headers...])],
    [])

# Check shell to use
AC_CHECK_FILE([/bin/bash], [AC_DEFINE([HAVE_BASH], [1], [/bin/bash exists?])])

#### Check for JAVA support #####
# Default java include path
JAVAINC=/usr/lib/jvm/default-java/include/
AC_SUBST([JAVAINC])

# Check files for java!
AC_CHECK_FILE([${JAVAINC}jni.h], [have_jni_h=true])
AC_CHECK_PROG([have_javac], [javac], [true])
AC_SEARCH_LIBS([JNI_CreateJavaVM], [jvm],  [have_jvm=true])
AM_CONDITIONAL([COND_JAVA],
	       [test "x${have_jni_h}" = xtrue -a \
	          "x${have_javac}" = xtrue -a "x${have_jvm}" = xtrue])
AM_COND_IF([COND_JAVA], [], [AC_MSG_NOTICE(['yljfe' is NOT available.])])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_C_BIGENDIAN

# Check size of primitive types
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(void*)
if test ${ac_cv_sizeof_char} != 1; then
	AC_MSG_ERROR([Size of char isn't 8 bit.])
fi
if test ${ac_cv_sizeof_short} != 2; then
	AC_MSG_ERROR([Size of short isn't 16 bit.])
fi
if test ${ac_cv_sizeof_int} != 4; then
	AC_MSG_ERROR([Size of int isn't 32 bit.])
fi
# some codes assumes below condition.
if test ${ac_cv_sizeof_long} != ${ac_cv_sizeof_voidp}; then
        AC_MSG_ERROR([Size of long isn't same with size of pointer.])
fi

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([memmove memset dup2 getcwd putenv regcomp select \
		setenv strerror],
    [],
    [AC_MSG_ERROR([sorry, these function is required. \
    		   But portability will be improved later!])])

AC_ARG_WITH(debug,
[  --with-debug[[=<opt>]]
       use for debug ylisp
       <opt> can be empty, 'v'
           empty     : minimal debug
           v[[verbose]]: verbose debug
           f[[full]]   : full debug - not recommended],

[
    case "$withval" in
        '')
            AC_DEFINE([CONFIG_LOG],        [1], [log is enabled?])
            AC_DEFINE([CONFIG_ASSERT],     [1], [assert is enabled?])
            ;;
        v)
            AC_DEFINE([CONFIG_LOG],        [1], [log is enabled?])
            AC_DEFINE([CONFIG_ASSERT],     [1], [assert is enabled?])
            AC_DEFINE([CONFIG_DBG_GEN],
	              [1],
		      [general debugging msg is enabled?])
            AC_DEFINE([CONFIG_DBG_EVAL],
	              [1],
		      [debugging for eval. is enabled?])
            ;;
        f)
            AC_DEFINE([CONFIG_LOG],        [1], [log is enabled?])
            AC_DEFINE([CONFIG_ASSERT],     [1], [assert is enabled?])
            AC_DEFINE([CONFIG_DBG_GEN],
	              [1],
		      [general debugging msg is enabled?])
            AC_DEFINE([CONFIG_DBG_EVAL],
	              [1],
		      [debugging for eval. is enabled?])
            AC_DEFINE([CONFIG_DBG_MT],
	              [1],
		      [debugging for multithread is enabled?])
            AC_DEFINE([CONFIG_DBG_MEM],
	              [1],
		      [debugging for memory is enabled?])
            AC_DEFINE([CONFIG_DBG_MUTEX],
	              [1],
		      [debugging for mutex is enabled?])
            ;;
    esac
],
[])

AC_ARG_WITH(static,
[  --with-static
        make ylbase and ylbase as an static linkable libraries.
        load-cnf and unload cnf is not available.
        ylr, test and yljfe is generated as static-linked-executable.],
[AC_DEFINE([CONFIG_STATIC_CNF],
	   [1],
	   [plug-in 'cnf' is not used. static executable is generated.])]
[config_static=true],
[])
AM_CONDITIONAL([COND_STATIC], [test "x$config_static" = xtrue])

AM_LDFLAGS="${LDFLAGS}"
if test x${config_static} = xtrue; then
        if test x${have_m} = xtrue; then
                AM_LDFLAGS="${AM_LDFLAGS} -lm"
        fi
        if test x${have_pcre} = xtrue; then
                AM_LDFLAGS="${AM_LDFLAGS} -lpcre"
        fi
else
        AM_LDFLAGS="${AM_LDFLAGS} -rdynamic"
fi
# Common flags!!
# '-I..' is already in $INCLUDES
AM_CPPFLAGS="$INCLUDES -I../ylisp"
AM_CFLAGS="-Wall -Werror"
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_LDFLAGS])

AC_CONFIG_FILES([Makefile
                 ylisp/Makefile
                 ylbase/Makefile
                 ylext/Makefile
                 ylr/Makefile
                 yld/Makefile
                 test/Makefile
                 yljfe/Makefile
                 yls/Makefile])

AC_OUTPUT
