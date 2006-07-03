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

#include "../config.h"

#ifdef _MSC_VER
#pragma warning ( disable : 4996 )

#define _CRT_SECURE_NO_DEPRECATE
#undef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#define _USE_32BIT_TIME_T
#endif

#include "fdhelper.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <limits.h>

#ifdef __cplusplus
#	include <typeinfo>
#endif

#ifdef _WIN32
#	include "win32.h"
#else
#	include "unix.h"
#endif

#include "fdhelper.h"

#ifndef _MSC_VER
#	ifdef DLL_EXPORT
#		undef DLL_EXPORT
#		define WAS_DLL_EXPORT
#	endif
#	include "libltdl/ltdl.h"
#	ifdef WAS_DLL_EXPORT
#		define DLL_EXPORT
#	endif
#endif

#ifndef _WIN32
typedef lt_dlhandle HMODULE;
#endif

#ifdef USESSL
#	include <openssl/ssl.h>
#	include <openssl/err.h>
#else
	typedef void SSL;
	typedef void SSL_CTX;
	typedef void X509;
	typedef void X509_STORE_CTX;
#endif

#if defined(HAVE_AF_INET6) && defined(HAVE_STRUCT_IN6_ADDR) && defined(HAVE_STRUCT_SOCKADDR_IN6)
#	define IPV6
#endif

#include "snprintf.h"

#ifndef SWIG
#	include "c-ares/ares.h"
#endif

#ifdef SBNC
#	define nmalloc(Size) mmalloc(Size, NULL)
#	define nrealloc(Block, NewSize) mrealloc(Block, NewSize, NULL)
#	define nstrdup(String) mstrdup(String, NULL)
#	define nfree(Block) mfree(Block)
#	define nmark(Block) mmark(Block)

#	define umalloc(Size) mmalloc(Size, GETUSER())
#	define urealloc(Block, NewSize) mrealloc(Block, NewSize, GETUSER())
#	define ustrdup(String) mstrdup(String, GETUSER())
#	define ufree(Block) mfree(Block)
#	define umark(Block) mmark(Block)

void mmark(void *Block);
#endif

#if !defined(_DEBUG) || !defined(SBNC)
#	define mmark(Block)
#endif

#ifdef SBNC
#	define EXTRA_SECURITY

#	ifdef EXTRA_SECURITY
#		define strcpy(dest, src) __undefined_function
#		define strcat(dest, src) __undefined_function
#		define fscanf __undefined_function
#		define sprintf __undefined_function
#	endif
#endif

#ifdef __cplusplus
#	include "sbncloader/AssocArray.h"

#	include "sbnc.h"
#	include "Result.h"
#	include "Object.h"
#	include "Zone.h"
#	include "Vector.h"
#	include "List.h"
#	include "Hashtable.h"
#	include "utility.h"
#	include "SocketEvents.h"
#	include "DnsEvents.h"
#	include "Timer.h"
#	include "FIFOBuffer.h"
#	include "Queue.h"
#	include "Connection.h"
#	include "Config.h"
#	include "Cache.h"
#	include "Core.h"
#	include "ClientConnection.h"
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
#	include "Persistable.h"
#endif
