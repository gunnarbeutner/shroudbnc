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

// utility functions

#undef USESSL
#include "StdAfx.h"

extern "C" {
	        #include "md5-c/global.h"
	        #include "md5-c/md5.h"
}

char* g_Args = NULL;
char** g_ArgArray = NULL;

extern SOCKET g_last_sock;
extern time_t g_LastReconnect;

char* LastArg(char* Args) {
	char* p;

	if (*Args == ':') {
		p = Args + 1;
	} else {
		p = strstr(Args, " :");

		if (p)
			p += 2;
		else {
			p = strstr(Args, " ");

			if (p)
				p++;
			else
				p = Args;
		}
	}

	return p;
}

// proc: ArgTokenize
// purp: tokenizes the given Data (i.e. splits parameters into arguments)
const char* ArgTokenize(const char* Data) {
	char* Copy;

	Copy = (char*)malloc(strlen(Data) + 2);

	if (Copy == NULL) {
		LOGERROR("malloc() failed. Could not tokenize string (%s).", Data);

		return NULL;
	}

	strcpy(Copy, Data);
	Copy[strlen(Data) + 1] = '\0';

	for (unsigned int i = 0; i < strlen(Data); i++) {
		if (Copy[i] == ' ') {
//			if (++Count == MaxArgs && MaxArgs != -1)
//				break;

			Copy[i] = '\0';

			if (i > 0 && Copy[i + 1] == ':')
				break;
		}
	}

	return Copy;
}

const char* ArgParseServerLine(const char* Data) {
	if (*Data == ':')
		Data++;

	return ArgTokenize(Data);
}

// proc: ArgGet
// purp: gets an argument from the previously tokenized data
const char* ArgGet(const char* Args, int Arg) {
	for (int i = 0; i < Arg - 1; i++) {
		Args += strlen(Args) + 1;

		if (strlen(Args) == 0)
			return NULL;
	}

	if (*Args == ':')
		Args++;

	return Args;
}

// proc: ArgCount
// purp: gets the argument count
int ArgCount(const char* Args) {
	int Count = 0;

	if (Args == NULL)
		return 0;

	while (true) {
		Args += strlen(Args) + 1;
		Count++;

		if (strlen(Args) == 0)
			return Count;
	}
}

const char** ArgToArray(const char* Args) {
	int Count = ArgCount(Args);

	const char** ArgArray = (const char**)malloc(Count * sizeof(const char*));

	if (ArgArray == NULL)
		return NULL;

	for (int i = 0; i < Count; i++) {
		ArgArray[i] = ArgGet(Args, i + 1);
	}

	return ArgArray;
}

void ArgFree(const char* Args) {
	free(const_cast<char*>(Args));
}

void ArgFreeArray(const char** Array) {
	free(const_cast<char**>(Array));
}

SOCKET SocketAndConnect(const char* Host, unsigned short Port, const char* BindIp) {
	if (!Host || !Port)
		return INVALID_SOCKET;

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock == INVALID_SOCKET)
		return INVALID_SOCKET;

	unsigned long lTrue = 1;
	ioctlsocket(sock, FIONBIO, &lTrue);

	if (g_last_sock < sock)
		g_last_sock = sock;

	sockaddr_in sin, sloc;

	if (BindIp && *BindIp) {
		sloc.sin_family = AF_INET;
		sloc.sin_port = 0;

		hostent* hent = gethostbyname(BindIp);

		if (hent) {
			in_addr* peer = (in_addr*)hent->h_addr_list[0];

	#ifdef _WIN32
			sloc.sin_addr.S_un.S_addr = peer->S_un.S_addr;
	#else
			sloc.sin_addr.s_addr = peer->s_addr;
	#endif
		} else {
			unsigned long addr = inet_addr(BindIp);

	#ifdef _WIN32
			sloc.sin_addr.S_un.S_addr = addr;
	#else
			sloc.sin_addr.s_addr = addr;
	#endif
		}

		bind(sock, (sockaddr*)&sloc, sizeof(sloc));
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(Port);

	hostent* hent = gethostbyname(Host);

	if (hent) {
		in_addr* peer = (in_addr*)hent->h_addr_list[0];

#ifdef _WIN32
		sin.sin_addr.S_un.S_addr = peer->S_un.S_addr;
#else
		sin.sin_addr.s_addr = peer->s_addr;
#endif
	} else {
		unsigned long addr = inet_addr(Host);

#ifdef _WIN32
		sin.sin_addr.S_un.S_addr = addr;
#else
		sin.sin_addr.s_addr = addr;
#endif
	}

	int code = connect(sock, (const sockaddr*)&sin, sizeof(sin));

#ifdef _WIN32
	if (code != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
#else
	if (code != 0 && errno != EINPROGRESS) {
#endif
		closesocket(sock);
		return INVALID_SOCKET;
	}

	g_LastReconnect = time(NULL);

	return sock;
}

SOCKET SocketAndConnectResolved(in_addr Host, unsigned short Port, const char* BindIp) {
	if (!Port)
		return INVALID_SOCKET;

	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock == INVALID_SOCKET)
		return INVALID_SOCKET;

	unsigned long lTrue = 1;
	ioctlsocket(sock, FIONBIO, &lTrue);

	if (g_last_sock < sock)
		g_last_sock = sock;

	sockaddr_in sin, sloc;

	if (BindIp && *BindIp) {
		sloc.sin_family = AF_INET;
		sloc.sin_port = 0;

		hostent* hent = gethostbyname(BindIp);

		if (hent) {
			in_addr* peer = (in_addr*)hent->h_addr_list[0];

	#ifdef _WIN32
			sloc.sin_addr.S_un.S_addr = peer->S_un.S_addr;
	#else
			sloc.sin_addr.s_addr = peer->s_addr;
	#endif
		} else {
			unsigned long addr = inet_addr(BindIp);

	#ifdef _WIN32
			sloc.sin_addr.S_un.S_addr = addr;
	#else
			sloc.sin_addr.s_addr = addr;
	#endif
		}

		bind(sock, (sockaddr*)&sloc, sizeof(sloc));
	}

	sin.sin_family = AF_INET;
	sin.sin_port = htons(Port);

#ifdef _WIN32
	sin.sin_addr = Host;
#else
	sin.sin_addr = Host;
#endif

	int code = connect(sock, (const sockaddr*)&sin, sizeof(sin));

#ifdef _WIN32
	if (code != 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
#else
	if (code != 0 && errno != EINPROGRESS) {
#endif
		closesocket(sock);
		return INVALID_SOCKET;
	}

	g_LastReconnect = time(NULL);

	return sock;
}

CIRCConnection* CreateIRCConnection(const char* Host, unsigned short Port, CBouncerUser* Owning, const char* BindIp) {
	g_LastReconnect = time(NULL);

	return new CIRCConnection(Host, Port, Owning, BindIp);
}

char* NickFromHostmask(const char* Hostmask) {
	const char* Ex = strstr(Hostmask, "!");

	if (!Ex)
		return NULL;
	else {
		char* Copy = strdup(Hostmask);

		if (Copy == NULL) {
			LOGERROR("strdup() failed. Could not parse hostmask (%s).", Hostmask);

			return NULL;
		}

		Copy[Ex - Hostmask] = '\0';

		return Copy;
	}
}

SOCKET CreateListener(unsigned short Port, const char* BindIp) {
	int code;
	SOCKET Listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (Listener == INVALID_SOCKET)
		return INVALID_SOCKET;

	g_last_sock = Listener;

	sockaddr_in sin;

	sin.sin_family = AF_INET;
	sin.sin_port = htons(Port);

	bool bound = false;

	if (BindIp) {
		hostent* hent = gethostbyname(BindIp);

		if (hent) {
			in_addr* peer = (in_addr*)hent->h_addr_list[0];

	#ifdef _WIN32
			sin.sin_addr.S_un.S_addr = peer->S_un.S_addr;
	#else
			sin.sin_addr.s_addr = peer->s_addr;
	#endif

			bound = true;
		}
	}


	if (!bound) {
#ifdef _WIN32
		sin.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
#else
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
#endif
	}

	code = bind(Listener, (sockaddr*)&sin, sizeof(sin));

	if (code != 0) {
		closesocket(Listener);

		return INVALID_SOCKET;
	}

	code = listen(Listener, SOMAXCONN);

	if (code != 0) {
		closesocket(Listener);

		return INVALID_SOCKET;
	}

	return Listener;

}

void string_free(char* string) {
	free(string);
}

int keyStrCmp(const void* a, const void* b) {
	return strcmp(*(const char**)a, *(const char**)b);
}

const char* UtilMd5(const char* String) {
	MD5_CTX context;
	static char Result[33];
	unsigned char digest[16];
	unsigned int len = strlen(String);

	MD5Init (&context);
	MD5Update (&context, (unsigned char*)String, len);
	MD5Final (digest, &context);

#undef sprintf

	for (int i = 0; i < 16; i++) {
		sprintf(Result + i * 2, "%02x", digest[i]);
	}

	return Result;
}
