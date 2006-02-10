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

typedef CModuleFar* (* FNGETOBJECT)() ;
typedef int (* FNGETINTERFACEVERSION)() ;

/**
 * CModule
 *
 * A dynamically loaded shroudBNC module.
 */
class CModule : public CModuleFar {
	HMODULE m_Image; /**< the os-specific module handle */
	char *m_File; /**< the filename of the module */
	CModuleFar *m_Far; /**< the module's implementation of the CModuleFar class */
	char *m_Error; /**< the last error */
public:
#ifndef SWIG
	CModule(const char *Filename);
#endif
	virtual ~CModule(void);

	void DestroyModule(bool IgnoreDestructor = true);

	virtual CModuleFar *GetModule(void);
	virtual const char *GetFilename(void);
	virtual HMODULE GetHandle(void);
	virtual const char *GetError(void);

	// proxy implementation of CModuleFar
	virtual void Destroy(void);
	virtual void Init(CCore *Root);

	virtual bool InterceptIRCMessage(CIRCConnection *Connection, int argc, const char  **argv);
	virtual bool InterceptClientMessage(CClientConnection *Connection, int argc, const char **argv);
	virtual bool InterceptClientCommand(CClientConnection *Connection, const char *Subcommand, int argc, const char **argv, bool NoticeUser);

	virtual void AttachClient(const char *Client);
	virtual void DetachClient(const char *Client) ;

	virtual void ServerDisconnect(const char *Client);
	virtual void ServerConnect(const char *Client);
	virtual void ServerLogon(const char *Client);

	virtual void UserLoad(const char *User);
	virtual void UserCreate(const char *User);
	virtual void UserDelete(const char *User);

	virtual void SingleModeChange(CIRCConnection *IRC, const char *Channel, const char *Source, bool Flip, char Mode, const char *Parameter);

	virtual const char *Command(const char *Cmd, const char *Parameters);

	virtual void TagModified(const char *Tag, const char *Value);
	virtual void UserTagModified(const char *Tag, const char *Value);
};
