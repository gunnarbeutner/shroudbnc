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

#if !defined(AFX_MODULE_H__E4E050DD_057C_4E38_BDB7_A875F73A9E8F__INCLUDED_)
#define AFX_MODULE_H__E4E050DD_057C_4E38_BDB7_A875F73A9E8F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef CModuleFar* (* FNGETOBJECT)() ;

class CModule : public CModuleFar {
	HMODULE m_Image;
	char* m_File;
	CModuleFar* m_Far;
public:
	CModule(const char* Filename);
	virtual ~CModule();

	virtual CModuleFar* GetModule(void);
	virtual const char* GetFilename(void);
	virtual HMODULE CModule::GetHandle(void);

	// proxy implementation of CModuleFar
	virtual void Destroy(void);

	virtual void Init(CBouncerCore* Root);
	virtual void Pulse(time_t Now);

	virtual bool InterceptIRCMessage(CIRCConnection* Connection, int argc, const char** argv);
	virtual bool InterceptClientMessage(CClientConnection* Connection, int argc, const char** argv);

	virtual void AttachClient(const char* Client);
	virtual void DetachClient(const char* Client) ;

	virtual void ServerDisconnect(const char* Client);
	virtual void ServerConnect(const char* Client);
	virtual void ServerLogon(const char* Client);

	virtual void UserLoad(const char* User);
	virtual void UserCreate(const char* User);
	virtual void UserDelete(const char* User);

	virtual void SingleModeChange(CIRCConnection* IRC, const char* Channel, const char* Source, bool Flip, char Mode, const char* Parameter);

	virtual const char* Command(const char* Cmd, const char* Parameters);
};

#endif // !defined(AFX_MODULE_H__E4E050DD_057C_4E38_BDB7_A875F73A9E8F__INCLUDED_)
