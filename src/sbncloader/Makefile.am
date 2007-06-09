bindir=$(exec_prefix)

bin_PROGRAMS=sbnc
sbnc_SOURCES=sbncloader.cpp Service.cpp Service.h
sbnc_LDADD=../libltdl/libltdlc.la ../rpc/librpc.la ../snprintf/libsnprintf.la
AM_CFLAGS=-DRPCSERVER
AM_CPPFLAGS=-DRPCSERVER

