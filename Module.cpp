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
#include "ModuleFar.h"
#include "Module.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CModule::CModule(const char* Filename) {
	m_Far = NULL;
	m_File = strdup(Filename);
	m_Image = LoadLibrary(Filename);
}

CModule::~CModule() {
	if (m_Far)
		m_Far->Destroy();

	FreeLibrary(m_Image);

	free(m_File);
}

CModuleFar* CModule::GetModule(void) {
	if (!m_Image) {
		return NULL;
	}

	if (m_Far)
		return m_Far;
	else {
		FNGETOBJECT pfGetObject = (FNGETOBJECT)GetProcAddress(m_Image, "bncGetObject");

		if (pfGetObject) {
			m_Far = pfGetObject();

			return m_Far;
		} else
			return NULL;
	}
}

const char* CModule::GetFilename(void) {
	return m_File;
}

HMODULE CModule::GetHandle(void) {
	return m_Image;
}

void CModule::Destroy(void) {
	m_Far->Destroy();
}

void CModule::Init(CBouncerCore* Root) {
	m_Far->Init(Root);
}

void CModule::Pulse(time_t Now) {
	m_Far->Pulse(Now);
}

bool CModule::InterceptIRCMessage(CIRCConnection* Connection, int argc, const char** argv) {
	return m_Far->InterceptIRCMessage(Connection, argc, argv);
}

bool CModule::InterceptClientMessage(CClientConnection* Connection, int argc, const char** argv) {
	return m_Far->InterceptClientMessage(Connection, argc, argv);
}

	
