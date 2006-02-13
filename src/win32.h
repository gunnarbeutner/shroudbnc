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

// win32 specific header

#define FD_SETSIZE 4096

#ifndef RUBY
#	include <windows.h>
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	include <shlwapi.h>
#else
// TODO: FIX!
typedef struct { char __addr[16]; } sockaddr_in6;
#endif

#if !defined(socklen_t)
typedef int socklen_t;
#endif

#undef GetClassName

#define strcasecmp strcmpi

#define EXPORT __declspec(dllexport)

#ifndef _MSC_VER
#undef LoadLibrary
#define LoadLibrary(lpLibFileName) (HMODULE)lt_dlopen(lpLibFileName)
#undef FreeLibrary
#define FreeLibrary(hLibModule) hLibModule ? !lt_dlclose((lt_dlhandle)hLibModule) : 0
#undef GetProcAddress
#define GetProcAddress(hModule, lpProcName) lt_dlsym((lt_dlhandle)hModule, lpProcName)
#endif

#undef HAVE_AF_INET6
#define HAVE_AF_INET6
#undef HAVE_STRUCT_IN6_ADDR
#define HAVE_STRUCT_IN6_ADDR
#undef HAVE_STRUCT_SOCKADDR_IN6
#define HAVE_STRUCT_SOCKADDR_IN6

#if defined(_DEBUG) && defined(SBNC)
void *DebugMalloc(size_t Size);
void DebugFree(void *p);
char *DebugStrDup(const char *p);
void *DebugReAlloc(void *p, size_t newsize);
bool ReportMemory(time_t Now, void *Cookie);

#define real_malloc malloc
#define real_free free
#define real_strdup strdup
#define real_realloc realloc

#define malloc DebugMalloc
#define free DebugFree
#undef strdup
#define strdup DebugStrDup
#define realloc DebugReAlloc
#endif
