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

#include "StdAfx.h"

// TODO: Write comments.

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CModule::CModule(const char *Filename) {
	char *CorePath;
	bool Result = false;

	m_Far = NULL;
	m_Image = NULL;
	m_File = strdup(Filename);

	CorePath = strdup(g_Bouncer->GetLoaderParameters()->GetModulePath());

	if (CorePath != NULL && *CorePath != '\0') {
		for (size_t i = strlen(CorePath) - 1; i >= 0; i--) {
			if (CorePath[i] == '/' || CorePath[i] == '\\') {
				CorePath[i] = '\0';

				break;
			}
		}

#if !defined(_WIN32) || defined(__MINGW32__)
		lt_dlsetsearchpath(CorePath);
#endif
	
		Result = InternalLoad(g_Bouncer->BuildPath(Filename, CorePath));
	}

	if (!Result) {
		InternalLoad(Filename);
	}
}

CModule::~CModule() {
	if (m_Far) {
		m_Far->Destroy();
	}

	if (m_Image != NULL) {
		FreeLibrary(m_Image);
	}

	free(m_File);

	free(m_Error);
}

bool CModule::InternalLoad(const char *Filename) {
	const CVector<CModule *> *Modules;

	m_Image = LoadLibrary(Filename);

	if (!m_Image) {
#ifdef _WIN32
		char *ErrorMsg, *p;

		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, (char *)&ErrorMsg, 0, NULL);

		if (ErrorMsg != NULL) {
			p = strchr(ErrorMsg, '\r');

			if (p != NULL) {
				p[0] = '\0';
			}

			p = strchr(ErrorMsg, '\n');

			if (p != NULL) {
				p[0] = '\0';
			}
		}
#else
		const char *ErrorMsg;

		ErrorMsg = lt_dlerror();
#endif

		if (ErrorMsg == NULL) {
			m_Error = strdup("Unknown error.");
		} else {
			m_Error = strdup(ErrorMsg);

#ifdef _WIN32
			if (ErrorMsg != NULL) {
				LocalFree(ErrorMsg);
			}
#endif
		}

		return false;
	} else {
		Modules = g_Bouncer->GetModules();

		for (unsigned int i = 0; i < Modules->GetLength(); i++) {
			if ((*Modules)[i]->GetHandle() == m_Image) {
				m_Error = strdup("This module is already loaded.");

				FreeLibrary(m_Image);
				m_Image = NULL;

				return false;
			}
		}
		FNGETINTERFACEVERSION pfGetInterfaceVersion =
			(FNGETINTERFACEVERSION)GetProcAddress(m_Image,
			"bncGetInterfaceVersion");

		if (pfGetInterfaceVersion != NULL && pfGetInterfaceVersion() < INTERFACEVERSION) {
			m_Error = strdup("This module was compiled for an earlier version"
				" of shroudBNC. Please recompile the module and try again.");

			FreeLibrary(m_Image);
			m_Image = NULL;

			return false;
		}

		if (GetModule() == NULL) {
			m_Error = strdup("GetModule() failed.");

			FreeLibrary(m_Image);
			m_Image = NULL;

			return false;
		}

		m_Error = NULL;
	}

	return true;
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

RESULT<bool> CModule::GetError(void) {
	if (m_Error != NULL) {
		THROW(bool, Generic_Unknown, m_Error);
	} else  {
		RETURN(bool, true);
	}
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

void CModule::AttachClient(CClientConnection *Client) {
	m_Far->AttachClient(Client);
}

void CModule::DetachClient(CClientConnection *Client) {
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
