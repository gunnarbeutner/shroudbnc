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

#ifndef UTILITY_H
#define UTILITY_H

/**
 * command_t
 *
 * A command.
 */
typedef struct command_s {
	char *Category; /**< the command's category */
	char *Description; /**< a short description of the command */
	char *HelpText; /**< the command's help text */
} command_t;

/** A list of commands. */
typedef class CHashtable<command_t *, false> *commandlist_t;

/**
 * utility_t
 *
 * Useful utility functions.
 */
typedef struct utility_s {
	const char *(*ArgParseServerLine)(const char *Data);
	const char *(*ArgTokenize)(const char *Data);
	const char **(*ArgToArray)(const char *Args);
	void (*ArgRejoinArray)(const char **ArgV, int Index);
	const char **(*ArgDupArray)(const char **ArgV);
	void (*ArgFree)(const char *Args);
	void (*ArgFreeArray)(const char **Array);
	const char *(*ArgGet)(const char *Args, int Arg);
	int (*ArgCount)(const char *Args);

	void (*FlushCommands)(commandlist_t *Commands);
	void (*AddCommand)(commandlist_t *Commands, const char *Name, const char *Category, const char *Description, const char *HelpText);
	void (*DeleteCommand)(commandlist_t *Commands, const char *Name);
	int (*CmpCommandT)(const void *pA, const void *pB);

	int (*asprintf)(char **ptr, const char *fmt, ...);

	void (*Free)(void *Pointer);
	void *(*Alloc)(size_t Size);

	const char *(*IpToString)(sockaddr *Address);
	bool (*StringToIp)(const char *IP, int Family, sockaddr *SockAddr, socklen_t Length);
	const sockaddr *(*HostEntToSockAddr)(hostent *HostEnt);
} utility_t;

const char *ArgParseServerLine(const char *Data);
const char *ArgTokenize(const char *Data);
const char **ArgToArray(const char *Args);
void ArgRejoinArray(const char **ArgV, int Index);
const char **ArgDupArray(const char **ArgV);
void ArgFree(const char *Args);
void ArgFreeArray(const char **Array);
const char *ArgGet(const char *Args, int Arg);
int ArgCount(const char *Args);

/**
 * tokendata_t
 *
 * Used for storing tokenized strings.
 */
typedef struct tokendata_s {
	unsigned int Count; /**< number of tokens */
	size_t Pointers[32]; /**< relative pointers to individual tokens */
	char String[512]; /**< the tokenized string */
} tokendata_t;

/* Version 2 of some tokenization functions
 * these functions have some limitations:
 * -only up to 32 tokens per string are supported
 * -strings cannot be longer than 512 chars
 */
tokendata_t ArgTokenize2(const char *String);
const char **ArgToArray2(const tokendata_t& Tokens);
const char *ArgGet2(const tokendata_t& Tokens, unsigned int Arg);
unsigned int ArgCount2(const tokendata_t& Tokens);

SOCKET SocketAndConnect(const char *Host, unsigned int Port, const char *BindIp = NULL);
SOCKET SocketAndConnectResolved(const sockaddr *Host, const sockaddr *BindIp);

SOCKET CreateListener(unsigned int Port, const char *BindIp = NULL, int Family = AF_INET);

char *NickFromHostmask(const char *Hostmask);

const char *UtilMd5(const char *String, const char *Salt, bool BrokenAlgo = false);
const char *GenerateSalt(void);
const char *SaltFromHash(const char *Hash);

void DestroyString(char *String);

void FlushCommands(commandlist_t *Commands);
void AddCommand(commandlist_t *Commands, const char *Name, const char *Category, const char *Description, const char *HelpText);
void DeleteCommand(commandlist_t *Commands, const char *Name);
int CmpCommandT(const void *pA, const void *pB);

#define BNCVERSION "1.3alpha24 $Revision$"
#define INTERFACEVERSION 25

extern const char *g_ErrorFile;
extern unsigned int g_ErrorLine;

void StrTrim(char *String);

char *strmcpy(char *Destination, const char *Source, size_t Size);
char *strmcat(char *Destination, const char *Source, size_t Size);

#ifdef HAVE_IPV6
#define SOCKADDR_LEN(Family) ((Family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6))
#define INADDR_LEN(Family) ((Family == AF_INET) ? sizeof(in_addr) : sizeof(in6_addr))
#else /* HAVE_IPV6 */
#define SOCKADDR_LEN(Family) (sizeof(sockaddr_in))
#define INADDR_LEN(Family) (sizeof(in_addr))
#endif /* HAVE_IPV6 */

const char *IpToString(sockaddr *Address);
bool StringToIp(const char *IP, int Family, sockaddr *SockAddr, socklen_t Length);
int CompareAddress(const sockaddr *pA, const sockaddr *pB);
const sockaddr *HostEntToSockAddr(hostent *HostEnt);

int SetPermissions(const char *Filename, int Modes);

void FreeString(char *String);

extern "C" BIO *BIO_new_socket(SOCKET Socket, int CloseFlag);
void SSL_CTX_set_passwd_cb(SSL_CTX *Context);

#if defined(_DEBUG) && defined(_WIN32)
LONG WINAPI GuardPageHandler(EXCEPTION_POINTERS *Exception);
#endif /* defined(_DEBUG) && defined(_WIN32) */

#ifdef HAVE_POLL
#	include <sys/poll.h>
#else /* HAVE_POLL */
struct pollfd {
	int fd;
	short events;
	short revents;
};

#define POLLIN 001
#define POLLPRI 002
#define POLLOUT 004
#define POLLNORM POLLIN
#define POLLERR 010
#define POLLHUP 020
#define POLLNVAL 040

int poll(struct pollfd *fds, unsigned long nfds, int timo);
#endif /* HAVE_POLL */

int sn_getline(char *buf, size_t size);
int sn_getline_passwd(char *buf, size_t size);

bool RcFailedInternal(int ReturnCode, const char *File, int Line);
bool AllocFailedInternal(const void *Ptr, const char *File, int Line);

/**
 * RcFailed
 *
 * Checks whether the specified return code signifies
 * a failed function call (rc < 0).
 *
 * @param RC the return code
 */
#define RcFailed(RC) RcFailedInternal(RC, __FILE__, __LINE__)

/**
 * AllocFailed
 *
 * Checks whether the result of an allocation function
 * is NULL.
 *
 * @param Variable the variable holding the result
 */
#define AllocFailed(Variable) AllocFailedInternal(Variable, __FILE__, __LINE__)

#ifndef _WIN32
lt_dlhandle sbncLoadLibrary(const char *Filename);
#endif

#endif /* UTILITY_H */
