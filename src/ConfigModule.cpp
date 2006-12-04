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

CConfigModule::CConfigModule(const char *Filename) {
	if (Filename == NULL) {
		m_Far = new CDefaultConfigModule();
		m_File = NULL;
		m_Error = NULL;
	} else {
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
}

CConfigModule::~CConfigModule(void) {
	if (m_Far) {
		m_Far->Destroy();
	}

	if (m_Image != NULL) {
		FreeLibrary(m_Image);
	}

	free(m_File);

	free(m_Error);
}

bool CConfigModule::InternalLoad(const char *Filename) {
	m_Image = LoadLibrary(Filename);

	if (!m_Image) {
#ifdef _WIN32
		char *ErrorMsg, *p;

		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, (char *)&ErrorMsg, 0, NULL);

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

		if (ErrorMsg == NULL) {
			m_Error = strdup("Unknown error.");
		} else {
			m_Error = strdup(ErrorMsg);
		}

#ifdef _WIN32
		if (ErrorMsg != NULL) {
			LocalFree(ErrorMsg);
		}
#endif

		return false;
	} else {
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

void CConfigModule::Destroy(void) {
	m_Far->Destroy();
}

void CConfigModule::Init(CCore *Root) {
	m_Far->Init(Root);
}

CConfig *CConfigModule::CreateConfigObject(const char *Name, CUser *Owner) {
	return m_Far->CreateConfigObject(Name, Owner);
}

CConfigModuleFar *CConfigModule::GetModule(void) {
	if (!m_Image) {
		return NULL;
	}

	if (m_Far != NULL) {
		return m_Far;
	} else {
		FNGETCONFIGOBJECT pfGetConfigObject = (FNGETCONFIGOBJECT)GetProcAddress(m_Image, "bncGetConfigObject");

		if (pfGetConfigObject) {
			m_Far = pfGetConfigObject();

			return m_Far;
		} else {
			return NULL;
		}
	}
}

const char *CConfigModule::GetFilename(void) {
	return m_File;
}

HMODULE CConfigModule::GetHandle(void) {
	return m_Image;
}

RESULT<bool> CConfigModule::GetError(void) {
	if (m_Error != NULL) {
		THROW(bool, Generic_Unknown, m_Error);
	} else  {
		RETURN(bool, true);
	}
}

CConfig *CDefaultConfigModule::CreateConfigObject(const char *Name, CUser *Owner) {
	return new CConfigFile(g_Bouncer->BuildPath(Name), Owner);
}
