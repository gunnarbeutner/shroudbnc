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

extern Tcl_Interp* g_Interp;

CHashtable<CTclClientSocket*, false, 5>* g_TclClientSockets;
extern int g_SocketIdx;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTclClientSocket::CTclClientSocket(SOCKET Socket, bool SSL, connection_role_e Role) {
	m_Socket = Socket;
	m_Wrap = g_Bouncer->WrapSocket(Socket, SSL, Role);
	g_Bouncer->RegisterSocket(Socket, this);

	char *Buf;

	g_asprintf(&Buf, "%d", g_SocketIdx);
	m_Idx = g_SocketIdx;
	g_SocketIdx++;

	g_TclClientSockets->Add(Buf, this);

	g_free(Buf);

	m_Control = NULL;
	m_InTcl = false;
	m_Destroy = false;
}

CTclClientSocket::~CTclClientSocket() {
	g_Bouncer->UnregisterSocket(m_Socket);
	g_Bouncer->DeleteWrapper(m_Wrap);
	closesocket(m_Socket);

	char *Buf;
	g_asprintf(&Buf, "%d", m_Idx);

	g_TclClientSockets->Remove(Buf);
	g_free(Buf);

	free(m_Control);
}

void CTclClientSocket::Destroy(void) {
	if (m_Control != NULL && g_Interp != NULL) {
		Tcl_Obj* objv[3];

		char *ptr;
		g_asprintf(&ptr, "%d", m_Idx);

		objv[0] = Tcl_NewStringObj(m_Control, strlen(m_Control));
		Tcl_IncrRefCount(objv[0]);

		objv[1] = Tcl_NewStringObj(ptr, strlen(ptr));
		Tcl_IncrRefCount(objv[1]);

		g_free(ptr);

		objv[2] = Tcl_NewStringObj("", 0);
		Tcl_IncrRefCount(objv[2]);

		m_InTcl = true;
		Tcl_EvalObjv(g_Interp, 3, objv, TCL_EVAL_GLOBAL);
		m_InTcl = false;

		Tcl_DecrRefCount(objv[2]);
		Tcl_DecrRefCount(objv[1]);
		Tcl_DecrRefCount(objv[0]);

		m_Control = NULL;
	}

	if (g_Bouncer->GetStatus() != STATUS_RUN || m_Wrap->GetSendqSize() == 0) {
		delete this;
	} else {
		m_Wrap->Kill("");
	}
}

int CTclClientSocket::Read(bool DontProcess) {
	int Ret = m_Wrap->Read();

	if (DontProcess)
		return 0;

	char* Line;

	while (m_Wrap->ReadLine(&Line) && m_Control) {
		Tcl_Obj* objv[3];

		if (strlen(Line) == 0) {
			g_free(Line);

			continue;
		}

		char *ptr;
		g_asprintf(&ptr, "%d", m_Idx);

		objv[0] = Tcl_NewStringObj(m_Control, strlen(m_Control));
		Tcl_IncrRefCount(objv[0]);

		objv[1] = Tcl_NewStringObj(ptr, strlen(ptr));
		Tcl_IncrRefCount(objv[1]);

		g_free(ptr);

		Tcl_DString dsText;

		Tcl_DStringInit(&dsText);
		Tcl_ExternalToUtfDString(NULL, Line, -1, &dsText);

		objv[2] = Tcl_NewStringObj(Tcl_DStringValue(&dsText), strlen(Tcl_DStringValue(&dsText)));
		Tcl_IncrRefCount(objv[2]);

		m_InTcl = true;
		int Code = Tcl_EvalObjv(g_Interp, 3, objv, TCL_EVAL_GLOBAL);
		m_InTcl = false;

		Tcl_DecrRefCount(objv[1]);
		Tcl_DecrRefCount(objv[0]);
		Tcl_DStringFree(&dsText);

		g_free(Line);
	}

	if (m_Destroy)
		return -1;
	else
		return Ret;
}

int CTclClientSocket::Write(void) {
	if (m_Wrap == NULL) {
		return 0;
	}

	return m_Wrap->Write();
}

void CTclClientSocket::Error(int ErrorCode) {
	if (m_Wrap == NULL) {
		return;
	}

	m_Wrap->Error(ErrorCode);
}

bool CTclClientSocket::HasQueuedData(void) const {
	if (m_Wrap == NULL) {
		return false;
	}

	return m_Wrap->HasQueuedData();
}

void CTclClientSocket::SetControlProc(const char* Proc) {
	free(m_Control);
	m_Control = strdup(Proc);

	if (m_Wrap->GetRecvqSize() > 0) {
		Read(false);
	}
}

const char* CTclClientSocket::GetControlProc(void) const {
	return m_Control;
}

void CTclClientSocket::WriteLine(const char* Line) {
	m_Wrap->WriteUnformattedLine(Line);
}

const char* CTclClientSocket::GetClassName(void) const {
	return "CTclClientSocket";
}

int CTclClientSocket::GetIdx(void) {
	return m_Idx;
}

bool CTclClientSocket::MayNotEnterDestroy(void) {
	return m_InTcl;
}

void CTclClientSocket::DestroyLater(void) {
	m_Destroy = true;
}

bool CTclClientSocket::ShouldDestroy(void) const {
	return false; 
}

void CTclClientSocket::SetSendQ(CFIFOBuffer *Buffer) {
	m_Wrap->SetSendQ(Buffer);
}

void CTclClientSocket::SetRecvQ(CFIFOBuffer *Buffer) {
	m_Wrap->SetRecvQ(Buffer);
}

void CTclClientSocket::SetSSLObject(SSL *SSLObject) {
	m_Wrap->SetSSLObject(SSLObject);
}

sockaddr *CTclClientSocket::GetRemoteAddress(void) const {
	return m_Wrap->GetRemoteAddress();
}
