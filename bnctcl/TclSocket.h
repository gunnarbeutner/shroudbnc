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

#if !defined(AFX_TCLSOCKET_H__840815A2_F117_4D3D_8593_4F97538DE148__INCLUDED_)
#define AFX_TCLSOCKET_H__840815A2_F117_4D3D_8593_4F97538DE148__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CCore;
class CConnection;
class CTclSocket;

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

		itoa(g_SocketIdx, Buf, 10);
		m_Idx = g_SocketIdx;
		g_SocketIdx++;

		m_SSL = SSL;

		g_TclListeners->Add(Buf, this);
	}

	~CTclSocket(void) {
		char Buf[20];

		free(m_TclProc);

		itoa(m_Idx, Buf, 10);

		g_TclListeners->Remove(Buf);
	}

	virtual const char *ClassName(void) {
		return "CTclSocket";
	}

	virtual int GetIdx(void) {
		return m_Idx;
	}

	virtual void Accept(SOCKET Client, sockaddr_in PeerName) {
		char ptr[20];
		Tcl_Obj* objv[2];
		CTclClientSocket *TclClient;

		TclClient = new CTclClientSocket(Client, false, m_SSL);

		itoa(TclClient->GetIdx(), ptr, 10);

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

#if 0
class CTclSocket : public CSocketEvents {
public:
	CTclSocket(const char* BindIp, unsigned short Port, const char* TclProc);
	virtual ~CTclSocket();

	virtual void Destroy(void);

	virtual bool Read(bool DontProcess);
	virtual void Write(void);
	virtual void Error(void);
	virtual bool HasQueuedData(void);
	virtual bool DoTimeout(void);
	virtual bool ShouldDestroy(void);


	virtual const char* ClassName(void);

	virtual bool IsValid(void);
	virtual int GetIdx(void);
private:
	SOCKET m_Listener;
	char* m_TclProc;
	int m_Idx;
};
#endif

#endif // !defined(AFX_TCLSOCKET_H__840815A2_F117_4D3D_8593_4F97538DE148__INCLUDED_)
