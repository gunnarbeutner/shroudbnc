pkglib_LTLIBRARIES=libbncidentd.la
libbncidentd_la_SOURCES=bncidentd.cpp
libbncidentd_la_LIBADD=${LIBLTDL} ${LIBSNPRINTF} ../third-party/mmatch/libmmatch.la

AM_CXXFLAGS=@TCL_INCLUDE_SPEC@ ${LTDLINCL} ${SNPRINTFINCL} ${CARESINCL} -I../third-party/mmatch
AM_CFLAGS=@TCL_INCLUDE_SPEC@ ${LTDLINCL} ${SNPRINTFINCL} ${CARESINCL} -I../third-party/mmatch
AM_LIBTOOLFLAGS=--silent

CC=@CXX@
