/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2011 Gunnar Beutner                                      *
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

#ifndef MODULEFAR_H
#define MODULEFAR_H

class CCore;
class CIRCConnection;
class CClientConnection;

/**
 * CModuleFar
 *
 * The interface for modules.
 */
struct SBNCAPI CModuleFar {
	/**
	 * ~CModuleFar
	 *
	 * Destructor
	 */
	virtual ~CModuleFar(void) {}

	/**
	 * Destroy
	 *
	 * Destroys the module.
	 */
	virtual void Destroy(void) = 0;

	/**
	 * Init
	 *
	 * Initializes the module.
	 *
	 * @param Root a reference to the CCore object
	 */
	virtual void Init(CCore *Root) = 0;

	/**
	 * InterceptIRCMessage
	 *
	 * Called when a line is received for an IRC connection. Returns "true" if the module
	 * has handled the line.
	 *
	 * @param Connection the IRC connection
	 * @param ArgC the number of arguments
	 * @param ArgV the arguments
	 */
	virtual bool InterceptIRCMessage(CIRCConnection *Connection, int ArgC, const char **ArgV) = 0;

	/**
	 * InterceptClientMessage
	 *
	 * Called when a line is received for a client connection. Returns "true" if the module
	 * has handled the line.
	 *
	 * @param Connection the IRC connection
	 * @param ArgC the number of arguments
	 * @param ArgV the arguments
	 */
	virtual bool InterceptClientMessage(CClientConnection *Connection, int ArgC, const char **ArgV) = 0;

	/**
	 * InterceptClientCommand
	 *
	 * Called when an /sbnc command is received for a client connection. Returns "true" if the module
	 * has handled the command.
	 *
	 * @param Connection the IRC connection
	 * @param Subcommand the command
	 * @param ArgC the number of arguments
	 * @param ArgV the arguments
	 * @param NoticeUser whether to send replies as notices
	 */
	virtual bool InterceptClientCommand(CClientConnection *Connection, const char *Subcommand, int ArgC, const char **ArgV, bool NoticeUser) = 0;

	/**
	 * AttachClient
	 *
	 * Called when a user logs in.
	 *
	 * @param Client the client
	 */
	virtual void AttachClient(CClientConnection *Client) = 0;

	/**
	 * DetachClient
	 *
	 * Called when a user logs out.
	 *
	 * @param Client the client
	 */
	virtual void DetachClient(CClientConnection *Client) = 0;

	/**
	 * ServerDisconnect
	 *
	 * Called when a user disconnects from an IRC server.
	 *
	 * @param Client the name of the user
	 */
	virtual void ServerDisconnect(const char *Client) = 0;

	/**
	 * ServerConnect
	 *
	 * Called when a user connects to an IRC server.
	 *
	 * @param Client the name of the user
	 */
	virtual void ServerConnect(const char *Client) = 0;

	/**
	 * ServerLogon
	 *
	 * Called when the MOTD has been received for an IRC connection.
	 *
	 * @param Client the name of the user
	 */
	virtual void ServerLogon(const char *Client) = 0;

	/**
	 * UserLoad
	 *
	 * Called when a user's config file is loaded from disk.
	 *
	 * @param User the name of the user
	 */
	virtual void UserLoad(const char *User) = 0;

	/**
	 * UserCreate
	 *
	 * Called when a new user is being created.
	 *
	 * @param User the name of the user
	 */
	virtual void UserCreate(const char *User) = 0;

	/**
	 * UserDelete
	 *
	 * Called when a user is being removed.
	 *
	 * @param User the name of the user
	 */
	virtual void UserDelete(const char *User) = 0;

	/**
	 * SingleModeChange
	 *
	 * Called for each mode change.
	 *
	 * @param IRC the irc connection
	 * @param Channel the channel's name
	 * @param Source the source of the mode change
	 * @param Flip whether the mode is set or unset
	 * @param Mode the channel mode
	 * @param Parameter the parameter for the mode change, or NULL
	 */
	virtual void SingleModeChange(CIRCConnection *IRC, const char *Channel, const char *Source, bool Flip, char Mode, const char *Parameter) = 0;

	/**
	 * Command
	 *
	 * Used for inter-module communication. Returns a string if the command has
	 * been handled, or NULL otherwise.
	 *
	 * @param Cmd the command
	 * @param Parameters any parameters for the command
	 */
	virtual const char *Command(const char *Cmd, const char *Parameters) = 0;

	/**
	 * TagModified
	 *
	 * Called when a global tag has been modified.
	 *
	 * @param Tag the name of the tag
	 * @param Value the new value of the tag
	 */
	virtual void TagModified(const char *Tag, const char *Value) = 0;

	/**
	 * UserTagModified
	 *
	 * Called when a user's tag has been modified.
	 *
	 * @param Tag the name of the tag
	 * @param Value the new value of the tag
	 */
	virtual void UserTagModified(const char *Tag, const char *Value) = 0;

	/**
	 * MainLoop
	 *
	 * Called in every mainloop iteration. Returns "true" if the module had something to do.
	 */
	virtual bool MainLoop(void) = 0;
};

/**
 * CModuleImplementation
 *
 * A default implementation of CModuleFar.
 */
class CModuleImplementation : public CModuleFar {
private:
	CCore *m_Core;

protected:
	virtual ~CModuleImplementation(void) { }

	virtual void Destroy(void) {
		delete this;
	}

	virtual void Init(CCore *Root) {
		m_Core = Root;
	}

	virtual bool InterceptIRCMessage(CIRCConnection *Connection, int ArgC, const char **ArgV) {
		return true;
	}

	virtual bool InterceptClientMessage(CClientConnection *Connection, int ArgC, const char **ArgV) {
		return true;
	}

	virtual bool InterceptClientCommand(CClientConnection *Connection, const char *Subcommand, int ArgC, const char **ArgV, bool NoticeUser) {
		return false;
	}

	virtual void AttachClient(CClientConnection *Client) { }
	virtual void DetachClient(CClientConnection *Client) { }

	virtual void ServerDisconnect(const char *Client) { }
	virtual void ServerConnect(const char *Client) { }
	virtual void ServerLogon(const char *Client) { }

	virtual void UserLoad(const char *User) { }
	virtual void UserCreate(const char *User) { }
	virtual void UserDelete(const char *User) { }

	virtual void SingleModeChange(CIRCConnection *IRC, const char *Channel, const char *Source, bool Flip, char Mode, const char *Parameter) { }

	virtual const char *Command(const char *Cmd, const char *Parameters) {
		return NULL;
	}

	virtual void TagModified(const char *Tag, const char *Value) { }
	virtual void UserTagModified(const char *Tag, const char *Value) { }

	virtual bool MainLoop(void) {
		return false;
	}
public:
	CCore *GetCore(void) {
		return m_Core;
	}
};

#endif /* MODULEFAR_H */
