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
#	include <direct.h>
#else
// TODO: FIX!
typedef struct { char __addr[16]; } sockaddr_in6;
#endif

#define mkdir _mkdir

#ifndef S_IRUSR
#define S_IRUSR 0
#endif
#ifndef S_IWUSR
#define S_IWUSR 0
#endif
#ifndef S_IXUSR
#define S_IXUSR 0
#endif
#ifndef S_IRGRP
#define S_IRGRP 0
#endif
#ifndef S_IROTH
#define S_IROTH 0
#endif

#if !defined(socklen_t)
typedef int socklen_t;
#endif

#undef GetClassName

#undef strcasecmp
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
void *DebugMalloc(size_t Size, const char *file, int line);
void DebugFree(void *p, const char *file, int line);
char *DebugStrDup(const char *p, const char *file, int line);
void *DebugReAlloc(void *p, size_t newsize, const char *file, int line);

#define malloc(x) DebugMalloc(x, __FILE__, __LINE__)
#define free(x) DebugFree(x, __FILE__, __LINE__)
#undef strdup
#define strdup(x) DebugStrDup(x, __FILE__, __LINE__)
#define realloc(x, y) DebugReAlloc(x, y, __FILE__, __LINE__)
#endif

#define MAXPATHLEN MAX_PATH
