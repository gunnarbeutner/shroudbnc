/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

extern CHashtable<CTclSocket*, false>* g_TclListeners;
extern int g_SocketIdx;
extern Tcl_Interp* g_Interp;


IMPL_SOCKETLISTENER(CTclSocket) {
private:
	int m_Idx;
	bool m_SSL;
	char *m_TclProc;
public:
	CTclSocket(unsigned int Port, const char *BindIp, const char *TclProc, bool SSL) : CListenerBase<CTclSocket>(Port, BindIp) {
		char *Buf;

		m_TclProc = strdup(TclProc);

		int rc = asprintf(&Buf, "%d", g_SocketIdx);

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		m_Idx = g_SocketIdx;
		g_SocketIdx++;

		m_SSL = SSL;

		g_TclListeners->Add(Buf, this);

		free(Buf);
	}

	~CTclSocket(void) {
		char *Buf;

		free(m_TclProc);

		int rc = asprintf(&Buf, "%d", m_Idx);

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		g_TclListeners->Remove(Buf);

		free(Buf);
	}

	virtual const char *GetClassName(void) const {
		return "CTclSocket";
	}

	virtual int GetIdx(void) {
		return m_Idx;
	}

	virtual void Accept(SOCKET Client, const sockaddr *PeerAddress) {
		char *ptr;
		Tcl_Obj* objv[2];
		CTclClientSocket *TclClient;

		TclClient = new CTclClientSocket(Client, m_SSL, Role_Server);

		int rc = asprintf(&ptr, "%d", TclClient->GetIdx());

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		objv[0] = Tcl_NewStringObj(m_TclProc, strlen(m_TclProc));
		Tcl_IncrRefCount(objv[0]);

		objv[1] = Tcl_NewStringObj(ptr, strlen(ptr));
		Tcl_IncrRefCount(objv[1]);

		free(ptr);

		Tcl_EvalObjv(g_Interp, 2, objv, TCL_EVAL_GLOBAL);

		Tcl_DecrRefCount(objv[1]);
		Tcl_DecrRefCount(objv[0]);

		if (TclClient->GetControlProc() == NULL) {
			delete TclClient;
		}
	}
};
