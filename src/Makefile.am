SUBDIRS=md5 libltdl sbncloader c-ares

pkglibdir=$(exec_prefix)/${MOD_DIR}
bindir=$(exec_prefix)

pkglib_LTLIBRARIES=libsbnc.la
libsbnc_la_SOURCES=Banlist.cpp \
	Cache.cpp \
	Config.cpp \
	ConfigModule.cpp \
	Core.cpp \
	Log.cpp \
	User.cpp \
	Channel.cpp \
	ClientConnection.cpp \
	ClientConnectionMultiplexer.cpp \
	Connection.cpp \
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
	RPCApi.cpp \
	SafeAPI.cpp \
	sbnc.cpp \
	snprintf.c \
	sslhelper.c \
	Timer.cpp \
	TrafficStats.cpp \
	utility.cpp \
	Banlist.h \
	Config.h \
	ConfigModule.h \
	Core.h \
	Log.h \
	User.h \
	Cache.h \
	Channel.h \
	ClientConnection.h \
	ClientConnectionMultiplexer.h \
	Connection.h \
	DnsEvents.h \
	FIFOBuffer.h \
	FloodControl.h \
	Hashtable.h \
	IdentSupport.h \
	IRCConnection.h \
	Keyring.h \
	List.h \
	Listener.h \
	Match.h \
	ModuleFar.h \
	Module.h \
	Nick.h \
	Object.h \
	Persistable.h \
	Result.h \
	Queue.h \
	RPCApi.h \
	SafeAPI.h \
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
	Zone.h \
	fdhelper.h
libsbnc_la_LIBADD=c-ares/libcares.la md5/libmd5.la libltdl/libltdlc.la @SSLLDFLAGS@
libsbnc_la_LDFLAGS=-no-undefined
AM_CFLAGS=-DSBNC @SSLCFLAGS@
AM_CPPFLAGS=-DSBNC @SSLCFLAGS@

