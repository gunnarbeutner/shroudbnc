pkglibdir=$(exec_prefix)/${MOD_DIR}/
EXTRA_DIST=commands.txt internalbinds.txt

pkglib_LTLIBRARIES=libbnctcl.la
libbnctcl_la_SOURCES=TclClientSocket.cpp TclSocket.cpp tickle.cpp tickleProcs.cpp tickleProcs_wrap.c StdAfx.h TclClientSocket.h TclSocket.h tickle.h tickleProcs.h
libbnctcl_la_LIBADD=@TCL_LIBS@ @TCL_LIB_SPEC@ ../src/libsbnc.la
libbnctcl_la_CFLAGS=@TCL_INCLUDE_SPEC@
libbnctcl_la_LDFLAGS=-version-info 0:0:0 -no-undefined

AM_CXXFLAGS=@TCL_INCLUDE_SPEC@
AM_CFLAGS=@TCL_INCLUDE_SPEC@
CC=@CXX@
