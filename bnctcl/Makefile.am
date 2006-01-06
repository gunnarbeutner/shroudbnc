pkglibdir=$(exec_prefix)

pkglib_LTLIBRARIES=libbnctcl.la
libbnctcl_la_SOURCES=itoa.c TclClientSocket.cpp TclSocket.cpp tickle.cpp tickleProcs.cpp tickleProcs_wrap.c itoa.h  StdAfx.h  TclClientSocket.h  TclSocket.h  tickle.h  tickleProcs.h
libbnctcl_la_LIBADD=@TCL_LIBS@ @TCL_LIB_SPEC@
libbnctcl_la_CFLAGS=@TCL_INCLUDE_SPEC@

AM_CXXFLAGS=@TCL_INCLUDE_SPEC@
AM_CFLAGS=@TCL_INCLUDE_SPEC@
CC=@CXX@
