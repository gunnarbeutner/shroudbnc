#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(shroudbnc, 1.1, gb@prco23.org)

AM_INIT_AUTOMAKE(shroudbnc, 1.1)
AM_CONFIG_HEADER(config.h)

AC_PROG_MAKE_SET

PACKAGE=sbnc
VERSION=1.1

AC_PREFIX_DEFAULT([\${HOME}/sbnc])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB

AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([dup2 gethostbyname gettimeofday inet_ntoa memchr memmove memset mkdir select socket strchr strcspn strdup strerror strstr strtoul])
AC_LIB_LTDL

AC_CHECK_LIB(dl, dlopen)

sinclude(./tcl.m4)
sinclude(./src/libltdl/acinclude.m4)

SC_PATH_TCLCONFIG

BNCTCL_MODULE=

if test x"${no_tcl}" = x ; then
	SC_LOAD_TCLCONFIG

	if test \( "$TCL_MAJOR_VERSION" -lt 8 \) -o \( \( "$TCL_MAJOR_VERSION" -eq 8 \) -a \( "$TCL_MINOR_VERSION" -lt 4 \) \); then
		AC_MSG_WARN([You need at least TCL 8.4 for the tcl module.])
	else
		CFLAGS_TMP=$CFLAGS
		CFLAGS=$TCL_INCLUDE_SPEC
		AC_LANG_PUSH(C)
		AC_COMPILE_IFELSE(AC_LANG_PROGRAM([[#include <tcl.h>]], []), TCLINC=ok)
		AC_LANG_POP(C)
		CFLAGS=$CFLAGS_TMP
	
		if test x"${TCLINC}" != x ; then
			BNCTCL_MODULE=bnctcl
		else
			AC_MSG_WARN([TCL header files could not be found.])
		fi
	fi
else
	AC_MSG_WARN([TCL is not installed. TCL module support disabled.])
fi

SSLCFLAGS=
AC_COMPILE_IFELSE(AC_LANG_PROGRAM([[#include <openssl/ssl.h>]], []), [ USESSL=yes ], [ USESSL=no; AC_MSG_WARN([You need the OpenSSL headers for SSL support. SSL support has been disabled.]) ])

if test ${USESSL} = yes; then
	AC_CHECK_HEADERS([openssl/ssl.h])
	AC_CHECK_LIB(ssl, SSL_new, [ SSLCFLAGS=-DUSESSL ], AC_MSG_WARN([You need the OpenSSL libraries for SSL support. SSL support has been disabled.]))

	if test x"${SSLCFLAGS}" != x; then
		SSLLDFLAGS=-lssl
	fi
fi

AC_SUBST(TCL_INCLUDE_SPEC)
AC_SUBST(TCL_LIBS)
AC_SUBST(TCL_LIB_SPEC)
AC_SUBST(BNCTCL_MODULE)
AC_SUBST(SSLCFLAGS)
AC_SUBST(SSLLDFLAGS)

AM_CONDITIONAL(INSTALL_LTDL, test "x" = "y")
AM_CONDITIONAL(CONVENIENCE_LTDL, test "x" = "x")

AC_CONFIG_FILES([Makefile
                 src/libltdl/Makefile
                 bnctcl/Makefile
                 src/sbncloader/Makefile
                 src/Makefile
                 src/adns/Makefile
		 src/md5/Makefile])
AC_OUTPUT
