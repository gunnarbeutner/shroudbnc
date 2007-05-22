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

/*

RPC protocol:

-> <function number><value 1><value 2>...
<- <value>

<function number> is an 8-bit value

<value> consists of:

<value type><actual value>

value types (8-bit integer):

0 - 32-bit integer
1 - block

type 1 value consists of a 32-bit value describing the length
of the block, followed by a number of bytes

*/

#ifdef RPCSERVER
static struct {
	Function_t Function;
	unsigned int ArgumentCount;
	bool (*RealFunction)(Value_t *Arguments, Value_t *ReturnValue);
} functions[] = {
	{ Function_safe_socket,			3,	RpcFunc_socket		},
	{ Function_safe_getpeername,	3,	RpcFunc_getpeername	},
	{ Function_safe_getsockname,	3,	RpcFunc_getsockname	},
	{ Function_safe_bind,			3,	RpcFunc_bind		},
	{ Function_safe_connect,		3,	RpcFunc_connect		},
	{ Function_safe_listen,			2,	RpcFunc_listen		},
	{ Function_safe_accept,			3,	RpcFunc_accept		},
	{ Function_safe_poll,			3,	RpcFunc_poll		},
	{ Function_safe_recv,			4,	RpcFunc_recv		},
	{ Function_safe_send,			4,	RpcFunc_send		},
	{ Function_safe_shutdown,		2,	RpcFunc_shutdown	},
	{ Function_safe_closesocket,	1,	RpcFunc_closesocket	},
	{ Function_safe_getsockopt,		5,	RpcFunc_getsockopt	},
	{ Function_safe_setsockopt,		5,	RpcFunc_setsockopt	},
	{ Function_safe_ioctlsocket,	3,	RpcFunc_ioctlsocket	},
	{ Function_safe_errno,			0,	RpcFunc_errno		},
	{ Function_safe_print,			1,	RpcFunc_print		},
	{ Function_safe_scan,			2,	RpcFunc_scan		},
	{ Function_safe_scan_passwd,	2,	RpcFunc_scan_passwd	},
	{ Function_safe_sendto,			6,	RpcFunc_sendto		},
	{ Function_safe_recvfrom,		6,	RpcFunc_recvfrom	},
	{ Function_safe_put_string,		3,	RpcFunc_put_string	},
	{ Function_safe_put_integer,	3,	RpcFunc_put_integer	},
	{ Function_safe_put_box,		2,	RpcFunc_put_box		},
	{ Function_safe_remove,			2,	RpcFunc_remove		},
	{ Function_safe_get_string,		2,	RpcFunc_get_string	},
	{ Function_safe_get_integer,	2,	RpcFunc_get_integer	},
	{ Function_safe_get_box,		2,	RpcFunc_get_box		},
	{ Function_safe_enumerate,		5,	RpcFunc_enumerate	},
	{ Function_safe_exit,			1,	RpcFunc_exit		}
};
#endif

#ifndef _WIN32
int GetStdHandle(int Handle) {
	return Handle;
}

int ReadFile(int File, void *Buffer, int Size, int *Read, void *Dummy) {
	int Result;

	if (Size == 0) {
		*Read = 0;

		return 1;
	}

	errno = 0;

	Result = read(File, Buffer, Size);

	if (errno == EINTR) {
		*Read = 0;

		return 1;
	}

	if (Result <= 0) {
		return 0;
	} else {
		*Read = Result;

		return 1;
	}
}

int WriteFile(int File, const void *Buffer, int Size, int *Written, void *Dummy) {
	size_t Offset = 0;
	int Result;

	if (Size == 0) {
		*Written = 0;

		return 1;
	}

	while (Offset < Size) {
		errno = 0;

		Result = write(File, (char *)Buffer + Offset, Size - Offset);

		if (errno == EINTR) {
			continue;
		}

		if (Result <= 0) {
			return 0;
		} else {
			Offset += Result;
		}
	}

	*Written = Offset;

	return 1;
}
#endif


bool RpcWriteValue(PIPE Pipe, Value_t Value) {
	char ReturnType;
	unsigned int ReturnSize;
	char ReturnFlags;
	DWORD Dummy;

	ReturnType = Value.Type;

	if (!WriteFile(Pipe, &ReturnType, sizeof(ReturnType), &Dummy, NULL)) {
		return false;
	}

	if (ReturnType == Integer) {
		if (!WriteFile(Pipe, &(Value.Integer), sizeof(Value.Integer), &Dummy, NULL)) {
			return false;
		}
	} else if (ReturnType == Pointer) {
		if (!WriteFile(Pipe, &(Value.Pointer), sizeof(Value.Pointer), &Dummy, NULL)) {
			return false;
		}
	} else if (ReturnType == Block) {
		ReturnFlags = Value.Flags;

		if (!WriteFile(Pipe, &ReturnFlags, sizeof(ReturnFlags), &Dummy, NULL)) {
			return false;
		}

		ReturnSize = Value.Size;

		if (!WriteFile(Pipe, &ReturnSize, sizeof(ReturnSize), &Dummy, NULL)) {
			return false;
		}

		if (!(Value.Flags & Flag_Alloc)) {
			if (!WriteFile(Pipe, Value.Block, ReturnSize, &Dummy, NULL)) {
				return false;
			}
		}
	}

	return true;
}

bool RpcBlockingRead(PIPE Pipe, void *Buffer, int Size) {
	int Offset = 0;
	DWORD Read;

	while (ReadFile(Pipe, (char *)Buffer + Offset, Size - Offset, &Read, NULL)) {
		Offset += Read;

		if (Offset == Size) {
			return true;
		}
	}

	return false;
}

bool RpcReadValue(PIPE Pipe, Value_t *Value) {
	char ValueType;
	char *Buffer;

	if (!RpcBlockingRead(Pipe, &ValueType, sizeof(char))) {
		return false;
	}

	Value->Type = (Type_t)ValueType;

	if (ValueType == Integer) {
		if (!RpcBlockingRead(Pipe, &(Value->Integer), sizeof(Value->Integer))) {
			return false;
		}

		Value->NeedFree = false;
	} else if (ValueType == Pointer) {
		if (!RpcBlockingRead(Pipe, &(Value->Pointer), sizeof(Value->Pointer))) {
			return false;
		}

		Value->NeedFree = false;
	} else if (ValueType == Block) {
		if (!RpcBlockingRead(Pipe, &(Value->Flags), sizeof(Value->Flags))) {
			return false;
		}

		if (!RpcBlockingRead(Pipe, &Value->Size, sizeof(Value->Size))) {
			return false;
		}

		Buffer = (char *)malloc(Value->Size);

		if (Buffer == NULL) {
			return false;
		}

		if (!(Value->Flags & Flag_Alloc)) {
			if (!RpcBlockingRead(Pipe, Buffer, Value->Size)) {
				free(Buffer);

				return false;
			}
		}

		Value->Block = Buffer;
		Value->NeedFree = true;
	}

	return true;
}

void RpcFreeValue(Value_t Value) {
	if (Value.Type == Block && Value.NeedFree) {
		free(Value.Block);
	}
}

void RpcFatal(void) {
	exit(1);
}

#ifdef RPCSERVER

int RpcInvokeClient(char *Program, PipePair_t *Pipes) {
#ifdef _WIN32
	HANDLE hChildStdinRd, hChildStdinWr, hChildStdinWrDup, 
		hChildStdoutRd, hChildStdoutWr, hChildStdoutRdDup, 
		hStdout; 
	PROCESS_INFORMATION piProcInfo; 
	STARTUPINFO siStartInfo;
	BOOL bFuncRetn = FALSE;
	SECURITY_ATTRIBUTES saAttr;
	BOOL fSuccess;
	char *CommandLine;

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL; 

	hStdout = GetStdHandle(STD_OUTPUT_HANDLE); 

	if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0)) {
		return 0;
	}

	fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdoutRd,
		GetCurrentProcess(), &hChildStdoutRdDup , 0,
		FALSE,
		DUPLICATE_SAME_ACCESS);

	if (!fSuccess) {
		CloseHandle(hChildStdoutRd);
		return 0;
	}

	if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) {
		return 0;
	}

	fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdinWr, 
		GetCurrentProcess(), &hChildStdinWrDup, 0, 
		FALSE,
		DUPLICATE_SAME_ACCESS); 

	if (!fSuccess) {
		return 0;
	}

	CloseHandle(hChildStdinWr); 

	memset(&piProcInfo, 0, sizeof(piProcInfo));
	memset(&siStartInfo, 0, sizeof(siStartInfo));

	siStartInfo.cb = sizeof(STARTUPINFO); 
	siStartInfo.hStdError = hChildStdoutWr;
	siStartInfo.hStdOutput = hChildStdoutWr;
	siStartInfo.hStdInput = hChildStdinRd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	CommandLine = (char *)malloc(strlen(Program) + 50);

	if (CommandLine == NULL) {
		return 0;
	}

	strcpy(CommandLine, Program);
	strcat(CommandLine, " --rpc-child");

	bFuncRetn = CreateProcess(NULL, 
					CommandLine,   // command line 
					NULL,          // process security attributes 
					NULL,          // primary thread security attributes 
					TRUE,          // handles are inherited 
					0,             // creation flags 
					NULL,          // use parent's environment 
					NULL,          // use parent's current directory 
					&siStartInfo,  // STARTUPINFO pointer 
					&piProcInfo);  // receives PROCESS_INFORMATION 

	free(CommandLine);

	if (bFuncRetn == 0) {
		return false;
	} else {
		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);

		Pipes->In = hChildStdoutRdDup;
		Pipes->Out = hChildStdinWrDup;

		return 1;
	}
#else
	int pid;
	int stdinpipes[2];
	int stdoutpipes[2];

	pipe(stdinpipes);
	pipe(stdoutpipes);

	pid = fork();

	if (pid == 0) {
		// child

		close(stdinpipes[1]);
		close(stdoutpipes[0]);

		dup2(stdinpipes[0], 0);
		close(stdinpipes[0]);
		dup2(stdoutpipes[1], 1);
		close(stdoutpipes[1]);

		execlp(Program, Program, "--rpc-child", (char *) NULL);
		exit(0);
	} else if (pid > 0) {
		close(stdinpipes[0]);
		close(stdoutpipes[1]);

		Pipes->In = stdoutpipes[0];
		Pipes->Out = stdinpipes[1];

		return 1;
	} else {
		close(stdinpipes[0]);
		close(stdinpipes[1]);
		close(stdoutpipes[0]);
		close(stdoutpipes[1]);

		return 0;
	}
#endif
}

int RpcRunServer(PipePair_t Pipes) {
	const size_t BlockSize = 512;
	char *Buffer, *NewBuffer;
	int AllocedSize;
	int Size = 0;
	int Result;
	int ReadOffset = 0;
	DWORD Read;

	AllocedSize = BlockSize;
	Buffer = (char *)malloc(AllocedSize);

	if (Buffer == NULL) {
		return 0;
	}

	while (ReadFile(Pipes.In, Buffer + Size, AllocedSize - Size, &Read, NULL)) {
		Size += Read;

		Result = RpcProcessCall(Buffer + ReadOffset, Size - ReadOffset, Pipes.Out);

		if (Result > 0) {
			ReadOffset += Result;
		} else if (Result < 0) {
			return 1;
		}

		if (ReadOffset < BlockSize) {
			AllocedSize = (Size / BlockSize + 1) * BlockSize;
			Buffer = (char *)realloc(Buffer, AllocedSize);

			if (Buffer == NULL) {
				return 0;
			}
		} else{
			AllocedSize = ((Size - ReadOffset) / BlockSize + 1) * BlockSize;
			NewBuffer = (char *)malloc(AllocedSize);

			if (NewBuffer == NULL) {
				free(Buffer);
				
				return 0;
			}

			memcpy(NewBuffer, Buffer + ReadOffset, Size - ReadOffset);
			Size -= ReadOffset;
			Buffer = NewBuffer;
			ReadOffset = 0;
		}

	}

	return 1;
}

#define RpcExpect(Bytes)				\
	{									\
		if (Length < Bytes) {			\
			if (Arguments != NULL) {	\
				free(Arguments);		\
			}							\
										\
			return 0;					\
		}								\
										\
		Length -= Bytes;				\
	}

/*

TODO: fix alignment issues

*/
int RpcProcessCall(char *Data, size_t Length, PIPE Out) {
	Function_t Function;
	Type_t ArgumentType;
	char *Call = Data;
	Value_t *Arguments = NULL;
	Value_t ReturnValue;
	DWORD Written;
	int CID;

	RpcExpect(sizeof(int));
	CID = *(int *)Call;
	Call += sizeof(int);

	RpcExpect(sizeof(char));
	Function = (Function_t)*Call;
	Call += sizeof(char);

	if (Function >= last_function) {
		return -1;
	}

	Arguments = (Value_t *)malloc(sizeof(Value_t) * functions[Function].ArgumentCount);

	for (unsigned int i = 0; i < functions[Function].ArgumentCount; i++) {
		RpcExpect(sizeof(char));
		ArgumentType = (Type_t)*Call;
		Call += sizeof(char);

		Arguments[i].Type = ArgumentType;

		if (ArgumentType == Integer) {
			Arguments[i].Flags = Flag_None;

			RpcExpect(4);
			Arguments[i].Integer = *(int *)Call;
			Call += sizeof(int);
		} else if (ArgumentType == Pointer) {
			Arguments[i].Flags = Flag_None;

			RpcExpect(sizeof(void *));
			Arguments[i].Pointer = *(void **)Call;
			Call += sizeof(void *);
		} else if (ArgumentType == Block) {
			RpcExpect(sizeof(char));
			Arguments[i].Flags = *Call;
			Call += sizeof(char);

			RpcExpect(sizeof(unsigned int));
			Arguments[i].Size = *(unsigned int *)Call;
			Call += sizeof(unsigned int);

			if (Arguments[i].Flags & Flag_Alloc) {
				Arguments[i].Block = malloc(Arguments[i].Size);

				if (Arguments[i].Block == NULL) {
					return -1;
				}
			} else {
				RpcExpect(Arguments[i].Size);
				Arguments[i].Block = Call;
				Call += Arguments[i].Size;
			}
		}
	}

	errno = 0;

	if (!WriteFile(Out, &CID, sizeof(CID), &Written, NULL)) {
		return -1;
	}

	if (functions[Function].RealFunction(Arguments, &ReturnValue)) {
		for (unsigned int i = 0; i < functions[Function].ArgumentCount; i++) {
			if (Arguments[i].Flags & Flag_Out) {
				if (!RpcWriteValue(Out, Arguments[i])) {
					return -1;
				}
			}

			if (Arguments[i].Flags & Flag_Alloc) {
				free(Arguments[i].Block);
			}
		}

		if (!RpcWriteValue(Out, ReturnValue)) {
			return -1;
		}
	}

	free(Arguments);

	return Call - Data;
}

#endif

#ifdef RPCCLIENT
int RpcInvokeFunction(PIPE PipeIn, PIPE PipeOut, Function_t Function, Value_t *Arguments, unsigned int ArgumentCount, Value_t *ReturnValue) {
	char FunctionByte;
	int CID, CIDReturn;
	DWORD Dummy;

	FunctionByte = Function;

	CID = rand();

	if (!WriteFile(PipeOut, &CID, sizeof(CID), &Dummy, NULL)) {
		return 0;
	}

	if (!WriteFile(PipeOut, &FunctionByte, sizeof(char), &Dummy, NULL)) {
		return 0;
	}

	for (unsigned int i = 0; i < ArgumentCount; i++) {
		if (!RpcWriteValue(PipeOut, Arguments[i])) {
			return 0;
		}
	}

	if (!RpcBlockingRead(PipeIn, &CIDReturn, sizeof(CIDReturn))) {
		return 0;
	}

	if (CID != CIDReturn) {
		exit(200);
	}

	for (unsigned int i = 0; i < ArgumentCount; i++) {
		if (Arguments[i].Type == Block && Arguments[i].Flags & Flag_Out) {
			if (!RpcReadValue(PipeIn, &(Arguments[i]))) {
				return 0;
			}
		}
	}

	if (!RpcReadValue(PipeIn, ReturnValue)) {
		return 0;
	}

	return 1;
}

Value_t RpcBuildInteger(int Value) {
	Value_t Val;

	Val.Type = Integer;
	Val.Integer = Value;
	Val.Flags = Flag_None;

	return Val;
}

Value_t RpcBuildBlock(const void *Pointer, int Size, char Flag) {
	Value_t Val;

	Val.Type = Block;
	Val.Block = const_cast<void *>(Pointer);
	Val.Size = Size;
	Val.Flags = Flag;

	return Val;
}

Value_t RpcBuildPointer(const void *Ptr) {
	Value_t Val;

	Val.Type = Pointer;
	Val.Pointer = const_cast<void *>(Ptr);
	Val.Flags = Flag_None;

	return Val;
}

#endif
