#ifndef _RPCAPI_H
#define _RPCAPI_H

#if !defined(RPCSERVER) && !defined(RPCCLIENT)
#	define RPCCLIENT
#endif

#if defined(SBNC) && !defined(RPCCLIENTBIN)
#	define RPCCLIENTBIN
#endif

#ifdef _WIN32
typedef HANDLE PIPE;
#else
typedef int HANDLE;

HANDLE GetStdHandle(HANDLE Handle);
DWORD ReadFile(int File, void *Buffer, size_t Size, DWORD *Read, void *Dummy);
DWORD WriteFile(int File, const void *Buffer, size_t Size, DWORD *Written, void *Dummy);
#endif

typedef struct PipePair_s {
	PIPE In;
	PIPE Out;
} PipePair_t;

typedef enum Function_e {
	Function_safe_socket,
	Function_safe_getpeername,
	Function_safe_getsockname,
	Function_safe_bind,
	Function_safe_connect,
	Function_safe_listen,
	Function_safe_accept,
	Function_safe_poll,
	Function_safe_recv,
	Function_safe_send,
	Function_safe_shutdown,
	Function_safe_closesocket,
	Function_safe_getsockopt,
	Function_safe_setsockopt,
	Function_safe_ioctlsocket,
	Function_safe_errno,
	Function_safe_print,
	last_function
} Function_t;

typedef enum Type_e {
	Integer,
	Block
} Type_t;

#define Flag_None 0
#define Flag_Out 1
#define Flag_Alloc 2

typedef struct Value_s {
	Type_t Type;
	char Flags;
	int NeedFree;

	union {
		int Integer;

		struct {
			unsigned int Size;
			void *Block;
		};
	};
} Value_t;

#define RPC_INT(Int) RpcBuildInteger(Int)
#define RPC_BLOCK(Ptr, Size, Flag) RpcBuildBlock(Ptr, Size, Flag)

void RpcFreeValue(Value_t Value);
void RpcFatal(void);

#ifdef RPCSERVER
int RpcInvokeClient(char *Program, PipePair_t *Pipes);
int RpcRunServer(PipePair_t Pipes);
int RpcProcessCall(char *Data, size_t Length, PIPE Out);
#endif

#ifdef RPCCLIENT
int RpcInvokeFunction(PIPE PipeIn, PIPE PipeOut, Function_t Function, Value_t *Arguments, unsigned int ArgumentCount, Value_t *ReturnValue);
Value_t RpcBuildInteger(int Value);
Value_t RpcBuildBlock(const void *Pointer, size_t Size, char Flag);
#endif
#endif
