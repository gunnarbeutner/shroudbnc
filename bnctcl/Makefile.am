SUBDIRS=scripts

EXTRA_DIST=commands.txt internalbinds.txt tickleProcs_wrap.c

pkglib_LTLIBRARIES=libbnctcl.la
libbnctcl_la_SOURCES=TclClientSocket.cpp TclSocket.cpp tickle.cpp tickleProcs.cpp tickleProcs_wrap.c StdAfx.h TclClientSocket.h TclSocket.h tickle.h tickleProcs.h
libbnctcl_la_LIBADD=@TCL_LIBS@ @TCL_LIB_SPEC@ ${LIBLTDL}
libbnctcl_la_CFLAGS=@TCL_INCLUDE_SPEC@
libbnctcl_la_LDFLAGS=-version-info 0:0:0

AM_CXXFLAGS=@TCL_INCLUDE_SPEC@ ${LTDLINCL}
AM_CFLAGS=@TCL_INCLUDE_SPEC@ ${LTDLINCL}
AM_LIBTOOLFLAGS=--silent
CC=@CXX@

tickleProcs_wrap.c: tickleProcs.h
	swig -tcl -nosafe -pkgversion 1.3 tickleProcs.h
