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

typedef CModuleFar *(* FNGETOBJECT)() ;
typedef int (* FNGETINTERFACEVERSION)() ;

/**
 * CModule
 *
 * A dynamically loaded shroudBNC module.
 */
class SBNCAPI CModule : public CModuleFar {
	HMODULE m_Image; /**< the os-specific module handle */
	char *m_File; /**< the filename of the module */
	CModuleFar *m_Far; /**< the module's implementation of the CModuleFar class */
	char *m_Error; /**< the last error */

	bool InternalLoad(const char *Path);
public:
#ifndef SWIG
	CModule(const char *Filename);
	~CModule(void);
#endif

	CModuleFar *GetModule(void);
	const char *GetFilename(void);
	HMODULE GetHandle(void);
	RESULT<bool> GetError(void);

	// proxy implementation of CModuleFar
	void Destroy(void);
	void Init(CCore *Root);

	bool InterceptIRCMessage(CIRCConnection *Connection, int ArgC, const char  **ArgV);
	bool InterceptClientMessage(CClientConnection *Connection, int ArgC, const char **ArgV);
	bool InterceptClientCommand(CClientConnection *Connection, const char *Subcommand, int ArgC, const char **ArgV, bool NoticeUser);

	void AttachClient(const char *Client);
	void DetachClient(const char *Client) ;

	void ServerDisconnect(const char *Client);
	void ServerConnect(const char *Client);
	void ServerLogon(const char *Client);

	void UserLoad(const char *User);
	void UserCreate(const char *User);
	void UserDelete(const char *User);

	void SingleModeChange(CIRCConnection *IRC, const char *Channel, const char *Source, bool Flip, char Mode, const char *Parameter);

	const char *Command(const char *Cmd, const char *Parameters);

	void TagModified(const char *Tag, const char *Value);
	void UserTagModified(const char *Tag, const char *Value);
};
