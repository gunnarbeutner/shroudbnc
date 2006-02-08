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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CModule::CModule(const char *Filename) {
	m_Far = NULL;
	m_File = strdup(Filename);
	m_Image = LoadLibrary(Filename);

	if (!m_Image) {
#ifdef _WIN32
		char *ErrorMsg, *p;

		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, (char*)&ErrorMsg, 0, NULL);

		p = strchr(ErrorMsg, '\r');

		if (p != NULL) {
			p[0] = '\0';
		}

		p = strchr(ErrorMsg, '\n');

		if (p != NULL) {
			p[0] = '\0';
		}
#else
		const char *ErrorMsg;

		ErrorMsg = lt_dlerror();
#endif

		if (ErrorMsg == NULL)
			m_Error = strdup("Unknown error.");
		else
			m_Error = strdup(ErrorMsg);

#ifdef _WIN32
		if (ErrorMsg)
			LocalFree(ErrorMsg);
#endif
	} else {
		FNGETINTERFACEVERSION pfGetInterfaceVersion =
			(FNGETINTERFACEVERSION)GetProcAddress(m_Image,
			"bncGetInterfaceVersion");

		if (pfGetInterfaceVersion != NULL && pfGetInterfaceVersion() < INTERFACEVERSION) {
			m_Error = strdup("This module was compiled for an earlier version"
				" of shroudBNC. Please recompile the module and try again.");

			return;
		}

		if (GetModule() == NULL) {
			m_Error = strdup("GetModule() failed.");

			return;
		}

		m_Error = NULL;
	}
}

CModule::~CModule() {
	if (m_Far) {
		m_Far->Destroy();
	}

	if (m_Image) {
		FreeLibrary(m_Image);
	}

	free(m_File);

	free(m_Error);
}

CModuleFar *CModule::GetModule(void) {
	if (!m_Image) {
		return NULL;
	}

	if (m_Far != NULL) {
		return m_Far;
	} else {
		FNGETOBJECT pfGetObject = (FNGETOBJECT)GetProcAddress(m_Image, "bncGetObject");

		if (pfGetObject) {
			m_Far = pfGetObject();

			return m_Far;
		} else {
			return NULL;
		}
	}
}

const char *CModule::GetFilename(void) {
	return m_File;
}

HMODULE CModule::GetHandle(void) {
	return m_Image;
}

const char *CModule::GetError(void) {
	return m_Error;
}

void CModule::Destroy(void) {
	m_Far->Destroy();
}

void CModule::Init(CCore *Root) {
	m_Far->Init(Root);
}

bool CModule::InterceptIRCMessage(CIRCConnection *Connection, int argc, const char **argv) {
	return m_Far->InterceptIRCMessage(Connection, argc, argv);
}

bool CModule::InterceptClientMessage(CClientConnection *Connection, int argc, const char **argv) {
	return m_Far->InterceptClientMessage(Connection, argc, argv);
}

void CModule::AttachClient(const char *Client) {
	m_Far->AttachClient(Client);
}

void CModule::DetachClient(const char *Client) {
	m_Far->DetachClient(Client);
}

void CModule::ServerDisconnect(const char *Client) {
	m_Far->ServerDisconnect(Client);
}

void CModule::ServerConnect(const char *Client) {
	m_Far->ServerConnect(Client);
}

void CModule::ServerLogon(const char *Client) {
	m_Far->ServerLogon(Client);
}

void CModule::UserLoad(const char *User) {
	m_Far->UserLoad(User);
}

void CModule::UserCreate(const char *User) {
	m_Far->UserCreate(User);
}

void CModule::UserDelete(const char *User) {
	m_Far->UserDelete(User);
}

void CModule::SingleModeChange(CIRCConnection *Connection, const char *Channel, const char *Source, bool Flip, char Mode, const char *Parameter) {
	m_Far->SingleModeChange(Connection, Channel, Source, Flip, Mode, Parameter);
}

const char *CModule::Command(const char *Cmd, const char *Parameters) {
	return m_Far->Command(Cmd, Parameters);
}

bool CModule::InterceptClientCommand(CClientConnection *Connection, const char *Subcommand, int argc, const char **argv, bool NoticeUser) {
	return m_Far->InterceptClientCommand(Connection, Subcommand, argc, argv, NoticeUser);
}

void CModule::TagModified(const char *Tag, const char *Value) {
	return m_Far->TagModified(Tag, Value);
}

void CModule::UserTagModified(const char *Tag, const char *Value) {
	return m_Far->UserTagModified(Tag, Value);
}
