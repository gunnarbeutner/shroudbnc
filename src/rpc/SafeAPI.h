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
#endif

#ifdef __cplusplus
extern "C" {
#endif
	SOCKET RPCAPI safe_socket(int Domain, int Type, int Protocol);
	int RPCAPI safe_getpeername(SOCKET Socket, struct sockaddr *Sockaddr, socklen_t *Len);
	int RPCAPI safe_getsockname(SOCKET Socket, struct sockaddr *Sockaddr, socklen_t *Len);
	int RPCAPI safe_bind(SOCKET Socket, const struct sockaddr *Sockaddr, socklen_t Len);
	int RPCAPI safe_connect(SOCKET Socket, const struct sockaddr *Sockaddr, socklen_t Len);
	int RPCAPI safe_listen(int Socket, int Backlog);
	SOCKET RPCAPI safe_accept(int Socket, struct sockaddr *Sockaddr, socklen_t *Len);
	int RPCAPI safe_poll(struct pollfd *Sockets, int Nfds, int Timeout);
	int RPCAPI safe_recv(SOCKET Socket, void *Buffer, size_t Size, int Flags);
	int RPCAPI safe_send(SOCKET Socket, const void *Buffer, size_t Size, int Flags);
	int RPCAPI safe_shutdown(SOCKET Socket, int How);
	int RPCAPI safe_closesocket(SOCKET Socket);
	int RPCAPI safe_getsockopt(SOCKET Socket, int Level, int OptName, char *OptVal, socklen_t *OptLen);
	int RPCAPI safe_setsockopt(SOCKET Socket, int Level, int OptName, const char *OptVal, socklen_t OptLen);
	int RPCAPI safe_ioctlsocket(SOCKET Socket, long Command, unsigned long *ArgP);

	int RPCAPI safe_errno(void);
	int RPCAPI safe_print(const char *Line);

	int RPCAPI safe_scan(char *Buffer, size_t Size);
	int RPCAPI safe_scan_passwd(char *Buffer, size_t Size);
	size_t RPCAPI safe_sendto(SOCKET Socket, const void *Buffer, size_t Len, int Flags, const struct sockaddr *To, socklen_t ToLen);
	size_t RPCAPI safe_recvfrom(SOCKET Socket, void *Buffer, size_t Len, int Flags, struct sockaddr *From, socklen_t *FromLen);

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
