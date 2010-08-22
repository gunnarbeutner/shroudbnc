/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

#include "../config.h"

#ifdef _MSC_VER
#pragma warning ( disable : 4996 )

#define _CRT_SECURE_NO_DEPRECATE
#undef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif /* _CRT_SECURE_NO_DEPRECATE */

#undef SFD_SETSIZE
#define SFD_SETSIZE 16384

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

#ifdef __cplusplus
#	include <algorithm>
#	include <typeinfo>

using std::min;
using std::max;
#endif /* __cplusplus */

#ifdef _WIN32
#	include "win32.h"
#else /* _WIN32 */
#	include "unix.h"
#endif /* _WIN32 */

#ifndef _MSC_VER
#	ifdef DLL_EXPORT
#		undef DLL_EXPORT
#		define WAS_DLL_EXPORT
#	endif /* DLL_EXPORT */
#	include "ltdl.h"
#	ifdef WAS_DLL_EXPORT
#		define DLL_EXPORT
#	endif /* WAS_DLL_EXPORT */
#endif /* _MSC_VER */

#ifndef _WIN32
typedef lt_dlhandle HMODULE;
#endif /* _WIN32 */

#ifdef USESSL
#	include <openssl/bio.h>
#	include <openssl/ssl.h>
#	include <openssl/md5.h>
#	include <openssl/err.h>
#else /* USESSL */
	typedef void SSL;
	typedef void BIO;
	typedef void SSL_CTX;
	typedef void X509;
	typedef void X509_STORE_CTX;
#endif /* USESSL */

#if defined(HAVE_AF_INET6) && defined(HAVE_STRUCT_IN6_ADDR) && defined(HAVE_STRUCT_SOCKADDR_IN6)
#	define IPV6
#endif /* defined(HAVE_AF_INET6) && defined(HAVE_STRUCT_IN6_ADDR) && defined(HAVE_STRUCT_SOCKADDR_IN6) */

#ifndef HAVE_SNPRINTF
#	include "../third-party/snprintf/snprintf.h"
#endif

#ifndef SWIG
#	include <ares.h>
#endif /* SWIG */

#ifdef SBNC
#	define EXTRA_SECURITY

#	ifdef EXTRA_SECURITY
#		define strcpy(dest, src) __undefined_function
#		define strcat(dest, src) __undefined_function
#		define fscanf __undefined_function
#		define sprintf __undefined_function
#	endif /* EXTRA_SECURITY */
#endif /* SBNC */

#ifdef __cplusplus
#	include "sbnc.h"
#	include "Result.h"
#	include "Object.h"
#	include "Vector.h"
#	include "List.h"
#	include "Hashtable.h"
#	include "utility.h"
#	include "SocketEvents.h"
#	include "DnsSocket.h"
#	include "DnsEvents.h"
#	include "Timer.h"
#	include "FIFOBuffer.h"
#	include "Queue.h"
#	include "Connection.h"
#	include "Config.h"
#	include "Cache.h"
#	include "Core.h"
#	include "ClientConnection.h"
#	include "ClientConnectionMultiplexer.h"
#	include "IRCConnection.h"
#	include "User.h"
#	include "Log.h"
#	include "ModuleFar.h"
#	include "Module.h"
#	include "Banlist.h"
#	include "Channel.h"
#	include "Nick.h"
#	include "Keyring.h"
#	include "IdentSupport.h"
#	include "Match.h"
#	include "TrafficStats.h"
#	include "FloodControl.h"
#	include "Listener.h"
#endif /* __cplusplus */
