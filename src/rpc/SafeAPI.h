#ifndef _SAFEAPI_H
#define _SAFEAPI_H

#ifdef _WIN32
#	ifdef RPCCLIENTBIN
#		define SAFEAPI __declspec(dllexport)
#	else
#		define SAFEAPI __declspec(dllimport)
#	endif
#else
#	define SAFEAPI
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
#endif

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

#ifdef RPCCLIENT
#ifdef __cplusplus
extern "C" {
#endif
	SOCKET SAFEAPI safe_socket(int Domain, int Type, int Protocol);
	int SAFEAPI safe_getpeername(SOCKET Socket, struct sockaddr *Sockaddr, socklen_t *Len);
	int SAFEAPI safe_getsockname(SOCKET Socket, struct sockaddr *Sockaddr, socklen_t *Len);
	int SAFEAPI safe_bind(SOCKET Socket, const struct sockaddr *Sockaddr, socklen_t Len);
	int SAFEAPI safe_connect(SOCKET Socket, const struct sockaddr *Sockaddr, socklen_t Len);
	int SAFEAPI safe_listen(int Socket, int Backlog);
	SOCKET SAFEAPI safe_accept(int Socket, struct sockaddr *Sockaddr, socklen_t *Len);
	int SAFEAPI safe_poll(struct pollfd *Sockets, int Nfds, int Timeout);
	int SAFEAPI safe_recv(SOCKET Socket, void *Buffer, size_t Size, int Flags);
	int SAFEAPI safe_send(SOCKET Socket, const void *Buffer, size_t Size, int Flags);
	int SAFEAPI safe_shutdown(SOCKET Socket, int How);
	int SAFEAPI safe_closesocket(SOCKET Socket);
	int SAFEAPI safe_getsockopt(SOCKET Socket, int Level, int OptName, char *OptVal, int *OptLen);
	int SAFEAPI safe_setsockopt(SOCKET Socket, int Level, int OptName, const char *OptVal, int OptLen);
	int SAFEAPI safe_ioctlsocket(SOCKET Socket, long Command, unsigned long *ArgP);

	int SAFEAPI safe_errno(void);
	int SAFEAPI safe_print(const char *Line);
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
#endif
