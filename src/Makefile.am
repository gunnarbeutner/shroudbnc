SUBDIRS=md5 adns

pkglibdir=$(exec_prefix)
bindir=$(exec_prefix)

pkglib_LTLIBRARIES=libsbnc.la
libsbnc_la_SOURCES=Banlist.cpp BouncerConfig.cpp BouncerCore.cpp BouncerLog.cpp BouncerUser.cpp Channel.cpp ClientConnection.cpp Connection.cpp Debug.cpp FIFOBuffer.cpp FloodControl.cpp IdentSupport.cpp IRCConnection.cpp Keyring.cpp Match.cpp Module.cpp Nick.cpp Queue.cpp sbnc.cpp snprintf.c Timer.cpp TrafficStats.cpp utility.cpp Banlist.h BouncerConfig.h BouncerCore.h BouncerLog.h BouncerUser.h Channel.h ClientConnection.h config.h Connection.h DnsEvents.h FIFOBuffer.h FloodControl.h Hashtable.h IdentSupport.h IRCConnection.h Keyring.h Listener.h Match.h ModuleFar.h Module.h Nick.h Queue.h sbnc.h snprintf.h SocketEvents.h StdAfx.h Timer.h TrafficStats.h unix.h utility.h Vector.h win32.h
libsbnc_la_LIBADD=adns/libadns.la md5/libmd5.la

bin_PROGRAMS=mkconfig
mkconfig_SOURCES=mkconfig.cpp BouncerConfig.cpp
mkconfig_LDADD=md5/libmd5.la
