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
typedef class CHashtable<command_t *, false, 16> *commandlist_t;

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

SOCKET SocketAndConnect(const char *Host, unsigned short Port, const char *BindIp = NULL);
SOCKET SocketAndConnectResolved(const sockaddr *Host, const sockaddr *BindIp);

SOCKET CreateListener(unsigned short Port, const char *BindIp = NULL, int Family = AF_INET);

char *NickFromHostmask(const char *Hostmask);

const char *UtilMd5(const char *String);

void DestroyString(char *String);

void FlushCommands(commandlist_t *Commands);
void AddCommand(commandlist_t *Commands, const char *Name, const char *Category, const char *Description, const char *HelpText);
void DeleteCommand(commandlist_t *Commands, const char *Name);
int CmpCommandT(const void *pA, const void *pB);

#define BNCVERSION "1.2 $Revision$"
#define INTERFACEVERSION 24

extern const char *g_ErrorFile;
extern unsigned int g_ErrorLine;

#define LOGERROR g_Bouncer->InternalSetFileAndLine(__FILE__, __LINE__); g_Bouncer->InternalLogError

void StrTrim(char *String);

char *strmcpy(char *Destination, const char *Source, size_t Size);
char *strmcat(char *Destination, const char *Source, size_t Size);

#ifndef min
#define min(a, b) ((a)<(b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a)<(b) ? (b) : (a))
#endif

#ifdef IPV6
#define SOCKADDR_LEN(Family) ((Family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6))
#define INADDR_LEN(Family) ((Family == AF_INET) ? sizeof(in_addr) : sizeof(in6_addr))
#define MAX_SOCKADDR_LEN (max(sizeof(sockaddr_in), sizeof(sockaddr_in6)))
#else
#define SOCKADDR_LEN(Family) (sizeof(sockaddr_in))
#define INADDR_LEN(Family) (sizeof(sockaddr_in6))
#define MAX_SOCKADDR_LEN (sizeof(sockaddr_in))
#endif

const char *IpToString(sockaddr *Address);
int CompareAddress(const sockaddr *pA, const sockaddr *pB);

int SetPermissions(const char *Filename, int Modes);
bool RegisterZone(CZoneInformation *ZoneInformation);

void FreeString(char *String);
void FreeUString(char *String);

typedef struct mmanager_s {
	CUser *RealManager;
	unsigned int ReferenceCount;
} mmanager_t;

typedef struct {
	size_t Size;
	mmanager_t *Manager;
#if defined(_DEBUG) && defined(_WIN32)
	int Marker;
#endif
} mblock;

#define BLOCKMARKER 0x7B3D33B7

void *mmalloc(size_t Size, CUser *Manager);
void *mrealloc(void *Block, size_t NewSize, CUser *Manager);
char *mstrdup(const char *String, CUser *Manager);
void mfree(void *Block);

#define GETUSER() ((typeid(this) == typeid(CUser *)) ? (CUser *)this : dynamic_cast<CObjectBase *>(this)->GetUser())

#if defined(_DEBUG) && defined(_WIN32)
LONG WINAPI GuardPageHandler(EXCEPTION_POINTERS *Exception);
#endif
