SUBDIRS=md5 libltdl sbncloader c-ares

pkglibdir=$(exec_prefix)/${MOD_DIR}
bindir=$(exec_prefix)

pkglib_LTLIBRARIES=libsbnc.la
libsbnc_la_SOURCES=Banlist.cpp \
	Config.cpp \
	Core.cpp \
	Log.cpp \
	User.cpp \
	Channel.cpp \
	ClientConnection.cpp \
	Connection.cpp \
	Debug.cpp \
Debug.h \
	DnsEvents.cpp \
	FIFOBuffer.cpp \
	FloodControl.cpp \
	IdentSupport.cpp \
	IRCConnection.cpp \
	Keyring.cpp \
	Match.cpp \
	Module.cpp \
	Nick.cpp \
	Queue.cpp \
	sbnc.cpp \
	snprintf.c \
	Timer.cpp \
	TrafficStats.cpp \
	utility.cpp \
	Banlist.h \
	Config.h \
	Core.h \
	Log.h \
	User.h \
	Channel.h \
	ClientConnection.h \
	Connection.h \
	DnsEvents.h \
	FIFOBuffer.h \
	FloodControl.h \
	Hashtable.h \
	IdentSupport.h \
	IRCConnection.h \
	Keyring.h \
	Listener.h \
	Match.h \
	ModuleFar.h \
	Module.h \
	Nick.h \
	OwnedObject.h \
	Persistable.h \
	Queue.h \
	sbnc.h \
	snprintf.h \
	SocketEvents.h \
	StdAfx.h \
	Timer.h \
	TrafficStats.h \
	unix.h \
	utility.h \
	Vector.h \
	win32.h \
	Zone.h
libsbnc_la_LIBADD=c-ares/libcares.la md5/libmd5.la libltdl/libltdlc.la @SSLLDFLAGS@
libsbnc_la_LDFLAGS=-no-undefined
AM_CFLAGS=@SSLCFLAGS@
AM_CPPFLAGS=@SSLCFLAGS@

