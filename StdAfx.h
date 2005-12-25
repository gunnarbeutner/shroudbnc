/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005 Gunnar Beutner                                           *
 *                                                                             *
 * This program is free software; you can redistribute it and/or               *
 * modify it under the terms of the GNU General Public License                 *
 * as published by the Free Software Foundation; either version 2              *
 * of the License, or (at your option) any later version.                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *******************************************************************************/

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES 1

// some newer visual c++ versions define time_t as a 64 bit integer
// while adns still expects a 32 bit integer
// -> sbnc crashes while accessing the adns_answer structure
#define _USE_32BIT_TIME_T
#endif

#ifdef _WIN32
	#include "win32.h"
#else
	#include "unix.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>

#ifdef USESSL
#include <openssl/ssl.h>
#endif

#include "snprintf.h"

#ifdef _WIN32
#ifndef RUBY
	#include <windows.h>
	#include <winsock2.h>
	#include <assert.h>
#endif
#else
	#include <dlfcn.h>
	#include <ctype.h>
	#include <string.h>
	#include <unistd.h>
	#include <netdb.h>
	#include <sys/time.h>
	#include <sys/types.h>
	#include <sys/ioctl.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <arpa/nameser.h>
	#include <errno.h>
	#include <sys/resource.h>
	#include <limits.h>
#endif

#ifndef SWIG
#ifdef _WIN32
	#ifndef NOADNSLIB
		#pragma comment(lib, "adns_win\\adns_win32\\lib\\adns_dll.lib")
	#endif

	#define ADNS_JGAA_WIN32

	#include "adns_win/src/adns.h"
#else
	#include "adns/adns.h"
#endif
#endif

#define sprintf __evil_function
#undef wsprintf
#define wsprintf __evil_function

#ifdef _WIN32
#ifdef _DEBUG
#ifdef SBNC

void* DebugMalloc(size_t Size);
void DebugFree(void* p);
char* DebugStrDup(const char* p);
void* DebugReAlloc(void* p, size_t newsize);
bool ReportMemory(time_t Now, void* Cookie);
int profilestrcmpi(const char*, const char*);

#define real_malloc malloc
#define real_free free
#define real_strdup strdup
#define real_realloc realloc
#define real_strcmpi strcmpi

#define malloc DebugMalloc
#define free DebugFree
#define strdup DebugStrDup
#define realloc DebugReAlloc
#define strcmpi profilestrcmpi

#endif
#endif
#endif

#include "utility.h"
#include "Vector.h"
#include "Hashtable.h"
#include "SocketEvents.h"
#include "DnsEvents.h"
#include "Timer.h"
#include "Connection.h"
#include "ClientConnection.h"
#include "IRCConnection.h"
#include "BouncerUser.h"
#include "BouncerCore.h"
#include "BouncerLog.h"
#include "BouncerConfig.h"
#include "ModuleFar.h"
#include "Module.h"
#include "Channel.h"
#include "Nick.h"
#include "Keyring.h"
#include "Banlist.h"
#include "IdentSupport.h"
#include "Match.h"
#include "TrafficStats.h"
#include "FIFOBuffer.h"
#include "Queue.h"
#include "FloodControl.h"
#include "Listener.h"

#include "sbncloader/AssocArray.h"
