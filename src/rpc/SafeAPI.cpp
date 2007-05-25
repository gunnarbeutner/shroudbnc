/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                      *
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

#include "../StdAfx.h"

// TODO: do NOT use real pointers for box_t/element_t, or at least verify that
// they are 100% valid

#ifdef RPCSERVER

#include "Box.h"

int g_RpcErrno = 0;

// int safe_socket(int Domain, int Type, int Protocol);
bool RpcFunc_socket(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Integer || Arguments[2].Type != Integer) {
		return false;
	}

	Result = socket(Arguments[0].Integer, Arguments[1].Integer, Arguments[2].Integer);

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_getpeername(int Socket, struct sockaddr *Sockaddr, socklen_t *Len);
bool RpcFunc_getpeername(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Block) {
		return false;
	}

	Result = getpeername(Arguments[0].Integer, (sockaddr *)Arguments[1].Block, (socklen_t *)Arguments[2].Block);

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_getsockname(int Socket, struct sockaddr *Sockaddr, socklen_t *Len);
bool RpcFunc_getsockname(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Block) {
		return false;
	}

	Result = getsockname(Arguments[0].Integer, (sockaddr *)Arguments[1].Block, (socklen_t *)Arguments[2].Block);

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_bind(int Socket, const struct sockaddr *Sockaddr, socklen_t Len);
bool RpcFunc_bind(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Integer) {
		return false;
	}

	Result = bind(Arguments[0].Integer, (sockaddr *)Arguments[1].Block, Arguments[2].Integer);

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_connect(int Socket, const struct sockaddr *Sockaddr, socklen_t Len);
bool RpcFunc_connect(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Integer) {
		return false;
	}

	Result = connect(Arguments[0].Integer, (sockaddr *)Arguments[1].Block, Arguments[2].Integer);

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_listen(int Socket, int Backlog);
bool RpcFunc_listen(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;
		
	if (Arguments[0].Type != Integer || Arguments[1].Type != Integer) {
		return false;
	}

	Result = listen(Arguments[0].Integer, Arguments[1].Integer);
	
	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_accept(int Socket, sockaddr *Sockaddr, socklen_t *Len);
bool RpcFunc_accept(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Block) {
		return false;
	}

	Result = accept(Arguments[0].Integer, (sockaddr *)Arguments[1].Block, (socklen_t *)Arguments[2].Block);
	
	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
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
		if (fds[i].fd < 0)
			continue;
		if (fds[i].fd > n)
			n = fds[i].fd;
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

	if (timo < 0)
		toptr = 0;
	else {
		toptr = &timeout;
		timeout.tv_sec = timo / 1000;
		timeout.tv_usec = (timo - timeout.tv_sec * 1000) * 1000;
    }

    rc = select(++n, ip, op, &efds, toptr);

	if (rc <= 0)
		return rc;

    for (i = 0, n = 0; i < (int)nfds; ++i) {
		if (fds[i].fd < 0)
			continue;

		if (fds[i].events & (POLLIN|POLLPRI) && FD_ISSET(fds[i].fd, &ifds))
			fds[i].revents |= POLLIN;

		if (fds[i].events & POLLOUT && FD_ISSET(fds[i].fd, &ofds))
			fds[i].revents |= POLLOUT;

		if (FD_ISSET(fds[i].fd, &efds))
			/* Some error was detected ... should be some way to know. */
			fds[i].revents |= POLLHUP;
    }
    return rc;
}
#endif /* !HAVE_POLL */

// int safe_poll(struct pollfd *Sockets, int Nfds, int Timeout);
bool RpcFunc_poll(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Block || Arguments[1].Type != Integer || Arguments[2].Type != Integer) {
		return false;
	}

	Result = poll((pollfd *)Arguments[0].Block, Arguments[1].Integer, Arguments[2].Integer);

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_recv(int Socket, void *Buffer, size_t Size, int Flags);
bool RpcFunc_recv(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Integer || Arguments[3].Type != Integer) {
		return false;
	}

	Result = recv(Arguments[0].Integer, (char *)Arguments[1].Block, Arguments[2].Integer, Arguments[3].Integer);
	Arguments[1].Flags = Flag_Out; // clear Flag_Alloc bit

	if (Result >= 0) {
		Arguments[1].Size = Result;
	} else {
		Arguments[1].Size = 0;
	}

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_send(int Socket, const void *Buffer, size_t Size, int Flags);
bool RpcFunc_send(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Integer || Arguments[3].Type != Integer) {
		return false;
	}

	Result = send(Arguments[0].Integer, (char *)Arguments[1].Block, Arguments[2].Integer, Arguments[3].Integer);

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

//int safe_shutdown(int Socket, int How);
bool RpcFunc_shutdown(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Integer) {
		return false;
	}

	Result = shutdown(Arguments[0].Integer, Arguments[1].Integer);

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_closesocket(int Socket);
bool RpcFunc_closesocket(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer) {
		return false;
	}

	Result = closesocket(Arguments[0].Integer);

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int SBNCAPI safe_getsockopt(int Socket, int Level, int OptName, char *OptVal, int *OptLen);
bool RpcFunc_getsockopt(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer) {
		return false;
	}

	Result = getsockopt(Arguments[0].Integer, Arguments[1].Integer, Arguments[2].Integer, (char *)Arguments[3].Block, (socklen_t *)Arguments[4].Block);

	g_RpcErrno = WSAGetLastError();

	Arguments[3].Flags = Flag_Out;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int SBNCAPI safe_setsockopt(int Socket, int Level, int OptName, const char *OptVal, int OptLen);
bool RpcFunc_setsockopt(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Integer || Arguments[2].Type != Integer ||
			Arguments[3].Type != Block || Arguments[4].Type != Integer) {
		return false;
	}

	Result = setsockopt(Arguments[0].Integer, Arguments[1].Integer, Arguments[2].Integer, (const char *)Arguments[3].Block, Arguments[4].Integer);

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int SBNCAPI safe_ioctlsocket(int Socket, long Command, unsigned long *ArgP);
bool RpcFunc_ioctlsocket(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Integer || Arguments[2].Type != Block) {
		return false;
	}

	Result = ioctlsocket(Arguments[0].Integer, Arguments[1].Integer, (unsigned long *)Arguments[2].Block);

	Arguments[2].Flags = Flag_Out;

	g_RpcErrno = WSAGetLastError();

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_errno(void);
bool RpcFunc_errno(Value_t *Arguments, Value_t *ReturnValue) {
	*ReturnValue = RPC_INT(errno);

	return true;
}

// int safe_print(const char *Line);
bool RpcFunc_print(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Block) {
		return false;
	}

	Result = fwrite(Arguments[0].Block, 1, strlen((const char *)Arguments[0].Block), stdout);

	g_RpcErrno = errno;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_scan(char *Buffer, size_t Size);
bool RpcFunc_scan(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Block || Arguments[1].Type != Integer) {
		return false;
	}

	if (fgets((char *)Arguments[0].Block, Arguments[1].Integer, stdin) != NULL) {
		Result = 1;
	} else {
		Result = -1;
	}

	g_RpcErrno = errno;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_scan_passwd(char *Buffer, size_t Size);
bool RpcFunc_scan_passwd(Value_t *Arguments, Value_t *ReturnValue) {
	bool term_succeeded;
	bool Result;
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
	}
#endif

	Result = RpcFunc_scan(Arguments, ReturnValue);

	if (term_succeeded) {
#ifndef _WIN32
		tcsetattr(STDIN_FILENO, TCSANOW, &term_old);
#else
		SetConsoleMode(StdInHandle, ConsoleModes);
#endif
	}

	return Result;
}

// size_t safe_sendto(int Socket, const void *Buffer, size_t Len, int Flags, const struct sockaddr *To, socklen_t ToLen);
bool RpcFunc_sendto(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Integer ||
			Arguments[3].Type != Integer || Arguments[4].Type != Block || Arguments[5].Type != Integer) {
		return false;
	}

	Result = sendto(Arguments[0].Integer, (const char *)Arguments[1].Block, Arguments[2].Integer,
		Arguments[3].Integer, (const sockaddr *)Arguments[4].Block, Arguments[5].Integer);

	g_RpcErrno = errno;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// size_t safe_recvfrom(int Socket, void *Buffer, size_t Len, int Flags, struct sockaddr *From, socklen_t *FromLen);
bool RpcFunc_recvfrom(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Integer ||
			Arguments[3].Type != Integer || Arguments[4].Type != Block || Arguments[5].Type != Block) {
		return false;
	}

	Result = recvfrom(Arguments[0].Integer, (char *)Arguments[1].Block, Arguments[2].Integer,
		Arguments[3].Integer, (sockaddr *)Arguments[4].Block, (socklen_t *)Arguments[5].Block);

	g_RpcErrno = errno;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_put_string(safe_box_t Parent, const char *Name, const char *Value);
bool RpcFunc_put_string(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Pointer) {
		return false;
	}

	Result = Box_put_string((box_t)Arguments[0].Pointer, FROM_RPC_STRING(Arguments[1]), FROM_RPC_STRING(Arguments[2]));

	g_RpcErrno = errno;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_put_integer(safe_box_t Parent, const char *Name, int Value);
bool RpcFunc_put_integer(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Pointer || Arguments[2].Type != Integer) {
		return false;
	}

	Result = Box_put_integer((box_t)Arguments[0].Pointer, FROM_RPC_STRING(Arguments[1]), Arguments[2].Integer);

	g_RpcErrno = errno;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// safe_box_t safe_put_box(safe_box_t Parent, const char *Name);
bool RpcFunc_put_box(Value_t *Arguments, Value_t *ReturnValue) {
	box_t Result;

	if (Arguments[0].Type != Pointer) {
		return false;
	}

	Result = Box_put_box((box_t)Arguments[0].Pointer, FROM_RPC_STRING(Arguments[1]));

	g_RpcErrno = errno;

	*ReturnValue = RPC_POINTER(Result);

	return true;
}

// int safe_remove(safe_box_t Parent, const char *Name);
bool RpcFunc_remove(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Pointer) {
		return false;
	}

	Result = Box_remove((box_t)Arguments[0].Pointer, FROM_RPC_STRING(Arguments[1]));

	g_RpcErrno = errno;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// const char *safe_get_string(safe_box_t Parent, const char *Name);
bool RpcFunc_get_string(Value_t *Arguments, Value_t *ReturnValue) {
	const char *Result;

	if (Arguments[0].Type != Pointer) {
		return false;
	}

	Result = Box_get_string((box_t)Arguments[0].Pointer, FROM_RPC_STRING(Arguments[1]));

	g_RpcErrno = errno;

	*ReturnValue = RPC_STRING(Result);

	return true;
}

// int safe_get_integer(safe_box_t Parent, const char *Name);
bool RpcFunc_get_integer(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Pointer) {
		return false;
	}

	Result = Box_get_integer((box_t)Arguments[0].Pointer, FROM_RPC_STRING(Arguments[1]));

	g_RpcErrno = errno;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// safe_box_t safe_get_box(safe_box_t Parent, const char *Name);
bool RpcFunc_get_box(Value_t *Arguments, Value_t *ReturnValue) {
	box_t Result;

	if (Arguments[0].Type != Pointer) {
		return false;
	}

	Result = Box_get_box((box_t)Arguments[0].Pointer, FROM_RPC_STRING(Arguments[1]));

	g_RpcErrno = errno;

	*ReturnValue = RPC_POINTER(Result);

	return true;
}

// int safe_enumerate(safe_box_t Parent, safe_element_t **Previous, char *Name, int Len);
bool RpcFunc_enumerate(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Pointer || Arguments[1].Type != Block ||
		Arguments[2].Type != Block || Arguments[3].Type != Integer) {
		return false;
	}

	Result = Box_enumerate((box_t)Arguments[0].Pointer, (element_t **)Arguments[1].Block,
		(char *)Arguments[2].Block, Arguments[2].Integer);

	g_RpcErrno = errno;

	Arguments[1].Flags = Flag_Out;
	Arguments[2].Flags = Flag_Out;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// int safe_rename(safe_box_t Parent, const char *OldName, const char *NewName);
bool RpcFunc_rename(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Pointer) {
		return false;
	}

	Result = Box_rename((box_t)Arguments[0].Pointer, FROM_RPC_STRING(Arguments[1]),
		RpcStringFromValue(Arguments[2]));

	g_RpcErrno = errno;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// safe_box_t safe_get_parent(safe_box_t Box);
bool RpcFunc_get_parent(Value_t *Arguments, Value_t *ReturnValue) {
	box_t Result;

	if (Arguments[0].Type != Pointer) {
		return false;
	}

	Result = Box_get_parent((box_t)Arguments[0].Pointer);

	g_RpcErrno = errno;

	*ReturnValue = RPC_POINTER(Result);

	return true;
}

// const char *safe_get_name(safe_box_t Box);
bool RpcFunc_get_name(Value_t *Arguments, Value_t *ReturnValue) {
	const char *Result;

	if (Arguments[0].Type != Pointer) {
		return false;
	}

	Result = Box_get_name((box_t)Arguments[0].Pointer);

	g_RpcErrno = errno;

	*ReturnValue = RPC_STRING(Result);

	return true;
}

// int safe_move(safe_box_t NewParent, safe_box_t Box, const char *NewName);
bool RpcFunc_move(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Pointer || Arguments[1].Type != Pointer) {
		return false;
	}

	Result = Box_move((box_t)Arguments[0].Pointer, (box_t)Arguments[1].Pointer, FROM_RPC_STRING(Arguments[2]));

	g_RpcErrno = errno;

	*ReturnValue = RPC_INT(Result);

	return true;
}

// void safe_exit(int ExitCode);
bool RpcFunc_exit(Value_t *Arguments, Value_t *ReturnValue) {
	box_t Result;

	if (Arguments[0].Type != Integer) {
		exit(0);
	} else {
		exit(Arguments[0].Integer);
	}

	return false;
}

#endif

#ifdef RPCCLIENT

int safe_socket(int Domain, int Type, int Protocol) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Domain);
	Arguments[1] = RPC_INT(Type);
	Arguments[2] = RPC_INT(Protocol);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_socket, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_getpeername(int Socket, sockaddr *Sockaddr, socklen_t *Len) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_BLOCK(Sockaddr, *Len, Flag_Out);
	Arguments[2] = RPC_BLOCK(Len, sizeof(Len), Flag_Out);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_getpeername, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	if (ReturnValue.Integer == 0) {
		memcpy(Sockaddr, Arguments[1].Block, *Len);
		memcpy(Len, Arguments[2].Block, sizeof(Arguments[2].Integer));
	}

	RpcFreeValue(Arguments[1]);
	RpcFreeValue(Arguments[2]);

	return ReturnValue.Integer;
}

int safe_getsockname(int Socket, sockaddr *Sockaddr, socklen_t *Len) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_BLOCK(Sockaddr, *Len, Flag_Out);
	Arguments[2] = RPC_BLOCK(Len, sizeof(Len), Flag_Out);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_getsockname, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	if (ReturnValue.Integer == 0) {
		memcpy(Sockaddr, Arguments[1].Block, *Len);
		memcpy(Len, Arguments[2].Block, sizeof(Arguments[2].Integer));
	}

	RpcFreeValue(Arguments[1]);
	RpcFreeValue(Arguments[2]);

	return ReturnValue.Integer;
}

int safe_bind(int Socket, const sockaddr *Sockaddr, socklen_t Len) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_BLOCK(Sockaddr, Len, Flag_None);
	Arguments[2] = RPC_INT(Len);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_bind, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_connect(int Socket, const sockaddr *Sockaddr, socklen_t Len) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_BLOCK(Sockaddr, Len, Flag_None);
	Arguments[2] = RPC_INT(Len);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_connect, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_listen(int Socket, int Backlog) {
	Value_t Arguments[2];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_INT(Backlog);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_listen, Arguments, 2, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_accept(int Socket, sockaddr *Sockaddr, socklen_t *Len) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_BLOCK(Sockaddr, *Len, Flag_Out);
	Arguments[2] = RPC_BLOCK(Len, sizeof(Len), Flag_Out);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_accept, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	RpcFreeValue(Arguments[1]);
	RpcFreeValue(Arguments[2]);

	return ReturnValue.Integer;
}

int safe_poll(struct pollfd *Sockets, int Nfds, int Timeout) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	Arguments[0] = RPC_BLOCK(Sockets, Nfds * sizeof(pollfd), Flag_Out);
	Arguments[1] = RPC_INT(Nfds);
	Arguments[2] = RPC_INT(Timeout);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_poll, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	if (ReturnValue.Integer >= 0) {
		memcpy(Sockets, Arguments[0].Block, Nfds * sizeof(pollfd));
	}

	RpcFreeValue(Arguments[0]);

	return ReturnValue.Integer;
}

int safe_recv(int Socket, void *Buffer, size_t Size, int Flags) {
	Value_t Arguments[4];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_BLOCK(Buffer, Size, Flag_Out | Flag_Alloc);
	Arguments[2] = RPC_INT(Size);
	Arguments[3] = RPC_INT(Flags);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_recv, Arguments, 4, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	if (ReturnValue.Integer > 0) {
		memcpy(Buffer, Arguments[1].Block, ReturnValue.Integer);
	}

	RpcFreeValue(Arguments[1]);

	return ReturnValue.Integer;
}

int safe_send(int Socket, const void *Buffer, size_t Size, int Flags) {
	Value_t Arguments[4];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_BLOCK(Buffer, Size, Flag_None);
	Arguments[2] = RPC_INT(Size);
	Arguments[3] = RPC_INT(Flags);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_send, Arguments, 4, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_shutdown(int Socket, int How) {
	Value_t Arguments[2];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_INT(How);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_shutdown, Arguments, 2, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_closesocket(int Socket) {
	Value_t Arguments[1];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_closesocket, Arguments, 1, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_getsockopt(int Socket, int Level, int OptName, char *OptVal, socklen_t *OptLen) {
	Value_t Arguments[5];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_INT(Level);
	Arguments[2] = RPC_INT(OptName);
	Arguments[3] = RPC_BLOCK(OptVal, *OptLen, Flag_Out | Flag_Alloc);
	Arguments[4] = RPC_BLOCK(OptLen, sizeof(int), Flag_Out);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_getsockopt, Arguments, 5, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	memcpy(OptLen, Arguments[4].Block, sizeof(int));
	memcpy(OptVal, Arguments[3].Block, *OptLen);

	RpcFreeValue(Arguments[3]);
	RpcFreeValue(Arguments[4]);

	return ReturnValue.Integer;
}

int safe_setsockopt(int Socket, int Level, int OptName, const char *OptVal, socklen_t OptLen) {
	Value_t Arguments[5];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_INT(Level);
	Arguments[2] = RPC_INT(OptName);
	Arguments[3] = RPC_BLOCK(OptVal, OptLen, Flag_None);
	Arguments[4] = RPC_INT(OptLen);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_setsockopt, Arguments, 5, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_ioctlsocket(int Socket, long Command, unsigned long *ArgP) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_INT(Command);
	Arguments[2] = RPC_BLOCK(ArgP, sizeof(unsigned long), Flag_Out);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_ioctlsocket, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	RpcFreeValue(Arguments[2]);

	return ReturnValue.Integer;
}

int safe_errno(void) {
	Value_t ReturnValue;

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_errno, NULL, 0, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_print(const char *Line) {
	Value_t Arguments[1];
	Value_t ReturnValue;

	Arguments[0] = RPC_BLOCK(Line, strlen(Line) + 1, Flag_None);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_print, Arguments, 1, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_scan(char *Buffer, size_t Size) {
	Value_t Arguments[2];
	Value_t ReturnValue;

	Arguments[0] = RPC_BLOCK(Buffer, Size, Flag_Out | Flag_Alloc);
	Arguments[1] = RPC_INT(Size);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_scan, Arguments, 2, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	if (ReturnValue.Integer > 0) {
		memcpy(Buffer, Arguments[0].Block, ReturnValue.Integer);
	}

	RpcFreeValue(Arguments[0]);

	return ReturnValue.Integer;
}

int safe_scan_passwd(char *Buffer, size_t Size) {
	Value_t Arguments[2];
	Value_t ReturnValue;

	Arguments[0] = RPC_BLOCK(Buffer, Size, Flag_Out | Flag_Alloc);
	Arguments[1] = RPC_INT(Size);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_scan_passwd, Arguments, 2, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	if (ReturnValue.Integer > 0) {
		memcpy(Buffer, Arguments[0].Block, ReturnValue.Integer);
	}

	RpcFreeValue(Arguments[0]);

	return ReturnValue.Integer;	
}

size_t safe_sendto(int Socket, const void *Buffer, size_t Len, int Flags, const struct sockaddr *To, socklen_t ToLen) {
	Value_t Arguments[6];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_BLOCK(Buffer, Len, Flag_None);
	Arguments[2] = RPC_INT(Len);
	Arguments[3] = RPC_INT(Flags);
	Arguments[4] = RPC_BLOCK(To, ToLen, Flag_None);
	Arguments[5] = RPC_INT(ToLen);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_sendto, Arguments, 6, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	RpcFreeValue(Arguments[1]);
	RpcFreeValue(Arguments[4]);

	return ReturnValue.Integer;
}

size_t safe_recvfrom(int Socket, void *Buffer, size_t Len, int Flags, struct sockaddr *From, socklen_t *FromLen) {
	Value_t Arguments[6];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(Socket);
	Arguments[1] = RPC_BLOCK(Buffer, Len, Flag_Out | Flag_Alloc);
	Arguments[2] = RPC_INT(Len);
	Arguments[3] = RPC_INT(Flags);
	Arguments[4] = RPC_BLOCK(From, *FromLen, Flag_Out | Flag_Alloc);
	Arguments[5] = RPC_BLOCK(FromLen, sizeof(socklen_t), Flag_Out);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_sendto, Arguments, 6, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	if (ReturnValue.Integer > 0) {
		memcpy(Buffer, Arguments[1].Block, ReturnValue.Integer);
		memcpy(FromLen, Arguments[5].Block, sizeof(socklen_t));
		memcpy(From, Arguments[4].Block, *FromLen);
	}

	RpcFreeValue(Arguments[1]);
	RpcFreeValue(Arguments[4]);
	RpcFreeValue(Arguments[5]);

	return ReturnValue.Integer;
}

int safe_printf(const char *Format, ...) {
	int Result;
	char *Out;
	va_list marker;

	va_start(marker, Format);
	vasprintf(&Out, Format, marker);
	va_end(marker);

	if (Out != NULL) {
		Result = safe_print(Out);

		free(Out);
	}

	return Result;
}

int safe_put_string(safe_box_t Parent, const char *Name, const char *Value) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	if (Value == NULL) {
		return 0;
	}

	Arguments[0] = RPC_POINTER(Parent);
	Arguments[1] = RPC_STRING(Name);
	Arguments[2] = RPC_STRING(Value);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_put_string, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

int safe_put_integer(safe_box_t Parent, const char *Name, int Value) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	Arguments[0] = RPC_POINTER(Parent);
	Arguments[1] = RPC_STRING(Name);
	Arguments[2] = RPC_INT(Value);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_put_integer, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

safe_box_t safe_put_box(safe_box_t Parent, const char *Name) {
	Value_t Arguments[2];
	Value_t ReturnValue;

	Arguments[0] = RPC_POINTER(Parent);
	Arguments[1] = RPC_STRING(Name);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_put_box, Arguments, 2, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Pointer) {
		RpcFatal();
	}

	return ReturnValue.Pointer;
}

int safe_remove(safe_box_t Parent, const char *Name) {
	Value_t Arguments[2];
	Value_t ReturnValue;

	if (Name == NULL) {
		return 0;
	}

	Arguments[0] = RPC_POINTER(Parent);
	Arguments[1] = RPC_STRING(Name);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_remove, Arguments, 2, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

const char *safe_get_string(safe_box_t Parent, const char *Name) {
	Value_t Arguments[2];
	static Value_t ReturnValue = {};

	RpcFreeValue(ReturnValue);

	Arguments[0] = RPC_POINTER(Parent);
	Arguments[1] = RPC_STRING(Name);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_get_string, Arguments, 2, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Block && ReturnValue.Type != Integer) {
		RpcFatal();
	}

	if (ReturnValue.Type == Block) {
		return (const char *)ReturnValue.Block;
	} else {
		return 0;
	}
}

int safe_get_integer(safe_box_t Parent, const char *Name) {
	Value_t Arguments[2];
	Value_t ReturnValue;

	Arguments[0] = RPC_POINTER(Parent);
	Arguments[1] = RPC_STRING(Name);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_get_integer, Arguments, 2, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

safe_box_t safe_get_box(safe_box_t Parent, const char *Name) {
	Value_t Arguments[2];
	Value_t ReturnValue;

	Arguments[0] = RPC_POINTER(Parent);
	Arguments[1] = RPC_STRING(Name);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_get_box, Arguments, 2, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Pointer) {
		RpcFatal();
	}

	return (safe_box_t)ReturnValue.Pointer;
}

int safe_enumerate(safe_box_t Parent, safe_element_t **Previous, char *Name, int Len) {
	Value_t Arguments[4];
	Value_t ReturnValue;

	Arguments[0] = RPC_POINTER(Parent);
	Arguments[1] = RPC_BLOCK(Previous, sizeof(safe_element_t *), Flag_Out);
	Arguments[2] = RPC_BLOCK(Name, Len, Flag_Out | Flag_Alloc);
	Arguments[3] = RPC_INT(Len);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_enumerate, Arguments, 4, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	memcpy(Previous, Arguments[1].Block, sizeof(safe_element_t *));
	memcpy(Name, Arguments[2].Block, Arguments[2].Size);

	return ReturnValue.Integer;
}

int safe_rename(safe_box_t Parent, const char *OldName, const char *NewName) {
	Value_t Arguments[3];
	Value_t ReturnValue;

	Arguments[0] = RPC_POINTER(Parent);
	Arguments[1] = RPC_STRING(OldName);
	Arguments[2] = RPC_STRING(NewName);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_rename, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

safe_box_t safe_get_parent(safe_box_t Box) {
	Value_t Arguments[1];
	Value_t ReturnValue;

	Arguments[0] = RPC_POINTER(Box);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_get_parent, Arguments, 1, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Pointer) {
		RpcFatal();
	}

	return ReturnValue.Pointer;
}

const char *safe_get_name(safe_box_t Box) {
	Value_t Arguments[1];
	static Value_t ReturnValue;

	RpcFreeValue(ReturnValue);

	Arguments[0] = RPC_POINTER(Box);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_get_name, Arguments, 1, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Block && ReturnValue.Type != Pointer) {
		RpcFatal();
	}

	if (ReturnValue.Type == Block) {
		return (const char *)ReturnValue.Block;
	} else {
		return (const char *)ReturnValue.Pointer;
	}
}

int safe_move(safe_box_t NewParent, safe_box_t Box, const char *NewName) {
	Value_t Arguments[3];
	static Value_t ReturnValue;

	Arguments[0] = RPC_POINTER(NewParent);
	Arguments[1] = RPC_POINTER(Box);
	Arguments[2] = RPC_STRING(NewName);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_move, Arguments, 3, &ReturnValue)) {
		RpcFatal();
	}

	if (ReturnValue.Type != Integer) {
		RpcFatal();
	}

	return ReturnValue.Integer;
}

void safe_exit(int ExitCode) {
	Value_t Arguments[1];
	Value_t ReturnValue;

	Arguments[0] = RPC_INT(ExitCode);

	if (!RpcInvokeFunction(GetStdHandle(STD_INPUT_HANDLE), GetStdHandle(STD_OUTPUT_HANDLE), Function_safe_exit, Arguments, 1, &ReturnValue)) {
		RpcFatal();
	}

	exit(ExitCode);
}

#endif
