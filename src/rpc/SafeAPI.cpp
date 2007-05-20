#define _SAFEAPI
#include "../StdAfx.h"

#ifdef RPCSERVER

int g_RpcErrno = 0;

// SOCKET safe_socket(int Domain, int Type, int Protocol);
bool RpcFunc_socket(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Integer || Arguments[2].Type != Integer) {
		return false;
	}

	Result = socket(Arguments[0].Integer, Arguments[1].Integer, Arguments[2].Integer);

	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int safe_getpeername(SOCKET Socket, struct sockaddr *Sockaddr, socklen_t *Len);
bool RpcFunc_getpeername(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Block) {
		return false;
	}

	Result = getpeername(Arguments[0].Integer, (sockaddr *)Arguments[1].Block, (socklen_t *)Arguments[2].Block);

	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int safe_getsockname(SOCKET Socket, struct sockaddr *Sockaddr, socklen_t *Len);
bool RpcFunc_getsockname(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Block) {
		return false;
	}

	Result = getsockname(Arguments[0].Integer, (sockaddr *)Arguments[1].Block, (socklen_t *)Arguments[2].Block);

	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int safe_bind(SOCKET Socket, const struct sockaddr *Sockaddr, socklen_t Len);
bool RpcFunc_bind(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Integer) {
		return false;
	}

	Result = bind(Arguments[0].Integer, (sockaddr *)Arguments[1].Block, Arguments[2].Integer);

	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int safe_connect(SOCKET Socket, const struct sockaddr *Sockaddr, socklen_t Len);
bool RpcFunc_connect(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Integer) {
		return false;
	}

	Result = connect(Arguments[0].Integer, (sockaddr *)Arguments[1].Block, Arguments[2].Integer);

	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

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

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// SOCKET safe_accept(int Socket, sockaddr *Sockaddr, socklen_t *Len);
bool RpcFunc_accept(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Block) {
		return false;
	}

	Result = accept(Arguments[0].Integer, (sockaddr *)Arguments[1].Block, (socklen_t *)Arguments[2].Block);
	
	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

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

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int safe_recv(SOCKET Socket, void *Buffer, size_t Size, int Flags);
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

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int safe_send(SOCKET Socket, const void *Buffer, size_t Size, int Flags);
bool RpcFunc_send(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Block || Arguments[2].Type != Integer || Arguments[3].Type != Integer) {
		return false;
	}

	Result = send(Arguments[0].Integer, (char *)Arguments[1].Block, Arguments[2].Integer, Arguments[3].Integer);

	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

//int safe_shutdown(SOCKET Socket, int How);
bool RpcFunc_shutdown(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Integer) {
		return false;
	}

	Result = shutdown(Arguments[0].Integer, Arguments[1].Integer);

	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int safe_closesocket(SOCKET Socket);
bool RpcFunc_closesocket(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer) {
		return false;
	}

	Result = closesocket(Arguments[0].Integer);

	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int SAFEAPI safe_getsockopt(SOCKET Socket, int Level, int OptName, char *OptVal, int *OptLen);
bool RpcFunc_getsockopt(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer) {
		return false;
	}

	Result = getsockopt(Arguments[0].Integer, Arguments[1].Integer, Arguments[2].Integer, (char *)Arguments[3].Block, (int *)Arguments[4].Block);

	g_RpcErrno = WSAGetLastError();

	Arguments[3].Flags = Flag_Out;

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int SAFEAPI safe_setsockopt(SOCKET Socket, int Level, int OptName, const char *OptVal, int OptLen);
bool RpcFunc_setsockopt(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Integer || Arguments[2].Type != Integer ||
			Arguments[3].Type != Block || Arguments[4].Type != Integer) {
		return false;
	}

	Result = setsockopt(Arguments[0].Integer, Arguments[1].Integer, Arguments[2].Integer, (const char *)Arguments[3].Block, Arguments[4].Integer);

	Arguments[4].Flags = Flag_Out;

	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int SAFEAPI safe_ioctlsocket(SOCKET Socket, long Command, unsigned long *ArgP);
bool RpcFunc_ioctlsocket(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Integer || Arguments[1].Type != Integer || Arguments[2].Type != Block) {
		return false;
	}

	Result = ioctlsocket(Arguments[0].Integer, Arguments[1].Integer, (unsigned long *)Arguments[2].Block);

	Arguments[2].Flags = Flag_Out;

	g_RpcErrno = WSAGetLastError();

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

// int safe_errno(void);
bool RpcFunc_errno(Value_t *Arguments, Value_t *ReturnValue) {
	ReturnValue->Type = Integer;
	ReturnValue->Integer = g_RpcErrno;

	return true;
}

bool RpcFunc_print(Value_t *Arguments, Value_t *ReturnValue) {
	int Result;

	if (Arguments[0].Type != Block) {
		return false;
	}

	Result = printf("%s\n", Arguments[0].Block);

	g_RpcErrno = errno;

	ReturnValue->Type = Integer;
	ReturnValue->Integer = Result;

	return true;
}

#endif

#ifdef RPCCLIENT

SOCKET safe_socket(int Domain, int Type, int Protocol) {
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

int safe_getpeername(SOCKET Socket, sockaddr *Sockaddr, socklen_t *Len) {
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

int safe_getsockname(SOCKET Socket, sockaddr *Sockaddr, socklen_t *Len) {
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

int safe_bind(SOCKET Socket, const sockaddr *Sockaddr, socklen_t Len) {
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

int safe_connect(SOCKET Socket, const sockaddr *Sockaddr, socklen_t Len) {
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

SOCKET safe_accept(int Socket, sockaddr *Sockaddr, socklen_t *Len) {
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

int safe_recv(SOCKET Socket, void *Buffer, size_t Size, int Flags) {
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

int safe_send(SOCKET Socket, const void *Buffer, size_t Size, int Flags) {
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

int safe_shutdown(SOCKET Socket, int How) {
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

int safe_closesocket(SOCKET Socket) {
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

int safe_getsockopt(SOCKET Socket, int Level, int OptName, char *OptVal, int *OptLen) {
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

	return ReturnValue.Integer;
}

int safe_setsockopt(SOCKET Socket, int Level, int OptName, const char *OptVal, int OptLen) {
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

int safe_ioctlsocket(SOCKET Socket, long Command, unsigned long *ArgP) {
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

#endif
