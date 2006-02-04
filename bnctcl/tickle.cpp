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

#include "../src/StdAfx.h"
#include "StdAfx.h"

#include "TclClientSocket.h"
#include "TclSocket.h"
#include "tickle.h"
#include "tickleProcs.h"

#define FREEZEFIX

class CTclSupport;

CCore* g_Bouncer;
Tcl_Interp* g_Interp;
CTclSupport* g_Tcl;
bool g_Ret;
bool g_NoticeUser;
Tcl_Encoding g_Encoding;

extern tcltimer_t** g_Timers;
extern int g_TimerCount;

int Tcl_AppInit(Tcl_Interp *interp) {
	if (Tcl_Init(interp) == TCL_ERROR)
		return TCL_ERROR;

	if (Tcl_ProcInit(interp) == TCL_ERROR)
		return TCL_ERROR;

	return TCL_OK;
}

class CTclSupport : public CModuleImplementation {
	void Destroy(void) {
		CallBinds(Type_Unload, NULL, 0, NULL);

		Tcl_FreeEncoding(g_Encoding);

		Tcl_DeleteInterp(g_Interp);

		Tcl_Release(g_Interp);

		Tcl_Finalize();

		int i = 0;

		while (hash_t<CTclSocket*>* p = g_TclListeners->Iterate(i)) {
			static_cast<CSocketEvents*>(p->Value)->Destroy();
		}

		delete g_TclListeners;

		i = 0;

		while (hash_t<CTclClientSocket*>* p = g_TclClientSockets->Iterate(i)) {
			p->Value->Destroy();
		}

		delete g_TclClientSockets;

		for (int a = 0; a < g_TimerCount; a++) {
			if (g_Timers[a]) {
				g_Timers[a]->timer->Destroy();
				free(g_Timers[a]->proc);
				free(g_Timers[a]->param);
			}
		}

		delete this;
	}

	void Init(CCore* Root) {
		g_Bouncer = Root;

		g_TclListeners = new CHashtable<CTclSocket*, false, 5>;
		g_TclClientSockets = new CHashtable<CTclClientSocket*, false, 5>;

		Tcl_FindExecutable(Root->GetArgV()[0]);

		Tcl_SetSystemEncoding(NULL, "ISO8859-1");

		g_Encoding = Tcl_GetEncoding(g_Interp, "ISO8859-1");

		g_Interp = Tcl_CreateInterp();

		Tcl_SetVar(g_Interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);

		Tcl_AppInit(g_Interp);

		Tcl_Preserve(g_Interp);

		Tcl_EvalFile(g_Interp, "./sbnc.tcl");
	}

	bool InterceptIRCMessage(CIRCConnection* IRC, int argc, const char** argv) {
		g_Ret = true;

		CallBinds(Type_PreScript, NULL, 0, NULL);
		CallBinds(Type_Server, IRC->GetOwner()->GetUsername(), argc, argv);
		CallBinds(Type_PostScript, NULL, 0, NULL);

		return g_Ret;
	}

	bool InterceptClientMessage(CClientConnection* Client, int argc, const char** argv) {
		g_Ret = true;

		CUser* User = Client->GetOwner();

		CallBinds(Type_PreScript, NULL, 0, NULL);
		CallBinds(Type_Client, User ? User->GetUsername() : NULL, argc, argv);
		CallBinds(Type_PostScript, NULL, 0, NULL);

		return g_Ret;
	}

	void AttachClient(const char* Client) {
		CallBinds(Type_Attach, Client, 0, NULL);
	}

	void DetachClient(const char* Client) {
		CallBinds(Type_Detach, Client, 0, NULL);
	}

	void ServerDisconnect(const char* Client) {
		CallBinds(Type_SvrDisconnect, Client, 0, NULL);
	}

	void ServerConnect(const char* Client) {
		CallBinds(Type_SvrConnect, Client, 0, NULL);
	}

	void ServerLogon(const char* Client) {
		CallBinds(Type_SvrLogon, Client, 0, NULL);
	}

	void UserLoad(const char* User) {
		CallBinds(Type_UsrLoad, User, 0, NULL);
	}

	void UserCreate(const char* User) {
		CallBinds(Type_UsrCreate, User, 0, NULL);
	}

	void UserDelete(const char* User) {
		CallBinds(Type_UsrDelete, User, 0, NULL);
	}

	void SingleModeChange(CIRCConnection* IRC, const char* Channel, const char* Source, bool Flip, char Mode, const char* Parameter) {
		char ModeC[3];

		ModeC[0] = Flip ? '+' : '-';
		ModeC[1] = Mode;
		ModeC[2] = '\0';

		const char* argv[4] = { Source, Channel, ModeC, Parameter };

		CallBinds(Type_SingleMode, IRC->GetOwner()->GetUsername(), Parameter ? 4 : 3, argv);
	}

	const char* Command(const char* Cmd, const char* Parameters) {
		if (strcasecmp(Cmd, "tcl:eval") == 0) {
			Tcl_Eval(g_Interp, Parameters);

			Tcl_Obj* Result = Tcl_GetObjResult(g_Interp);

			const char* strResult = Tcl_GetString(Result);

			return strResult;
		}

		return NULL;
	}

	bool InterceptClientCommand(CClientConnection* Client, const char* Subcommand, int argc, const char** argv, bool NoticeUser) {
		CUser* User = Client->GetOwner();
		const utility_t *Utils;

		g_NoticeUser = NoticeUser;

		if (strcasecmp(Subcommand, "help") == 0 && User && User->IsAdmin()) {
			commandlist_t *Commands = Client->GetCommandList();
			Utils = g_Bouncer->GetUtilities();

			Utils->AddCommand(Commands, "tcl", "Admin", "executes tcl commands", "Syntax: tcl command\nExecutes the specified tcl command.");
		}

		if (strcasecmp(Subcommand, "tcl") == 0 && User && User->IsAdmin()) {

			if (argc <= 1) {
				if (NoticeUser)
					User->RealNotice("Syntax: tcl :command");
				else
					User->Notice("Syntax: tcl :command");

				return true;
			}

			setctx(User->GetUsername());

			Tcl_DString dsScript;
			const char **argvdup;

			Utils = g_Bouncer->GetUtilities();

			argvdup = Utils->ArgDupArray(argv);
			Utils->ArgRejoinArray(argvdup, 1);

			int Code = Tcl_EvalEx(g_Interp, Tcl_UtfToExternalDString(g_Encoding, argvdup[1], -1, &dsScript), -1, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);

			Utils->ArgFreeArray(argvdup);

			Tcl_DStringFree(&dsScript);

			Tcl_Obj* Result = Tcl_GetObjResult(g_Interp);

			const char* strResult = Tcl_GetString(Result);

			if (Code == TCL_ERROR) {
				if (NoticeUser)
					User->RealNotice("An error occured in the tcl script:");
				else
					User->Notice("An error occured in the tcl script:");
			}

			if (strResult && *strResult) {
				Tcl_DString dsResult;

				char* Dup = strdup(Tcl_UtfToExternalDString(g_Encoding, strResult, -1, &dsResult));

				Tcl_DStringFree(&dsResult);

				char* token = strtok(Dup, "\n");

				while (token != NULL) {
					if (NoticeUser)
						User->RealNotice(strResult ? (*token ? token : "empty string.") : "<null>");
					else
						User->Notice(strResult ? (*token ? token : "empty string.") : "<null>");

					token = strtok(NULL, "\n");
				}
			} else {
					if (NoticeUser)
						User->RealNotice("<null>");
					else
						User->Notice("<null>");
			}

			return true;
		}

		g_Ret = true;

		CallBinds(Type_Command, Client->GetOwner()->GetUsername(), argc, argv);

		return !g_Ret;
	}

	void TagModified(const char *Tag, const char *Value) {
		const char *argv[2];

		argv[0] = Tag;
		argv[1] = Value;
		CallBinds(Type_SetTag, NULL, 2, argv);
	}

	void UserTagModified(const char *Tag, const char *Value) {
		const char *argv[2];

		argv[0] = Tag;
		argv[1] = Value;
		CallBinds(Type_SetUserTag, NULL, 2, argv);
	}
public:
	void RehashInterpreter(void) {
		Tcl_EvalFile(g_Interp, "./sbnc.tcl");
	}
};

extern "C" EXPORT CModuleFar* bncGetObject(void) {
	g_Tcl = new CTclSupport();
	return (CModuleFar*)g_Tcl;
}

extern "C" EXPORT int bncGetInterfaceVersion(void) {
	return INTERFACEVERSION;
}

void RehashInterpreter(void) {
	g_Tcl->RehashInterpreter();
}

void CallBinds(binding_type_e type, const char* user, int argc, const char** argv) {
	Tcl_Obj** listv;

	CUser* User = g_Bouncer->GetUser(user);

	if (User)
		setctx(user);

	int objc = 3;
	int idx = 1;
	Tcl_Obj* objv[3];
	bool lazyConversionDone = false;

	for (int i = 0; i < g_BindCount; i++) {
		if (g_Binds[i].valid && g_Binds[i].type == type) {
			Tcl_DString dsProc;

			if (g_Binds[i].user && user && strcasecmp(g_Binds[i].user, user) != 0)
				continue;

			bool Match = false;

			if (g_Binds[i].pattern == NULL)
				Match = true;

			if (!Match) {
				for (int a = 0; a < argc; a++) {
					if (g_Bouncer->Match(g_Binds[i].pattern, argv[a])) {
						Match = true;

						break;
					}
				}
			}

			if (Match) {
				if (!lazyConversionDone) {
					if (user) {
						Tcl_DString dsUser;

						Tcl_ExternalToUtfDString(g_Encoding, user ? user : "", -1, &dsUser);
						objv[idx++] = Tcl_NewStringObj(Tcl_DStringValue(&dsUser), Tcl_DStringLength(&dsUser));
						Tcl_DStringFree(&dsUser);

						Tcl_IncrRefCount(objv[idx - 1]);
					}

					if (argc) {
						listv = (Tcl_Obj**)malloc(sizeof(Tcl_Obj*) * argc);

						for (int a = 0; a < argc; a++) {
							Tcl_DString dsString;

							Tcl_ExternalToUtfDString(g_Encoding, argv[a], -1, &dsString);
							listv[a] = Tcl_NewStringObj(Tcl_DStringValue(&dsString), Tcl_DStringLength(&dsString));
							Tcl_DStringFree(&dsString);

							Tcl_IncrRefCount(listv[a]);
						}

						objv[idx++] = Tcl_NewListObj(argc, listv);
						Tcl_IncrRefCount(objv[idx - 1]);
					}

					lazyConversionDone = true;
				}

				Tcl_ExternalToUtfDString(g_Encoding, g_Binds[i].proc, -1, &dsProc);
				objv[0] = Tcl_NewStringObj(Tcl_DStringValue(&dsProc), Tcl_DStringLength(&dsProc));
				Tcl_DStringFree(&dsProc);

				Tcl_IncrRefCount(objv[0]);

				Tcl_EvalObjv(g_Interp, idx, objv, TCL_EVAL_GLOBAL);

				Tcl_DecrRefCount(objv[0]);
			}
		}
	}

	if (lazyConversionDone) {
		for (int i = 1; i < idx; i++) {
			if (objv[i])
				Tcl_DecrRefCount(objv[i]);
		}


		if (argc) {
			for (int a = 0; a < argc; a++) {
				Tcl_DecrRefCount(listv[a]);
			}

			free(listv);
		}
	}
}

void SetLatchedReturnValue(bool Ret) {
	g_Ret = Ret;
}
