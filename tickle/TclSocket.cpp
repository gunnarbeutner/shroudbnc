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

#include "StdAfx.h"
#include "../StdAfx.h"
#include "TclSocket.h"
#include "TclClientSocket.h"

extern Tcl_Interp* g_Interp;

CHashtable<CTclSocket*, false, 5>* g_TclListeners;
int g_SocketIdx = 0;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTclSocket::CTclSocket(const char* BindIp, unsigned short Port, const char* TclProc) {
	m_Listener = g_Bouncer->CreateListener(Port, BindIp);

	if (IsValid()) {
		g_Bouncer->RegisterSocket(m_Listener, this);
		m_TclProc = strdup(TclProc);
	} else
		m_TclProc = NULL;

	char Buf[20];

	itoa(g_SocketIdx, Buf, 10);
	m_Idx = g_SocketIdx;
	g_SocketIdx++;

	g_TclListeners->Add(Buf, this);
}

CTclSocket::~CTclSocket() {
	if (IsValid()) {
		g_Bouncer->UnregisterSocket(m_Listener);

		closesocket(m_Listener);

		free(m_TclProc);
	}

	char Buf[20];
	itoa(m_Idx, Buf, 10);

	g_TclListeners->Remove(Buf);
}

void CTclSocket::Destroy(void) {
	delete this;
}

bool CTclSocket::Read(bool DontProcess) {
	sockaddr_in peer;
	socklen_t peerSize = sizeof(peer);

	SOCKET Socket = accept(m_Listener, (sockaddr*)&peer, &peerSize);

	CTclClientSocket* Client = new CTclClientSocket(Socket);

	char ptr[20];

	itoa(Client->GetIdx(), ptr, 10);

	Tcl_Obj* objv[2];
	
	objv[0] = Tcl_NewStringObj(m_TclProc, strlen(m_TclProc));
	Tcl_IncrRefCount(objv[0]);

	objv[1] = Tcl_NewStringObj(ptr, strlen(ptr));
	Tcl_IncrRefCount(objv[1]);

	Tcl_EvalObjv(g_Interp, 2, objv, TCL_EVAL_GLOBAL);

	Tcl_DecrRefCount(objv[1]);
	Tcl_DecrRefCount(objv[0]);

	if (Client->GetControlProc() == NULL) {
		delete Client;
	}

	return true;
}

void CTclSocket::Write(void) {

}

void CTclSocket::Error(void) {

}

bool CTclSocket::HasQueuedData(void) {
	return false;
}

bool CTclSocket::DoTimeout(void) {
	return false;
}

const char* CTclSocket::ClassName(void) {
	return "CTclSocket";
}

bool CTclSocket::IsValid(void) {
	return m_Listener != INVALID_SOCKET;
}

int CTclSocket::GetIdx(void) {
	return m_Idx;
}
