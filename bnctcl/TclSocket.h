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

class CCore;
class CConnection;
class CTclSocket;

#undef sprintf

extern CHashtable<CTclSocket*, false, 5>* g_TclListeners;
extern int g_SocketIdx;
extern Tcl_Interp* g_Interp;

IMPL_SOCKETLISTENER(CTclSocket, int) {
private:
	int m_Idx;
	bool m_SSL;
	char *m_TclProc;
public:
	CTclSocket(unsigned int Port, const char *BindIp, const char *TclProc, bool SSL) : CListenerBase<int>(Port, BindIp, NULL) {
		char Buf[20];

		m_TclProc = strdup(TclProc);

		sprintf(Buf, "%d", g_SocketIdx);
		m_Idx = g_SocketIdx;
		g_SocketIdx++;

		m_SSL = SSL;

		g_TclListeners->Add(Buf, this);
	}

	~CTclSocket(void) {
		char Buf[20];

		free(m_TclProc);

		sprintf(Buf, "%d", m_Idx);

		g_TclListeners->Remove(Buf);
	}

	virtual const char *GetClassName(void) {
		return "CTclSocket";
	}

	virtual int GetIdx(void) {
		return m_Idx;
	}

	virtual void Accept(SOCKET Client, sockaddr_in PeerName) {
		char ptr[20];
		Tcl_Obj* objv[2];
		CTclClientSocket *TclClient;

		TclClient = new CTclClientSocket(Client, m_SSL, Role_Server);

		sprintf(ptr, "%d", TclClient->GetIdx());

		objv[0] = Tcl_NewStringObj(m_TclProc, strlen(m_TclProc));
		Tcl_IncrRefCount(objv[0]);

		objv[1] = Tcl_NewStringObj(ptr, strlen(ptr));
		Tcl_IncrRefCount(objv[1]);

		Tcl_EvalObjv(g_Interp, 2, objv, TCL_EVAL_GLOBAL);

		Tcl_DecrRefCount(objv[1]);
		Tcl_DecrRefCount(objv[0]);

		if (TclClient->GetControlProc() == NULL) {
			delete TclClient;
		}
	}
};
