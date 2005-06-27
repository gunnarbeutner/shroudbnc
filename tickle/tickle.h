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

extern Tcl_Interp* g_Interp;

enum binding_type_e {
	Type_Invalid,
	Type_Client,
	Type_Server,
	Type_Pulse,
	Type_PreScript,
	Type_PostScript,
	Type_Attach,
	Type_Detach,
	Type_SingleMode,
	Type_Unload,
	Type_SvrDisconnect,
	Type_SvrConnect,
	Type_SvrLogon,
	Type_UsrLoad,
	Type_UsrCreate,
	Type_UsrDelete,
	Type_Command
};

typedef struct binding_s {
	bool valid;
	binding_type_e type;
	char* proc;
} binding_t;

extern binding_t* g_Binds;
extern int g_BindCount;

class CTclSocket;
class CTclClientSocket;

extern CHashtable<CTclSocket*, false>* g_TclListeners;
extern CHashtable<CTclClientSocket*, false>* g_TclClientSockets;
extern int g_SocketIdx;

void RestartInterpreter(void);
void RehashInterpreter(void);
void CallBinds(binding_type_e type, const char* user, int argc, const char** argv);
void SetLatchedReturnValue(bool Ret);
