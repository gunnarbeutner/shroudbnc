/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2014 Gunnar Beutner                                      *
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

#include "StdAfx.h"

extern "C" {
	#include "../third-party/md5/md5.h"
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
	char *Copy;
	size_t LengthData, Size;

	if (Data == NULL) {
		return NULL;
	}

	LengthData = strlen(Data);

	Size = LengthData + 2;
	Copy = (char *)malloc(Size);

	if (AllocFailed(Copy)) {
		return NULL;
	}

	strmcpy(Copy, Data, Size);
	Copy[LengthData + 1] = '\0';

	for (unsigned int i = 0; i < LengthData; i++) {
		if (Copy[i] == ' ' && Copy[i + 1] != ' ' && Copy[i + 1] != '\0') {
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

	if (AllocFailed(ArgArray)) {
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

		if (strchr(Arg, ' ') != NULL || *(Arg - 1) == ':') {
			*(Arg - 1) = ':';
			*(Arg - 2) = ' ';
		} else {
			*(Arg - 1) = ' ';
		}
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
	size_t Len = 0;
	size_t Offset;
	int Count = ArgCount(ArgV[0]);

	for (int i = 0; i < Count; i++) {
		Len += strlen(ArgV[i]) + 1;
	}

	Dup = (char **)malloc(Count * sizeof(char *) + Len + 2);

	if (AllocFailed(Dup)) {
		return NULL;
	}

	Offset = (char *)Dup + Count * sizeof(char *) - ArgV[0];

	memcpy(Dup, ArgV, Count * sizeof(char *));
	memcpy((char *)Dup + Count * sizeof(char *), ArgV[0], Len + 2);

	for (int i = 0; i < Count; i++) {
		Dup[i] += Offset;
	}

	return (const char **)Dup;
}

/**
 * ArgFree
 *
 * Frees a tokenized string.
 *
 * @param Args the tokenized string
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
 * ArgTokenize2
 *
 * Tokenizes a string and returns it.
 *
 * @param String the string
 */
tokendata_t ArgTokenize2(const char *String) {
	tokendata_t tokens;
	register unsigned int a = 1;
	size_t Len = min(strlen(String), sizeof(tokens.String) - 1);

	memset(tokens.String, 0, sizeof(tokens.String));
	strmcpy(tokens.String, String, sizeof(tokens.String));

	tokens.Pointers[0] = 0;

	for (unsigned int i = 0; i < Len; i++) {
		if (String[i] == ' ' && String[i + 1] != ' ') {
			if (String[i + 1] == '\0') {
				tokens.String[i] = '\0';

				continue;
			}

			tokens.Pointers[a] = i + 1;
			tokens.String[i] = '\0';

			a++;

			if (a >= 32) {
				break;
			}

			if (String[i + 1] == ':') {
				tokens.Pointers[a - 1]++;

				break;
			}
		}
	}

	tokens.Count = a;

	return tokens;
}

/**
 * ArgToArray2
 *
 * Constructs a pointer array for a tokendata_t structure. The returned
 * array has to be passed to ArgFreeArray.
 *
 * @param Tokens the tokenized string
 */
const char **ArgToArray2(const tokendata_t& Tokens) {
	const char **Pointers;
	
	Pointers = (const char **)malloc(sizeof(const char *) * 33);

	if (AllocFailed(Pointers)) {
		return NULL;
	}

	memset(Pointers, 0, sizeof(const char *) * 33);

	for (unsigned int i = 0; i < min(Tokens.Count, (unsigned int)32); i++) {
		Pointers[i] = Tokens.Pointers[i] + Tokens.String;
	}

	return Pointers;
}

/**
 * ArgGet2
 *
 * Retrieves an argument from a tokenized string.
 *
 * @param Tokens the tokenized string
 * @param Arg the index of the argument which is to be returned
 */
const char *ArgGet2(const tokendata_t& Tokens, unsigned int Arg) {
	if (Arg >= Tokens.Count) {
		return NULL;
	} else {
		return Tokens.Pointers[Arg] + Tokens.String;
	}
}

/**
 * ArgCount2
 *
 * Returns the number of arguments in a tokenized string.
 *
 * @param Tokens the tokenized string
 */
unsigned int ArgCount2(const tokendata_t& Tokens) {
	return Tokens.Count;
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
SOCKET SocketAndConnect(const char *Host, unsigned int Port, const char *BindIp) {
	unsigned long lTrue = 1;
	sockaddr_in sin, sloc;
	SOCKET Socket;
	hostent *hent;
	unsigned long addr;
	int code;

	if (Host == NULL || Port == 0) {
		return INVALID_SOCKET;
	}

	Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (Socket == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}

	ioctlsocket(Socket, FIONBIO, &lTrue);

	if (BindIp && *BindIp) {
		memset(&sloc, 0, sizeof(sloc));

		sloc.sin_family = AF_INET;
		sloc.sin_port = 0;

		hent = gethostbyname(BindIp);

		if (hent) {
			in_addr *peer = (in_addr *)hent->h_addr_list[0];

			sloc.sin_addr.s_addr = peer->s_addr;
		} else {
			addr = inet_addr(BindIp);

			sloc.sin_addr.s_addr = addr;
		}

		bind(Socket, (sockaddr *)&sloc, sizeof(sloc));
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(Port);

	hent = gethostbyname(Host);

	if (hent) {
		in_addr *peer = (in_addr *)hent->h_addr_list[0];

		sin.sin_addr.s_addr = peer->s_addr;
	} else {
		addr = inet_addr(Host);

		sin.sin_addr.s_addr = addr;
	}

	code = connect(Socket, (const sockaddr *)&sin, sizeof(sin));

#ifdef _WIN32
	if (code != 0 && errno != WSAEWOULDBLOCK) {
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
SOCKET SocketAndConnectResolved(const sockaddr *Host, const sockaddr *BindIp, int *error) {
	unsigned long lTrue = 1;
	int Code, Size;

	SOCKET Socket = socket(Host->sa_family, SOCK_STREAM, IPPROTO_TCP);

#ifdef _WIN32
	*error = WSAGetLastError();
#else
	*error = errno;
#endif

	if (Socket == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}

	ioctlsocket(Socket, FIONBIO, &lTrue);

	if (BindIp != NULL) {
		if (bind(Socket, (sockaddr *)BindIp, SOCKADDR_LEN(BindIp->sa_family)) != 0) {
#ifdef _WIN32
			*error = WSAGetLastError();
#else
			*error = errno;
#endif

			closesocket(Socket);

			return INVALID_SOCKET;
		}
	}

	Size = SOCKADDR_LEN(Host->sa_family);

	Code = connect(Socket, Host, Size);

#ifdef _WIN32
	*error = WSAGetLastError();
#else
	*error = errno;
#endif

#ifdef _WIN32
	if (Code != 0 && *error != WSAEWOULDBLOCK) {
#else
	if (Code != 0 && *error != EINPROGRESS) {
#endif
		closesocket(Socket);

		return INVALID_SOCKET;
	}

	*error = 0;

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

	ExclamationMark = strchr(Hostmask, '!');

	if (ExclamationMark == NULL) {
		return NULL;
	}

	Copy = strdup(Hostmask);

	if (AllocFailed(Copy)) {
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
SOCKET CreateListener(unsigned int Port, const char *BindIp, int Family) {
	sockaddr *saddr;
	sockaddr_in sin;
#ifdef HAVE_IPV6
	sockaddr_in6 sin6;
#endif /* HAVE_IPV6 */
	const int optTrue = 1;
	bool Bound = false;
	SOCKET Listener;
	hostent *hent;

	Listener = socket(Family, SOCK_STREAM, IPPROTO_TCP);

	if (Listener == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}

#ifndef _WIN32
	setsockopt(Listener, SOL_SOCKET, SO_REUSEADDR, (char *)&optTrue, sizeof(optTrue));
#endif

#ifdef HAVE_IPV6
	if (Family == AF_INET) {
#endif /* HAVE_IPV6 */
		sin.sin_family = AF_INET;
		sin.sin_port = htons(Port);

		saddr = (sockaddr *)&sin;
#ifdef HAVE_IPV6 /* HAVE_IPV6 */
	} else if (Family == AF_INET6) {
		memset(&sin6, 0, sizeof(sin6));
		sin6.sin6_family = AF_INET6;
		sin6.sin6_port = htons(Port);

		saddr = (sockaddr *)&sin6;

#if !defined(_WIN32) && defined(IPV6_V6ONLY)
		setsockopt(Listener, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&optTrue, sizeof(optTrue));
#endif
	} else {
		closesocket(Listener);

		return INVALID_SOCKET;
	}
#endif /* HAVE_IPV6 */

	if (BindIp) {
		hent = gethostbyname(BindIp);

		if (hent) {
#ifdef HAVE_IPV6
			if (Family == AF_INET) {
#endif /* HAVE_IPV6 */
				sin.sin_addr.s_addr = ((in_addr*)hent->h_addr_list[0])->s_addr;
#ifdef HAVE_IPV6
			} else {
				memcpy(&(sin6.sin6_addr.s6_addr), &(((in6_addr*)hent->h_addr_list[0])->s6_addr), sizeof(in6_addr));
			}
#endif /* HAVE_IPV6 */

			Bound = true;
		}
	}

	if (!Bound) {
#ifdef HAVE_IPV6
		if (Family == AF_INET) {
#endif /* HAVE_IPV6 */
			sin.sin_addr.s_addr = htonl(INADDR_ANY);
#ifdef HAVE_IPV6
		} else {
			const struct in6_addr v6any = IN6ADDR_ANY_INIT;

			memcpy(&(sin6.sin6_addr.s6_addr), &v6any, sizeof(in6_addr));
		}
#endif /* HAVE_IPV6 */
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
 * UtilSaltedMd5
 *
 * Computes the MD5 hash of a given string and returns
 * a string representation of the hash.
 *
 * @param String the string which should be hashed
 * @param Salt the salt value
 */
const char *UtilMd5(const char *String, const char *Salt) {
#ifdef HAVE_LIBSSL
	MD5_CTX context;
#else /* HAVE_LIBSSL */
	sMD5_CTX context;
#endif /* HAVE_LIBSSL */
	unsigned char digest[16];
	char *StringAndSalt, *StringPtr;
	static char *SaltAndResult = NULL;
	int rc;

	free(SaltAndResult);

	if (Salt != NULL) {
		rc = asprintf(&StringAndSalt, "%s%s", String, Salt);
	} else {
		rc = asprintf(&StringAndSalt, "%s", String);
	}

	if (RcFailed(rc)) {
		g_Bouncer->Fatal();
	}

#ifdef HAVE_LIBSSL
	MD5_Init(&context);
	MD5_Update(&context, (unsigned char *)StringAndSalt, strlen(StringAndSalt));
	MD5_Final(digest, &context);
#else /* HAVE_LIBSSL */
	MD5Init(&context);
	MD5Update(&context, (unsigned char *)StringAndSalt, strlen(StringAndSalt));
	MD5Final(digest, &context);
#endif /* HAVE_LIBSSL */

	free(StringAndSalt);

	if (Salt != NULL) {
		SaltAndResult = (char *)malloc(strlen(Salt) + 50);

		if (AllocFailed(SaltAndResult)) {
			g_Bouncer->Fatal();
		}

		strmcpy(SaltAndResult, Salt, strlen(Salt) + 50);
		strmcat(SaltAndResult, "$", strlen(Salt) + 50);
		StringPtr = SaltAndResult + strlen(SaltAndResult);
	} else {
		StringPtr = SaltAndResult = (char *)malloc(50);

		if (AllocFailed(SaltAndResult)) {
			g_Bouncer->Fatal();
		}
	}

	for (int i = 0; i < 16; i++) {
		/* TODO: don't use sprintf */
		sprintf(StringPtr + i * 2, "%02x", digest[i]);
	}

	return SaltAndResult;
}

/**
 * GenerateSalt
 *
 * Generates a salt value which is suitable for use with UtilMd5().
 */
const char *GenerateSalt(void) {
	static char Salt[33];

	for (unsigned int i = 0; i < sizeof(Salt) - 1; i++) {
		do {
			Salt[i] = '0' + rand() % ('^' - '0');
		} while (Salt[i] == '$');
	}

	Salt[sizeof(Salt) - 1] = '\0';

	return Salt;
}

/**
 * SaltFromHash
 *
 * Returns the salt value for a hash (or NULL if there is none).
 *
 * @param Hash the hash value
 */
const char *SaltFromHash(const char *Hash) {
	static char *Salt = NULL;
	const char *HashSign;

	HashSign = strchr(Hash, '$');

	if (HashSign == NULL) {
		return NULL;
	}

	free(Salt);

	Salt = (char *)malloc(HashSign - Hash + 1);

	if (AllocFailed(Salt)) {
		g_Bouncer->Fatal();
	}

	strmcpy(Salt, Hash, HashSign - Hash + 1);

	return Salt;
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
	delete Command;
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
		*Commands = new CHashtable<command_t *, false>();

		if (AllocFailed(*Commands)) {
			return;
		}

		(*Commands)->RegisterValueDestructor(DestroyCommandT);
	}

	Command = new command_t;

	if (AllocFailed(Command)) {
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

	if (CmpCat == 0) {
		return strcasecmp(a->Name, b->Name);
	} else {
		return CmpCat;
	}
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
	DWORD BufferLength = sizeof(Buffer);

	if (WSAAddressToString(Address, SOCKADDR_LEN(Address->sa_family), NULL, Buffer, &BufferLength) != 0) {
		return NULL;
	}
#else
	void *IpAddress;

	if (Address->sa_family == AF_INET) {
		IpAddress = &(((sockaddr_in *)Address)->sin_addr);
	} else {
		IpAddress = &(((sockaddr_in6 *)Address)->sin6_addr);
	}

	if (inet_ntop(Address->sa_family, IpAddress, Buffer, sizeof(Buffer)) == NULL) {
		return NULL;
	}
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
int CompareAddress(const sockaddr *pA, const sockaddr *pB) {
	if (pA == NULL || pB == NULL) {
		return -1; // two NULL addresses are never equal
	}

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
		if (memcmp(&(((sockaddr_in6 *)pA)->sin6_addr.s6_addr),
				&(((sockaddr_in6 *)pB)->sin6_addr.s6_addr), sizeof(in6_addr)) == 0) {
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
 * Removes the specified char from the start and end of the string.
 *
 * @param String the string
 */
void StrTrim(char *String, char Character) {
	size_t Length = strlen(String);
	size_t Offset = 0, i;

	// remove leading chars
	for (i = 0; i < Length; i++) {
		if (String[i] == Character) {
			Offset++;
		} else {
			break;
		}
	}

	if (Offset > 0) {
		for (i = 0; i < Length; i++) {
			String[i] = String[i + Offset];
		}
	}

	// remove trailing chars
	while (String[strlen(String) - 1] == Character) {
		String[strlen(String) - 1] = '\0';
	}
}

/**
 * SetPermissions
 *
 * A wrapper for chmod.
 *
 * @param Filename the file
 * @param Modes the new permissions
 */
int SetPermissions(const char *Filename, int Modes) {
#ifndef _WIN32
	return chmod(Filename, Modes);
#else
	return 1;
#endif
}

/**
 * FreeString
 *
 * Calls free() on a string.
 *
 * @param String the string
 */
void FreeString(char *String) {
	free(String);
}

/**
 * strmcpy
 *
 * Behaves like strncpy. However this function guarantees that Destination
 * will always be zero-terminated (unless Size is 0).
 *
 * @param Destination destination string
 * @param Source source string
 * @param Size size of the Destination buffer
 */
char *strmcpy(char *Destination, const char *Source, size_t Size) {
	size_t CopyLength = min(strlen(Source), (Size > 0) ? Size - 1 : 0);

	memcpy(Destination, Source, CopyLength);
	Destination[CopyLength] = '\0';

	return Destination;
}

/**
 * strmcat
 *
 * Behaves like strncat. However this function guarantees that Destination
 * will always be zero-terminated (unless Size is 0).
 *
 * @param Destination destination string
 * @param Source source string
 * @param Size size of the Destination buffer
 */
char *strmcat(char *Destination, const char *Source, size_t Size) {
	size_t Offset = strlen(Destination);
	size_t CopyLength = min(strlen(Source), Size - Offset - 1);

#ifdef _DEBUG
	if (CopyLength != strlen(Source)) {
		DebugBreak();
	}
#endif

	memcpy(Destination + Offset, Source, CopyLength);
	Destination[Offset + CopyLength] = '\0';

	return Destination;
}

const sockaddr *HostEntToSockAddr(hostent *HostEnt) {
	static sockaddr_storage sin;

	memset(&sin, 0, sizeof(sin));

	sin.ss_family = HostEnt->h_addrtype;

	if (HostEnt->h_addrtype == AF_INET) {
		((sockaddr_in *)&sin)->sin_port = 0;
		memcpy(&(((sockaddr_in *)&sin)->sin_addr), HostEnt->h_addr_list[0], sizeof(in_addr));
#ifdef HAVE_IPV6
	} else if (HostEnt->h_addrtype == AF_INET6) {
		((sockaddr_in6 *)&sin)->sin6_port = 0;
		memcpy(&(((sockaddr_in6 *)&sin)->sin6_addr), HostEnt->h_addr_list[0], sizeof(in6_addr));
#endif /* HAVE_IPV6 */
	} else {
		return NULL;
	}

	return (sockaddr *)&sin;
}

bool StringToIp(const char *IP, int Family, sockaddr *SockAddr, socklen_t Length) {
	memset(SockAddr, 0, Length);

#ifdef _WIN32
	socklen_t *LengthPtr = &Length;

	if (WSAStringToAddress(const_cast<char *>(IP), Family, NULL, SockAddr, LengthPtr) != 0) {
		return false;
	}
#else
	if (Length < SOCKADDR_LEN(Family)) {
		return false;
	}

	if (inet_pton(Family, IP, SockAddr) <= 0) {
		return false;
	}
#endif

	return true;
}

static int passwd_cb(char *Buffer, int Size, int RWFlag, void *Cookie) {
	int Result;
	char ConfirmBuffer[128];

	if (Size > 128) {
		Size = 128; // nobody could seriously have such a passphrase...
	}

	do {
		printf("PEM passphrase: ");
		
		Result = sn_getline_passwd(Buffer, Size);
		printf("\n");
		
		if (Result <= 0) {
			return 0;
		}

		if (RWFlag == 1) {
			printf("Confirm PEM passphrase: ");
			
			Result = sn_getline_passwd(ConfirmBuffer, sizeof(ConfirmBuffer));
			printf("\n");

			if (Result <= 0) {
				return 0;
			}

			if (strcmp(Buffer, ConfirmBuffer) != 0) {
				printf("The passwords you specified do not match. Please try again.\n");

				continue;
			}
		}

		break;
	} while (1);

	return strlen(Buffer);
}

void SSL_CTX_set_passwd_cb(SSL_CTX *Context) {
#ifdef HAVE_LIBSSL
	SSL_CTX_set_default_passwd_cb(Context, passwd_cb);
#endif /* HAVE_LIBSSL */
}

#ifndef HAVE_POLL
/*
 *  prt
 *
 *  Copyright 1994 University of Washington
 *
 *  Permission is hereby granted to copy this software, and to
 *  use and redistribute it, except that this notice may not be
 *  removed.  The University of Washington does not guarantee
 *  that this software is suitable for any purpose and will not
 *  be held liable for any damage it may cause.
 */

/*
**  emulate poll() for those platforms (Ultrix) that don't have it.
*/
int poll(pollfd *fds, unsigned long nfds, int timo) {
	struct timeval timeout, *toptr;
	fd_set ifds, ofds, efds, *ip, *op;
	int i, rc, n;

	FD_ZERO(&ifds);
	FD_ZERO(&ofds);
	FD_ZERO(&efds);

	for (i = 0, n = -1, op = ip = 0; i < (int)nfds; ++i) {
		fds[i].revents = 0;

		if (fds[i].fd < 0) {
			continue;
		}

		if (fds[i].fd > n) {
			n = fds[i].fd;
		}

		if (fds[i].events & (POLLIN|POLLPRI)) {
			ip = &ifds;
			FD_SET(fds[i].fd, ip);
		}

		if (fds[i].events & POLLOUT) {
			op = &ofds;
			FD_SET(fds[i].fd, op);
		}

		FD_SET(fds[i].fd, &efds);
	}

	if (timo < 0) {
		toptr = 0;
	} else {
		toptr = &timeout;
		timeout.tv_sec = timo / 1000;
		timeout.tv_usec = (timo - timeout.tv_sec * 1000) * 1000;
	}

	rc = select(++n, ip, op, &efds, toptr);

	if (rc <= 0) {
		return rc;
	}

	for (i = 0, n = 0; i < (int)nfds; ++i) {
		if (fds[i].fd < 0) {
			continue;
		}

		if (fds[i].events & (POLLIN|POLLPRI) && FD_ISSET(fds[i].fd, &ifds)) {
			fds[i].revents |= POLLIN;
		}

		if (fds[i].events & POLLOUT && FD_ISSET(fds[i].fd, &ofds)) {
			fds[i].revents |= POLLOUT;
		}

		if (FD_ISSET(fds[i].fd, &efds)) {
			/* Some error was detected ... should be some way to know. */
			fds[i].revents |= POLLHUP;
		}
	}

	return rc;
}
#endif /* !HAVE_POLL */

int sn_getline(char *buf, size_t size) {
	if (fgets(buf, size, stdin) == NULL) {
		return -1;
	}


	for (char *p = buf + strlen(buf); p >= buf; p--) {
		if (*p == '\r' || *p == '\n') {
			*p = '\0';

			break;
		}
	}

	return 1;
}

int sn_getline_passwd(char *buf, size_t size) {
	bool term_succeeded;
	int result;
#ifndef _WIN32
	termios term_old, term_new;
#else
	HANDLE StdInHandle;
	DWORD ConsoleModes, NewConsoleModes;
#endif

#ifndef _WIN32
	if (tcgetattr(STDIN_FILENO, &term_old) == 0) {
		memcpy(&term_new, &term_old, sizeof(term_old));
		term_new.c_lflag &= ~ECHO;

		tcsetattr(STDIN_FILENO, TCSANOW, &term_new);

		term_succeeded = true;
	} else {
		term_succeeded = false;
	}
#else
	StdInHandle = GetStdHandle(STD_INPUT_HANDLE);

	if (StdInHandle != INVALID_HANDLE_VALUE) {
		if (GetConsoleMode(StdInHandle, &ConsoleModes)) {
			NewConsoleModes = ConsoleModes & ~ENABLE_ECHO_INPUT;

			SetConsoleMode(StdInHandle,NewConsoleModes);

			term_succeeded = true;
		} else {
			term_succeeded = false;
		}
	} else {
		term_succeeded = false;
	}
#endif

	result = sn_getline(buf, size);

	if (term_succeeded) {
#ifndef _WIN32
		tcsetattr(STDIN_FILENO, TCSANOW, &term_old);
#else
		SetConsoleMode(StdInHandle, ConsoleModes);
#endif
	}

	return result;
}

bool RcFailedInternal(int ReturnCode, const char *File, int Line) {
	if (ReturnCode >= 0) {
		return false;
	}

	if (g_Bouncer != NULL) {
		g_Bouncer->Log("Function call in %s:%d failed: %s\n", File, Line, strerror(errno));
	} else {
		printf("Function call in %s:%d failed: %s\n", File, Line, strerror(errno));
	}

	return true;
}

bool AllocFailedInternal(const void *Ptr, const char *File, int Line) {
	if (Ptr != NULL) {
		return false;
	}

	if (g_Bouncer != NULL) {
		g_Bouncer->Log("Allocation in %s:%d failed: %s\n", File, Line, strerror(errno));
	} else {
		printf("Allocation in %s:%d failed: %s\n", File, Line, strerror(errno));
	}

	return true;
}

#ifndef _WIN32
lt_dlhandle sbncLoadLibrary(const char *Filename) {
	lt_dlhandle handle = 0;
	lt_dladvise advise;

	if (!lt_dladvise_init(&advise) && !lt_dladvise_global(&advise)) {
		handle = lt_dlopenadvise(Filename, advise);
	}

	lt_dladvise_destroy(&advise);

	return handle;
}
#endif

void gfree(void *ptr) {
	/**
	 * Win32 might have separate heaps for sbnc and its modules (depending
	 * on how we link to the CRT) - so in order to free memory that was allocated
	 * by sbnc (e.g. using asprintf) we need to export a function
	 * that calls the appropriate version of free().
	 */
	free(ptr);
}
