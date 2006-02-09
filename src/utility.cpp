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

#undef USESSL
#include "StdAfx.h"

extern "C" {
	#include "md5/global.h"
	#include "md5/md5.h"
}

/**
 * ArgTokenize
 *
 * Tokenizes a string (i.e. splits it into arguments). ArgFree must
 * eventually be called on the returned string.
 *
 * @param Data the string
 */
const char *ArgTokenize(const char *Data) {
	char* Copy;
	unsigned int LengthData;

	if (Data == NULL) {
		return NULL;
	}

	LengthData = strlen(Data);

	Copy = (char *)malloc(LengthData + 2);

	if (Copy == NULL) {
		LOGERROR("malloc() failed. Could not tokenize string (%s).", Data);

		return NULL;
	}

	strcpy(Copy, Data);
	Copy[LengthData + 1] = '\0';

	for (unsigned int i = 0; i < LengthData; i++) {
		if (Copy[i] == ' ' && Copy[i + 1] != ' ') {
			Copy[i] = '\0';

			if (i > 0 && Copy[i + 1] == ':') {
				break;
			}
		}
	}

	return Copy;
}

/**
 * ArgParseServerLine
 *
 * Tokenizes a line which was read from an irc server.
 *
 * @param Data the line
 */
const char *ArgParseServerLine(const char *Data) {
	if (Data != NULL && Data[0] == ':') {
		Data++;
	}

	return ArgTokenize(Data);
}

/**
 * ArgGet
 *
 * Retrieves an argument from a tokenized string.
 *
 * @param Args the tokenized string
 * @param Arg the index of the argument which is to be returned
 */
const char *ArgGet(const char *Args, int Arg) {
	for (int i = 0; i < Arg - 1; i++) {
		Args += strlen(Args) + 1;

		if (Args[0] == '\0') {
			return NULL;
		}
	}

	if (Args[0] == ':') {
		Args++;
	}

	return Args;
}

/**
 * ArgCount
 *
 * Returns the number of arguments in a tokenized string.
 *
 * @param Args the tokenized string
 */
int ArgCount(const char *Args) {
	int Count = 0;

	if (Args == NULL) {
		return 0;
	}

	while (true) {
		Args += strlen(Args) + 1;
		Count++;

		if (strlen(Args) == 0) {
			return Count;
		}
	}
}

/**
 * ArgToArray
 *
 * Converts a tokenized string into an array. ArgFreeArray must
 * eventually be called on the returned string.
 *
 * @param Args the tokenized string.
 */
const char **ArgToArray(const char *Args) {
	int Count = ArgCount(Args);

	const char **ArgArray = (const char **)malloc(Count * sizeof(const char *));

	if (ArgArray == NULL) {
		return NULL;
	}

	for (int i = 0; i < Count; i++) {
		ArgArray[i] = ArgGet(Args, i + 1);
	}

	return ArgArray;
}

/**
 * ArgRejoinArray
 *
 * "Rejoins" arguments in an array.
 *
 * @param ArgV the array
 * @param Index the index of the first argument which is to be rejoined
 */
void ArgRejoinArray(const char **ArgV, int Index) {
	int Count = ArgCount(ArgV[0]);

	if (Count - 1 <= Index) {
		return;
	}

	for (int i = Index + 1; i < Count; i++) {
		char *Arg = const_cast<char *>(ArgV[i]);

		*(Arg - 1) = ' ';
	}
}

/**
 * ArgDupArray
 *
 * Duplicates an array of arguments.
 *
 * @param ArgV the array
 */
const char **ArgDupArray(const char **ArgV) {
	char **Dup;
	int Len = 0;
	int Offset;
	int Count = ArgCount(ArgV[0]);

	for (int i = 0; i < Count; i++)
		Len += strlen(ArgV[i]) + 1;

	Dup = (char **)malloc(Count * sizeof(char *) + Len + 2);

	Offset = (char *)Dup + Count * sizeof(char *) - ArgV[0];

	memcpy(Dup, ArgV, Count * sizeof(char *));
	memcpy((char *)Dup + Count * sizeof(char *), ArgV[0], Len + 2);

	for (int i = 0; i < Count; i++)
		Dup[i] += Offset;

	return (const char **)Dup;
}

/**
 * ArgFree
 *
 * Frees a tokenized string.
 *
 * @params Args the tokenized string
 */
void ArgFree(const char *Args) {
	free(const_cast<char *>(Args));
}

/**
 * ArgFreeArray
 *
 * Frees an array of arguments.
 *
 * @param Array the array
 */
void ArgFreeArray(const char **Array) {
	free(const_cast<char **>(Array));
}

/**
 * SocketAndConnect
 *
 * Creates a socket and connects to the specified host/port. You should use
 * SocketAndConnectResolved instead wherever possible as this function uses
 * blocking DNS requests.
 *
 * @param Host the host
 * @param Port the port
 * @param BindIp the ip address/hostname which should be used for binding the socket
 */
SOCKET SocketAndConnect(const char* Host, unsigned short Port, const char* BindIp) {
	unsigned long lTrue = 1;
	sockaddr_in sin, sloc;
	SOCKET Socket;
	hostent *hent;
	unsigned long addr;
	int code;

	if (!Host || !Port)
		return INVALID_SOCKET;

	Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (Socket == INVALID_SOCKET)
		return INVALID_SOCKET;

	ioctlsocket(Socket, FIONBIO, &lTrue);

	if (BindIp && *BindIp) {
		sloc.sin_family = AF_INET;
		sloc.sin_port = 0;

		hent = gethostbyname(BindIp);

		if (hent) {
			in_addr* peer = (in_addr*)hent->h_addr_list[0];

			sloc.sin_addr.s_addr = peer->s_addr;
		} else {
			addr = inet_addr(BindIp);

			sloc.sin_addr.s_addr = addr;
		}

		bind(Socket, (sockaddr *)&sloc, sizeof(sloc));
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(Port);

	hent = gethostbyname(Host);

	if (hent) {
		in_addr* peer = (in_addr*)hent->h_addr_list[0];

		sin.sin_addr.s_addr = peer->s_addr;
	} else {
		addr = inet_addr(Host);

		sin.sin_addr.s_addr = addr;
	}

	code = connect(Socket, (const sockaddr *)&sin, sizeof(sin));

#ifdef _WIN32
	if (code != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
#else
	if (code != 0 && errno != EINPROGRESS) {
#endif
		closesocket(Socket);

		return INVALID_SOCKET;
	}

	return Socket;
}

/**
 * SocketAndConnectResolved
 *
 * Creates a socket and connects to the specified host/port. 
 *
 * @param Host the host's ip address
 * @param BindIp the ip address which should be used for binding the socket
 */
SOCKET SocketAndConnectResolved(const sockaddr *Host, const sockaddr* BindIp) {
	unsigned long lTrue = 1;
	int Code, Size;

	SOCKET Socket = socket(Host->sa_family, SOCK_STREAM, IPPROTO_TCP);

	if (Socket == INVALID_SOCKET)
		return INVALID_SOCKET;

	ioctlsocket(Socket, FIONBIO, &lTrue);

	if (BindIp) {
#ifdef IPV6
		if (BindIp->sa_family == AF_INET) {
#endif
			Size = sizeof(sockaddr_in);
#ifdef IPV6
		} else {
			Size = sizeof(sockaddr_in6);
		}
#endif

		bind(Socket, (sockaddr *)&BindIp, Size);
	}

#ifdef IPV6
	if (Host->sa_family == AF_INET) {
#endif
		Size = sizeof(sockaddr_in);
#ifdef IPV6
	} else {
		Size = sizeof(sockaddr_in6);
	}
#endif

	Code = connect(Socket, Host, Size);

#ifdef _WIN32
	if (Code != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
#else
	if (Code != 0 && errno != EINPROGRESS) {
#endif
		closesocket(Socket);

		return INVALID_SOCKET;
	}

	return Socket;
}

/**
 * NickFromHostname
 *
 * Given a complete hostmask (nick!ident\@host) this function returns a copy
 * of the nickname. The result must be passed to free() when it is no longer used.
 *
 * @param Hostmask the hostmask
 */
char *NickFromHostmask(const char *Hostmask) {
	char *Copy;
	const char *ExclamationMark;

	ExclamationMark = strstr(Hostmask, "!");

	if (ExclamationMark == NULL) {
		return NULL;
	}

	Copy = strdup(Hostmask);

	if (Copy == NULL) {
		LOGERROR("strdup() failed. Could not parse hostmask (%s).", Hostmask);

		return NULL;
	}

	Copy[ExclamationMark - Hostmask] = '\0';

	return Copy;
}

/**
 * CreateListener
 *
 * Creates a new listening socket.
 *
 * @param Port the port this socket should listen on
 * @param BindIp the IP address this socket should be bound to
 * @param Family address family (i.e. IPv4 or IPv6)
 */
SOCKET CreateListener(unsigned short Port, const char *BindIp, int Family) {
	sockaddr *saddr;
	sockaddr_in sin;
#ifdef IPV6
	sockaddr_in6 sin6;
#endif
	const int optTrue = 1;
	bool Bound = false;
	SOCKET Listener;
	hostent* hent;

	Listener = socket(Family, SOCK_STREAM, IPPROTO_TCP);

	if (Listener == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}

	setsockopt(Listener, SOL_SOCKET, SO_REUSEADDR, (char *)&optTrue, sizeof(optTrue));

#ifdef IPV6
	if (Family == AF_INET) {
#endif
		sin.sin_family = AF_INET;
		sin.sin_port = htons(Port);

		saddr = (sockaddr *)&sin;
#ifdef IPV6
	} else {
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = htons(Port);

		saddr = (sockaddr *)&sin6;

#ifndef _WIN32
		setsockopt(Listener, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&optTrue, sizeof(optTrue));
#endif
	}
#endif

	if (BindIp) {
		hent = gethostbyname(BindIp);

		if (hent) {
#ifdef IPV6
			if (Family = AF_INET) {
#endif
				sin.sin_addr.s_addr = ((in_addr*)hent->h_addr_list[0])->s_addr;
#ifdef IPV6
			} else {
				memcpy(&(sin6.sin6_addr.s6_addr), &(((in6_addr*)hent->h_addr_list[0])->s6_addr), sizeof(in6_addr));
			}
#endif

			Bound = true;
		}
	}

	if (!Bound) {
#ifdef IPV6
		if (Family == AF_INET) {
#endif
			sin.sin_addr.s_addr = htonl(INADDR_ANY);
#ifdef IPV6
		} else {
			memcpy(&(sin6.sin6_addr.s6_addr), &in6addr_any, sizeof(in6_addr));
		}
#endif
	}

	if (bind(Listener, saddr, SOCKADDR_LEN(Family)) != 0) {
		closesocket(Listener);

		return INVALID_SOCKET;
	}

	if (listen(Listener, SOMAXCONN) != 0) {
		closesocket(Listener);

		return INVALID_SOCKET;
	}

	return Listener;
}

/**
 * UtilMd5
 *
 * Computes the MD5 hash of a given string and returns
 * a string representation of the hash.
 *
 * @param String the string which should be hashed
 */
const char *UtilMd5(const char *String) {
	MD5_CTX context;
	static char Result[33];
	unsigned char digest[16];
	unsigned int len = strlen(String);

	MD5Init (&context);
	MD5Update (&context, (unsigned char *)String, len);
	MD5Final (digest, &context);

	for (int i = 0; i < 16; i++) {
#undef sprintf
		sprintf(Result + i * 2, "%02x", digest[i]);
	}

	return Result;
}

/**
 * FlushCommands
 *
 * Destroy a list of commands.
 *
 * @param Commands the list
 */
void FlushCommands(commandlist_t *Commands) {
	if (Commands != NULL && *Commands != NULL) {
		delete *Commands;
		*Commands = NULL;
	}
}

/**
 * DestroyCommandT
 *
 * Destroy a command and frees any resources which it used. This function
 * is used internally by the hashtable which is used for storing commands.
 *
 * @param Command the command which is to be destroyed
 */
static void DestroyCommandT(command_t *Command) {
	free(Command->Category);
	free(Command->Description);
	free(Command->HelpText);
	free(Command);
}

/**
 * AddCommand
 *
 * Adds a command to a list of commands.
 *
 * @param Commands the list of commands
 * @param Name the name of the command
 * @param Category the category of the command (e.g. "Admin" or "User")
 * @param Description a short description of the command
 * @param HelpText the associated help text (can contain embedded \\n characters
 */
void AddCommand(commandlist_t *Commands, const char *Name, const char *Category, const char *Description, const char *HelpText) {
	command_t *Command;

	if (Commands == NULL) {
		return;
	}

	if (*Commands == NULL) {
		*Commands = new CHashtable<command_t *, false, 16>();

		if (*Commands == NULL) {
			LOGERROR("new operator failed. Could not add command.");

			return;
		}

		(*Commands)->RegisterValueDestructor(DestroyCommandT);
	}

	Command = (command_t *)malloc(sizeof(command_t));

	if (Command == NULL) {
		LOGERROR("malloc() failed. Could not add command.");

		return;
	}

	Command->Category = strdup(Category);
	Command->Description = strdup(Description);
	Command->HelpText = HelpText ? strdup(HelpText) : NULL;

	(*Commands)->Add(Name, Command);
}

/**
 * DeleteCommand
 *
 * Removes a command from a list of commands.
 *
 * @param Commands the list of commands
 * @param Name the name of the command which is to be removed
 */
void DeleteCommand(commandlist_t *Commands, const char *Name) {
	if (Commands != NULL && *Commands != NULL) {
		(*Commands)->Remove(Name);
	}
}

/**
 * CmpCommandT
 *
 * Compares two commands. This function is intended to be used with qsort().
 *
 * @param pA the first command
 * @param pB the second command
 */
int CmpCommandT(const void *pA, const void *pB) {
	const hash_t<command_t *> *a = (const hash_t<command_t *> *)pA;
	const hash_t<command_t *> *b = (const hash_t<command_t *> *)pB;

	int CmpCat = strcasecmp(a->Value->Category, b->Value->Category);

	if (CmpCat == 0)
		return strcasecmp(a->Name, b->Name);
	else
		return CmpCat;
}

/**
 * CmpString
 *
 * Compares two strings. This function is intended to be used with qsort().
 *
 * @param pA the first string
 * @param pB the second string
 */
int CmpString(const void *pA, const void *pB) {
	return strcmp(*(const char **)pA, *(const char **)pB);
}

/**
 * DestroyString
 *
 * Frees a string. Used by CHashtable
 *
 * @param String the string
 */
void DestroyString(char *String) {
	free(String);
}

/**
 * IpToString
 *
 * Converts a sockaddr struct into a string.
 *
 * @param Address the address
 */
const char *IpToString(sockaddr *Address) {
	static char Buffer[256];

#ifdef _WIN32
	sockaddr *Copy = (sockaddr *)malloc(SOCKADDR_LEN(Address->sa_family));
	DWORD BufferLength = sizeof(Buffer);

	memcpy(Copy, Address, SOCKADDR_LEN(Address->sa_family));

	if (Address->sa_family == AF_INET) {
		((sockaddr_in *)Address)->sin_port = 0;
	} else {
		((sockaddr_in6 *)Address)->sin6_port = 0;
	}

	WSAAddressToString(Address, SOCKADDR_LEN(Address->sa_family), NULL, Buffer, &BufferLength);

	free(Copy);
#else
	void *IpAddress;

	if (Address->sa_family == AF_INET) {
		IpAddress = &(((sockaddr_in *)Address)->sin_addr);
	} else {
		IpAddress = &(((sockaddr_in6 *)Address)->sin6_addr);
	}

	inet_ntop(Address->sa_family, IpAddress, Buffer, sizeof(Buffer));
#endif

	return Buffer;
}

/**
 * CompareAddress
 *
 * Compares two sockaddr structs.
 *
 * @param pA the first sockaddr
 * @param pB the second sockaddr
 */
int CompareAddress(sockaddr *pA, sockaddr *pB) {
	if (pA->sa_family != pB->sa_family) {
		return -1;
	}

	if (pA->sa_family == AF_INET) {
		if (((sockaddr_in *)pA)->sin_addr.s_addr == ((sockaddr_in *)pB)->sin_addr.s_addr) {
			return 0;
		} else {
			return 1;
		}
	}

	if (pA->sa_family == AF_INET6) {
		if (((sockaddr_in6 *)pA)->sin6_addr.s6_addr == ((sockaddr_in6 *)pB)->sin6_addr.s6_addr) {
			return 0;
		} else {
			return 1;
		}
	}

	return 2;
}

/**
 * StrTrim
 *
 * Removes leading and trailing spaces from a string.
 *
 * @param String the string
 */
void StrTrim(char *String) {
	size_t Length = strlen(String);
	size_t Offset = 0;

	// remove leading spaces
	for (int i = 0; i < Length; i++) {
		if (String[i] == ' ') {
			Offset++;
		} else {
			break;
		}
	}

	if (Offset > 0) {
		for (int i = 0; i < Length; i++) {
			String[i] = String[i + Offset];
		}
	}

	// remove trailing spaces
	while (String[strlen(String) - 1] == ' ') {
		String[strlen(String) - 1] = '\0';
	}
}