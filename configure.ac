#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(shroudbnc, 1.3, gunnar@beutner.name)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([config])

AM_INIT_AUTOMAKE(shroudbnc, 1.3)
AM_CONFIG_HEADER(config.h)

PACKAGE=sbnc
VERSION=1.3

AC_PREFIX_DEFAULT([\${HOME}/sbnc])
AC_CONFIG_SUBDIRS([third-party/c-ares])

# Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_CC_C_O

# Name the subdirectory that contains libltdl sources
LT_CONFIG_LTDL_DIR([third-party/libltdl])
     
# Configure libtool with dlopen support if possible
LT_INIT([dlopen])
     
# Enable building of the convenience libltdl library
LTDL_INIT([convenience])

AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([winsock.h arpa/inet.h arpa/nameser.h arpa/nameser_compat.h fcntl.h limits.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

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
AC_CHECK_FUNCS([dup2 gethostbyname gettimeofday inet_ntoa memchr memmove memset mkdir select socket strchr strcspn strdup strerror strstr strtoul poll])

AC_CHECK_FUNCS([snprintf], [builtin_snprintf=no], [builtin_snprintf=yes])
AM_CONDITIONAL([USE_BUILTIN_SNPRINTF], [test "$builtin_snprintf" = "yes"])

AC_CHECK_LIB(iphlpapi, main)
AC_CHECK_LIB(ws2_32, main)
AC_CHECK_LIB(shlwapi, main)
AC_CHECK_LIB(advapi32, main)
AC_CHECK_LIB(ssl, SSL_new)
AC_CHECK_LIB(crypto, X509_NAME_oneline)
AC_CHECK_LIB(eay32, X509_NAME_oneline)

# Check for IPv6

dnl This macro determines if the specified constant exists in the specified file
dnl Syntax:
dnl CARES_CHECK_CONSTANT(headers, constant name, if found, [if not found])
AC_DEFUN([CARES_CHECK_CONSTANT], [
  AC_MSG_CHECKING([for $2])
  AC_EGREP_CPP(VARIABLEWASDEFINED,
   [
      $1

      #ifdef $2
        VARIABLEWASDEFINED
      #else
        NJET
      #endif
    ], ac_constant="yes", ac_constant="no"
  )
  if test "$ac_constant" = "yes" ; then
    AC_MSG_RESULT(yes)
    $3
  else
    AC_MSG_RESULT(no)
    $4
  fi
])

dnl This macro determines if the specified struct exists in the specified file
dnl Syntax:
dnl CARES_CHECK_STRUCT(headers, struct name, if found, [if not found])

AC_DEFUN([CARES_CHECK_STRUCT], [
  AC_MSG_CHECKING([for struct $2])
  AC_TRY_COMPILE([$1], 
    [
      struct $2 struct_instance;
    ], ac_struct="yes", ac_found="no")
  if test "$ac_struct" = "yes" ; then
    AC_MSG_RESULT(yes)
    $3
  else
    AC_MSG_RESULT(no)
    $4
  fi
])

dnl check for PF_INET6
CARES_CHECK_CONSTANT(
  [
#undef inline
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#endif
  ], [AF_INET6],
     AC_DEFINE_UNQUOTED(HAVE_AF_INET6,1,[Define to 1 if you have AF_INET6.])
)

dnl check for the in6_addr structure
CARES_CHECK_STRUCT(
  [
#undef inline
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
#endif
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#endif
  ], [in6_addr],
     AC_DEFINE_UNQUOTED(HAVE_STRUCT_IN6_ADDR,1,[Define to 1 if you have struct in6_addr.])
)

dnl check for the sockaddr_in6 structure
CARES_CHECK_STRUCT(
  [
#undef inline
#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif
#endif
#else
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#endif
  ], [sockaddr_in6],
     AC_DEFINE_UNQUOTED(HAVE_STRUCT_SOCKADDR_IN6,1,
       [Define to 1 if you have struct sockaddr_in6.]) ac_have_sockaddr_in6=yes
)

#CFLAGS="$CFLAGS -Wall -Wshadow -Wconversion -Wcast-qual -Wwrite-strings"
#CXXFLAGS="$CXXFLAGS -Wall -Wshadow -Wconversion -Wcast-qual -Wwrite-strings"

AC_MSG_CHECKING(whether to enable debugging)
debug_default="yes"
AC_ARG_ENABLE(debug, [  --enable-debug=[no/yes] turn on debugging
			[default=$debug_default]],, enable_debug=$debug_default)
if test "x$enable_debug" = "xyes"; then
	CFLAGS="$CFLAGS -g"
	CXXFLAGS="$CXXFLAGS -g"
	AC_MSG_RESULT(yes)
else
	CFLAGS="$CFLAGS -O2"
	CXXFLAGS="$CFLAGS -O2"
	AC_MSG_RESULT(no)
fi


sinclude(./tcl.m4)
SC_PATH_TCLCONFIG

build_bnctcl=no

if test x"${no_tcl}" = x ; then
	SC_LOAD_TCLCONFIG

	if test \( "$TCL_MAJOR_VERSION" -lt 8 \) -o \( \( "$TCL_MAJOR_VERSION" -eq 8 \) -a \( "$TCL_MINOR_VERSION" -lt 3 \) \); then
		AC_MSG_WARN([You need at least TCL 8.3 for the tcl module.])
	else
		CFLAGS_TMP=$CFLAGS
		CFLAGS=$TCL_INCLUDE_SPEC
		AC_LANG_PUSH(C)
		AC_COMPILE_IFELSE(AC_LANG_PROGRAM([[#include <tcl.h>]], []), TCLINC=ok)
		AC_LANG_POP(C)
		CFLAGS=$CFLAGS_TMP
	
		if test x"${TCLINC}" != x ; then
			build_bnctcl=yes

			AC_SUBST(TCL_INCLUDE_SPEC)
			AC_SUBST(TCL_LIBS)
			AC_SUBST(TCL_LIB_SPEC)
		else
			AC_MSG_WARN([TCL header files could not be found.])
		fi
	fi
else
	AC_MSG_WARN([TCL is not installed. TCL module support disabled.])
fi

AM_CONDITIONAL([BUILD_BNCTCL_MODULE], [test "$build_bnctcl" = yes])

AC_COMPILE_IFELSE(AC_LANG_PROGRAM([[#include <openssl/ssl.h>]], []), [ build_with_ssl=yes ], [ build_with_ssl=no; AC_MSG_WARN([You need the OpenSSL headers for SSL support. SSL support has been disabled.]) ])
AM_CONDITIONAL([BUILD_WITH_SSL], [test "$build_with_ssl" = yes])

AC_COMPILE_IFELSE(AC_LANG_PROGRAM([[#include <ares.h>]], []), [ builtin_cares=no ], [ builtin_cares=yes ])
AM_CONDITIONAL([USE_BUILTIN_CARES], [test "$builtin_cares" = yes])

if test "$builtin_cares" = yes; then
	LIBCARES=../third-party/c-ares/libcares.la
	CARESINCL=-I../third-party/c-ares
else
	LIBCARES=-lcares
	CARESINCL=
fi

AC_SUBST([LIBCARES])
AC_SUBST([CARESINCL])

AC_CHECK_LIB(ws2_32, main, [ build_identd=yes; LIBS += -lws2_32 ], [ build_identd=no ])
AC_ARG_WITH(identd, enabled building the identd module (enabled by default if possible), [ build_identd=yes ])
AM_CONDITIONAL([BUILD_IDENTD_MODULE], [test "$build_identd" = yes])

AC_CONFIG_FILES([Makefile \
                 bnctcl/Makefile \
                 bnctcl/scripts/Makefile \
                 identd32/Makefile \
                 php/Makefile \
                 src/Makefile
                 third-party/Makefile \
                 third-party/md5/Makefile \
                 third-party/snprintf/Makefile])
AC_OUTPUT

if test "$build_with_ssl" = yes; then
	feat_ssl_msg="yes"
	openssl_msg="yes"
else
	feat_ssl_msg="no"
	openssl_msg="no"
fi

if test "$build_bnctcl" = yes; then
	feat_tcl_msg="yes"
	tcl_msg="yes"
else
	feat_tcl_msg="no"
	tcl_msg="no"
fi

if test -z "$LTDLDEPS"; then
	ltdl_msg="system-provided"
else
	ltdl_msg="bundled"
fi

if test "$builtin_cares" = yes; then
	cares_msg="bundled"
else
	cares_msg="system-provided"
fi

if test "$builtin_snprintf" = yes; then
	snprintf_msg="bundled"
else
	snprintf_msg="system-provided"
fi

echo "
Features:
     SSL:	$feat_ssl_msg
     TCL:	$feat_tcl_msg

Libraries:
     OpenSSL:	$openssl_msg
     TCL:	$tcl_msg
     ltdl:	$ltdl_msg
     c-ares:	$cares_msg
     snprintf:	$snprintf_msg
"
