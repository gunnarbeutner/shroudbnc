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

// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__9D162711_E45F_4BE4_A821_2CABE6E60ED8__INCLUDED_)
#define AFX_STDAFX_H__9D162711_E45F_4BE4_A821_2CABE6E60ED8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>

#include "snprintf.h"

#ifdef _WIN32
	#include <windows.h>
	#include <winsock2.h>
	#include <assert.h>

	#include "win32.h"
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

	#include "unix.h"
#endif

#define ASYNC_DNS

#ifdef ASYNC_DNS
	#ifdef _WIN32
		#pragma comment(lib, "adns\\adns_win32\\lib\\adns_dll.lib")

		#define ADNS_JGAA_WIN32
	#endif

	#include <adns.h>
#endif

#define sprintf __evil_function
#undef wsprintf
#define wsprintf __evil_function

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__9D162711_E45F_4BE4_A821_2CABE6E60ED8__INCLUDED_)
