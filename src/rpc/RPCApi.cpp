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

static bool g_LPC = false;

static struct {
	Function_t Function;
	unsigned int ArgumentCount;
	int (*RealFunction)(Value_t *Arguments, Value_t *ReturnValue);
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
	{ Function_safe_enumerate,		4,	RpcFunc_enumerate	},
	{ Function_safe_rename,			3,	RpcFunc_rename		},
	{ Function_safe_get_parent,		1,	RpcFunc_get_parent	},
	{ Function_safe_get_name,		1,	RpcFunc_get_name	},
	{ Function_safe_move,			3,	RpcFunc_move		},
	{ Function_safe_set_ro,			2,	RpcFunc_set_ro		},
	{ Function_safe_reinit,			0,	RpcFunc_reinit		},
	{ Function_safe_daemonize,		0,	RpcFunc_daemonize	},
	{ Function_safe_exit,			1,	RpcFunc_exit		}
};

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

bool RpcWriteValue(FILE *Pipe, Value_t Value) {
	char ReturnType;
	char ReturnFlags;

	ReturnType = Value.Type;

	if (fwrite(&ReturnType, 1, sizeof(ReturnType), Pipe) <= 0) {
		return false;
	}

	if (ReturnType == Integer) {
		if (fwrite(&(Value.Integer), 1, sizeof(Value.Integer), Pipe) <= 0) {
			return false;
		}
	} else if (ReturnType == Pointer) {
		if (fwrite(&(Value.Pointer), 1, sizeof(Value.Pointer), Pipe) <= 0) {
			return false;
		}
	} else if (ReturnType == Block) {
		ReturnFlags = Value.Flags;

		if (fwrite(&ReturnFlags, 1, sizeof(ReturnFlags), Pipe) <= 0) {
			return false;
		}

		if (fwrite(&(Value.Size), 1, sizeof(Value.Size), Pipe) <= 0) {
			return false;
		}

		if (!(Value.Flags & Flag_Alloc)) {
			if (fwrite(Value.Block, 1, Value.Size, Pipe) <= 0 && Value.Size != 0) {
				return false;
			}
		}
	}

	return true;
}

bool RpcBlockingRead(FILE *Pipe, void *Buffer, int Size) {
/*	int Offset = 0;
	DWORD Read;*/

	if (fread(Buffer, 1, Size, Pipe) <= 0 && Size != 0) {
		return false;
	} else {
		return true;
	}

/*	while (ReadFile(Pipe, (char *)Buffer + Offset, Size - Offset, &Read, NULL) && (Read > 0 || Size - Offset == 0)) {
		Offset += Read;

		if (Offset == Size) {
			return true;
		}
	}

	return false;*/
}

bool RpcReadValue(FILE *Pipe, Value_t *Value) {
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
		char Flags;

		if (!RpcBlockingRead(Pipe, &Flags, sizeof(Flags))) {
			return false;
		}

		Value->Flags = Flags;

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

#ifdef _WIN32
static char *ArgVToString(int argc, char **argv, char *Additional) {
	char *Result;
	size_t Length = 0;
	size_t Offset = 0, PieceLength;

	for (unsigned int i = 0; i < argc; i++) {
		Length += strlen(argv[i]) + 1;
	}

	if (Additional != NULL) {
		Length += strlen(Additional) + 1;
	}

	Result = (char *)malloc(Length + 1);

	if (Result == NULL) {
		return NULL;
	}

	Result[0] = '\0';

	for (unsigned int i = 0; i < argc; i++) {
		PieceLength = strlen(argv[i]);

		memcpy(Result + Offset, argv[i], PieceLength);
		Result[Offset + PieceLength] = ' ';

		Offset += PieceLength + 1;
	}

	if (Additional != NULL) {
		memcpy(Result + Offset, Additional, PieceLength);
		Result[Offset + PieceLength] = ' ';

		Offset += PieceLength + 1;
	}

	return Result;
}
#endif

int RpcInvokeClient(char *Program, PipePair_t *PipesLocal, int argc, char **argv) {
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
	siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	siStartInfo.hStdOutput = hChildStdoutWr;
	siStartInfo.hStdInput = hChildStdinRd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	CommandLine = ArgVToString(argc, argv, "--rpc-child");

	if (CommandLine == NULL) {
		return 0;
	}

	bFuncRetn = CreateProcess(NULL, 
					CommandLine,      // command line 
					NULL,             // process security attributes 
					NULL,             // primary thread security attributes 
					TRUE,			  // handles are inherited 
					CREATE_SUSPENDED, // creation flags 
					NULL,             // use parent's environment 
					NULL,             // use parent's current directory 
					&siStartInfo,     // STARTUPINFO pointer 
					&piProcInfo);     // receives PROCESS_INFORMATION 

	CloseHandle(hChildStdoutWr);
	CloseHandle(hChildStdinRd);

	free(CommandLine);

	if (bFuncRetn == 0) {
		return 0;
	} else {
		if (IsDebuggerPresent()) {
			DebugBreak();
		}

		ResumeThread(piProcInfo.hThread);

		CloseHandle(piProcInfo.hProcess);
		CloseHandle(piProcInfo.hThread);

		int InFd = _open_osfhandle((intptr_t)hChildStdoutRdDup, 0);
		int OutFd = _open_osfhandle((intptr_t)hChildStdinWrDup, 0);

		PipesLocal->In = fdopen(InFd, "rb");
		PipesLocal->Out = fdopen(OutFd, "wb");

		return 1;
	}
#else
	int pid;
	int stdinpipes[2];
	int stdoutpipes[2];

	pipe(stdinpipes);
	pipe(stdoutpipes);

	char **new_argv = (char **)malloc((argc + 2) * sizeof(char *));

	if (new_argv == NULL) {
		return 0;
	}

	memcpy(new_argv, argv, argc * sizeof(char *));

	new_argv[argc] = "--rpc-child";
	new_argv[argc + 1] = NULL;

	pid = fork();

	if (pid == 0) {
		// child

		close(stdinpipes[1]);
		close(stdoutpipes[0]);

		if (stdinpipes[0] != 0) {
			dup2(stdinpipes[0], 0);
		}
		close(stdinpipes[0]);

		if (stdoutpipes[1] != 1) {
			dup2(stdoutpipes[1], 1);
		}
		close(stdoutpipes[1]);

		fd = open("/dev/null", O_RDWR);
		if (fd != 2) {
			dup2(fd, 2);
		}
		close(fd);

		execvp(Program, new_argv);
		exit(0);
	} else if (pid > 0) {
		close(stdinpipes[0]);
		close(stdoutpipes[1]);

		PipesLocal->In = fdopen(stdoutpipes[0], "rb");
		PipesLocal->Out = fdopen(stdinpipes[1], "wb");

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
	while (RpcProcessCall(Pipes.In, Pipes.Out) > 0)
		; // empty

	return 1;
}

int RpcProcessCall(FILE *In, FILE *Out) {
	char Function;
	char ArgumentType;
	Value_t *Arguments = NULL;
	Value_t ReturnValue;
	int CID;
	unsigned int Index = 0;

	if (fread(&CID, 1, sizeof(int), In) <= 0) {
		return -1;
	}

	if (fread(&Function, 1, sizeof(char), In) <= 0) {
		return -1;
	}

	if (Function >= last_function) {
		return -1;
	}

	Arguments = (Value_t *)malloc(sizeof(Value_t) * functions[Function].ArgumentCount);

	for (Index = 0; Index < functions[Function].ArgumentCount; Index++) {
		if (fread(&ArgumentType, 1, sizeof(char), In) <= 0) {
			return -1;
		}

		Arguments[Index].Type = ArgumentType;

		if (ArgumentType == Integer) {
			Arguments[Index].Flags = Flag_None;

			if (fread(&(Arguments[Index].Integer), 1, sizeof(Arguments[Index].Integer), In) <= 0) {
				return -1;
			}
		} else if (ArgumentType == Pointer) {
			Arguments[Index].Flags = Flag_None;

			if (fread(&(Arguments[Index].Pointer), 1, sizeof(Arguments[Index].Pointer), In) <= 0) {
				return -1;
			}
		} else if (ArgumentType == Block) {
			char Flags;

			if (fread(&Flags, 1, sizeof(Flags), In) <= 0) {
				return -1;
			}

			Arguments[Index].Flags = Flags;

			if (fread(&(Arguments[Index].Size), 1, sizeof(Arguments[Index].Size), In) <= 0) {
				return -1;
			}

			Arguments[Index].NeedFree = true;
			Arguments[Index].Block = malloc(Arguments[Index].Size);

			if (Arguments[Index].Block == NULL) {
				return -1;
			}

			if (!(Arguments[Index].Flags & Flag_Alloc)) {
				if (fread(Arguments[Index].Block, 1, Arguments[Index].Size, In) <= 0 && Arguments[Index].Size != 0) {
					free(Arguments[Index].Block);

					return -1;
				}
			}
		}
	}

	errno = 0;

	if (fwrite(&CID, 1, sizeof(CID), Out) <= 0) {
		return -1;
	}

	if (functions[Function].RealFunction(Arguments, &ReturnValue)) {
		for (unsigned int i = 0; i < functions[Function].ArgumentCount; i++) {
			if (Arguments[i].Flags & Flag_Out) {
				Arguments[i].Flags &= ~Flag_Alloc;

				if (!RpcWriteValue(Out, Arguments[i])) {
					return -1;
				}
			}

			RpcFreeValue(Arguments[i]);
		}

		if (!RpcWriteValue(Out, ReturnValue)) {
			return -1;
		}

		RpcFreeValue(ReturnValue);
	} else {
		return -1;
	}

	fflush(Out);

	free(Arguments);

	return 1;
}

const char *RpcStringFromValue(Value_t Value) {
	if (Value.Type == Block) {
		return (const char *)Value.Block;
	} else {
		return NULL;
	}
}

int RpcInvokeFunction(Function_t Function, Value_t *Arguments, unsigned int ArgumentCount, Value_t *ReturnValue) {
	char FunctionByte;
	int CID, CIDReturn;
	FILE *RpcIn, *RpcOut;

	RpcIn = stdin;
	RpcOut = stdout;

	if (!g_LPC) {
		FunctionByte = Function;

		CID = rand();

		if (fwrite(&CID, 1, sizeof(CID), RpcOut) <= 0) {
			return 0;
		}

		if (fwrite(&FunctionByte, 1, sizeof(FunctionByte), RpcOut) <= 0) {
			return 0;
		}

		for (unsigned int i = 0; i < ArgumentCount; i++) {
			if (!RpcWriteValue(RpcOut, Arguments[i])) {
				return 0;
			}
		}

		fflush(RpcOut);

		if (!RpcBlockingRead(RpcIn, &CIDReturn, sizeof(CIDReturn))) {
			return 0;
		}

		if (CID != CIDReturn) {
			exit(200);
		}

		for (unsigned int i = 0; i < ArgumentCount; i++) {
			if (Arguments[i].Type == Block && Arguments[i].Flags & Flag_Out) {
				RpcFreeValue(Arguments[i]);

				if (!RpcReadValue(RpcIn, &(Arguments[i]))) {
					return 0;
				}
			}
		}

		if (!RpcReadValue(RpcIn, ReturnValue)) {
			return 0;
		}
	} else {
		if (functions[Function].ArgumentCount > ArgumentCount) {
			exit(201);
		}

		functions[Function].RealFunction(Arguments, ReturnValue);
	}

	return 1;
}

void RpcSetLPC(int LPC) {
	g_LPC = (LPC != 0);
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
	Val.NeedFree = false;

	return Val;
}

Value_t RpcBuildPointer(const void *Ptr) {
	Value_t Val;

	Val.Type = Pointer;
	Val.Pointer = const_cast<void *>(Ptr);
	Val.Flags = Flag_None;

	return Val;
}

Value_t RpcBuildString(const char *Ptr) {
	Value_t Val;

	if (Ptr != NULL) {
		Val.Type = Block;
		Val.Block = const_cast<char *>(Ptr);
		Val.Flags = Flag_None;
		Val.Size = strlen(Ptr) + 1;
		Val.NeedFree = false;
	} else {
		Val.Type = Pointer;
		Val.Pointer = NULL;
	}

	return Val;
}
