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

#ifndef _SAFEAPI_H
#define _SAFEAPI_H

#ifdef HAVE_POLL
#	include <sys/poll.h>
#else
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
#endif

#ifdef RPCSERVER
bool RpcFunc_socket(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_getpeername(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_getsockname(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_bind(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_connect(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_listen(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_accept(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_poll(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_recv(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_send(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_shutdown(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_closesocket(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_getsockopt(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_setsockopt(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_ioctlsocket(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_ioctl(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_errno(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_print(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_scan(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_scan_passwd(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_sendto(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_recvfrom(Value_t *Arguments, Value_t *ReturnValue);

bool RpcFunc_put_string(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_put_integer(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_put_box(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_remove(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_get_string(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_get_integer(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_get_box(Value_t *Arguments, Value_t *ReturnValue);
bool RpcFunc_enumerate(Value_t *Arguments, Value_t *ReturnValue);
#endif

typedef void *safe_box_t;

#ifdef __cplusplus
extern "C" {
#endif
	int RPCAPI safe_socket(int Domain, int Type, int Protocol);
	int RPCAPI safe_getpeername(int Socket, struct sockaddr *Sockaddr, socklen_t *Len);
	int RPCAPI safe_getsockname(int Socket, struct sockaddr *Sockaddr, socklen_t *Len);
	int RPCAPI safe_bind(int Socket, const struct sockaddr *Sockaddr, socklen_t Len);
	int RPCAPI safe_connect(int Socket, const struct sockaddr *Sockaddr, socklen_t Len);
	int RPCAPI safe_listen(int Socket, int Backlog);
	int RPCAPI safe_accept(int Socket, struct sockaddr *Sockaddr, socklen_t *Len);
	int RPCAPI safe_poll(struct pollfd *Sockets, int Nfds, int Timeout);
	int RPCAPI safe_recv(int Socket, void *Buffer, size_t Size, int Flags);
	int RPCAPI safe_send(int Socket, const void *Buffer, size_t Size, int Flags);
	int RPCAPI safe_shutdown(int Socket, int How);
	int RPCAPI safe_closesocket(int Socket);
	int RPCAPI safe_getsockopt(int Socket, int Level, int OptName, char *OptVal, socklen_t *OptLen);
	int RPCAPI safe_setsockopt(int Socket, int Level, int OptName, const char *OptVal, socklen_t OptLen);
	int RPCAPI safe_ioctlsocket(int Socket, long Command, unsigned long *ArgP);

	int RPCAPI safe_errno(void);
	int RPCAPI safe_print(const char *Line);

	int RPCAPI safe_scan(char *Buffer, size_t Size);
	int RPCAPI safe_scan_passwd(char *Buffer, size_t Size);
	size_t RPCAPI safe_sendto(int Socket, const void *Buffer, size_t Len, int Flags, const struct sockaddr *To, socklen_t ToLen);
	size_t RPCAPI safe_recvfrom(int Socket, void *Buffer, size_t Len, int Flags, struct sockaddr *From, socklen_t *FromLen);

	int RPCAPI safe_put_string(safe_box_t Parent, const char *Name, const char *Value);
	int RPCAPI safe_put_integer(safe_box_t Parent, const char *Name, int Value);
	safe_box_t RPCAPI safe_put_box(safe_box_t Parent, const char *Name);
	int RPCAPI safe_remove(safe_box_t Parent, const char *Name);
	const char RPCAPI *safe_get_string(safe_box_t Parent, const char *Name);
	int RPCAPI safe_get_integer(safe_box_t Parent, const char *Name);
	safe_box_t RPCAPI safe_get_box(safe_box_t Parent, const char *Name);
	int RPCAPI safe_enumerate(safe_box_t Parent, safe_box_t *Previous, char *Name, int Len);

	int RPCAPI safe_printf(const char *Format, ...);


#ifdef __cplusplus
}
#endif

/*
#	define socket _lame<>1!!
#	define getpeername _lame<>1!!
#	define getsockname _lame<>1!!
//#	define bind _lame<>1!!
#	define connect _lame<>1!!
#	define listen _lame<>1!!
#	define accept _lame<>1!!
#	define poll _lame<>1!!
#	define recv _lame<>1!!
#	define send _lame<>1!!
#	define shutdown _lame<>1!!
#	define closesocket _lame<>1!!
#	define getsockopt _lame<>1!!
#	define setsockopt _lame<>1!!
#	define ioctlsocket _lame<>1!!
#	define ioctl _lame<>1!!
#	define WSAGetLastError _lame<>1!!
#	define fcntl _lame<>1!!
#	define recvfrom _lame<>1!!
#	define sendto _lame<>1!!
*/
#endif
