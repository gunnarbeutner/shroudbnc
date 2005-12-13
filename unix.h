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

// *nix specific things

typedef int SOCKET;

#define strcmpi strcasecmp

#define SD_BOTH SHUT_RDWR
#define closesocket close
#define wsprintf sprintf
#define INVALID_SOCKET (-1)
#define MAX_SOCKETS 1024
#define ioctlsocket ioctl

typedef void* HMODULE;
typedef int BOOL;

HMODULE LoadLibrary(const char* lpLibFileName);
BOOL FreeLibrary(HMODULE hLibModule);
void* GetProcAddress(HMODULE hModule, const char* lpProcName);

#ifdef __FreeBSD__
#define sighandler_t sig_t
#endif
