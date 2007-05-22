bindir=$(exec_prefix)

bin_PROGRAMS=sbnc
sbnc_SOURCES=sbncloader.cpp AssocArray.cpp AssocArray.h Service.cpp Service.h
sbnc_LDADD=../libltdl/libltdlc.la ../rpc/librpc.la
AM_CFLAGS=-DRPCSERVER
AM_CPPFLAGS=-DRPCSERVER

../libltdl/libltdlc.la:
	make -C ../libltdl

../rpc/librpc.la:
	make -C ../rpc
