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
#include "SocketEvents.h"
#include "Connection.h"
#include "ClientConnection.h"
#include "IRCConnection.h"
#include "BouncerUser.h"
#include "BouncerCore.h"
#include "BouncerLog.h"
#include "BouncerConfig.h"
#include "ModuleFar.h"
#include "Module.h"
#include "Channel.h"
#include "Hashtable.h"
#include "Nick.h"
#include "utility.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CClientConnection::CClientConnection(SOCKET Client, sockaddr_in Peer) : CConnection(Client, Peer) {
	m_Nick = NULL;
	m_Password = NULL;
	m_Username = NULL;

	g_Bouncer->RegisterSocket(Client, (CSocketEvents*)this);
}

CClientConnection::~CClientConnection() {
	free(m_Nick);
}

connection_role_e CClientConnection::GetRole(void) {
	return Role_Client;
}

bool CClientConnection::ParseLineArgV(int argc, const char** argv) {
	char Out[1024];

	CModule** Modules = g_Bouncer->GetModules();
	int Count = g_Bouncer->GetModuleCount();

	for (int i = 0; i < Count; i++) {
		if (Modules[i]) {
			if (!Modules[i]->InterceptClientMessage(this, argc, argv))
				return false;
		}
	}

	const char* Command = argv[0];
	const char* Args = argv[1];

	if (!m_Owner || !m_Owner->IsConnectedToIRC()) {
		if (strcmpi(Command, "nick") == 0) {
			const char* Nick = Args;

			if (m_Nick == NULL) {
				InternalWriteLine(":Notice!sBNC@shroud.nhq NOTICE * :shroudBNC0.1");
			} else {
				if (strcmp(m_Nick, Nick) != 0) {
					m_Owner->GetConfig()->WriteString("user.nick", Nick);

					WriteLine(":%s!ident@sbnc NICK :%s", m_Nick, Nick);
				}
			}

			free(m_Nick);
			m_Nick = strdup(Nick);
		} else if (strcmpi(Command, "pass") == 0) {
			if (!Args) {
				WriteLine(":bouncer 461 %s :Not enough parameters", m_Nick);
			} else {
				m_Password = strdup(Args);
			}
		} else if (strcmpi(Command, "user") == 0) {
			if (m_Username) {
				WriteLine(":bouncer 462 %s :You may not reregister", m_Nick);
			} else if (!m_Password) {
				Kill("Use PASS first");
			} else {
				if (!Args) {
					WriteLine(":bouncer 461 %s :Not enough parameters", m_Nick);
				} else {
					const char* Username = argv[1];

					m_Username = strdup(Username);
				}
			}

			ValidateUser();

			return false;
/*		} else if (strcmpi(Command, "whois") == 0) {
			char* Nick = LastArg(Args);

			if (strcmpi(m_Nick, Nick) == 0) {
				HOSTENT* hent = gethostbyaddr((const char*)GetPeer(), sizeof(in_addr), AF_INET);

				wsprintf(Out, ":bouncer 311 %s %s ident %s * :shroudBNC User", m_Nick, Nick, hent->h_name);
				WriteLine(Out);

				wsprintf(Out, ":bouncer 312 %s %s :shroudBNC Server", m_Nick, Nick);
				WriteLine(Out);

				wsprintf(Out, ":bouncer 318 %s %s :End of /WHOIS list.", m_Nick, Nick);
				WriteLine(Out);
			} else {
				wsprintf(Out, ":bouncer 402 %s %s :No such server", m_Nick, Nick);
				WriteLine(Out);
			}
*/
		}
	}
	
	if (m_Owner) {
		if (strcmpi(Command, "quit") == 0) {
			Kill("Thanks for flying with shroudBNC :P");
			return false;
		} else if (strcmpi(Command, "perror") == 0) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: PERROR :quit-msg");
				return false;
			}

			CIRCConnection* Conn = m_Owner->GetIRCConnection();

			if (Conn)
				Conn->Kill(argv[1]);

			return false;
		} else if (strcmpi(Command, "global") == 0 && m_Owner->IsAdmin()) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: GLOBAL :text");
				return false;
			}

			g_Bouncer->GlobalNotice(Args);
			return false;
		} else if (strcmpi(Command, "simul") == 0 && m_Owner->IsAdmin()) {
			if (argc < 3) {
				m_Owner->Notice("Syntax: SIMUL username :command");
				return false;
			}

			CBouncerUser* User = g_Bouncer->GetUser(Args);

			if (User)
				User->Simulate(argv[2]);
			else {
				snprintf(Out, sizeof(Out), "No such user: %s", Args);
				m_Owner->Notice(Out);
			}

			return false;
		} else if (strcmpi(Command, "bkill") == 0 && m_Owner->IsAdmin()) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: BKILL username");
				return false;
			}
			
			snprintf(Out, sizeof(Out), "SIMUL %s :QUIT", Args);
			ParseLine(Out);

			return false;
		} else if (strcmpi(Command, "bdisconnect") == 0 && m_Owner->IsAdmin()) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: BKILL username");
				return false;
			}

			snprintf(Out, sizeof(Out), "SIMUL %s :PERROR :Requested.", Args);
			ParseLine(Out);

			return false;
		} else if (strcmpi(Command, "jump") == 0) {
			m_Owner->Reconnect();
			return false;
		} else if (strcmpi(Command, "status") == 0) {
			snprintf(Out, sizeof(Out), "Username: %s", m_Owner->GetUsername());
			m_Owner->Notice(Out);

			snprintf(Out, sizeof(Out), "You are %san admin.", m_Owner->IsAdmin() ? "" : "not ");
			m_Owner->Notice(Out);

			snprintf(Out, sizeof(Out), "Client: sendq: %d, recvq: %d", SendqSize(), RecvqSize());
			m_Owner->Notice(Out);

			CIRCConnection* IRC = m_Owner->GetIRCConnection();

			if (IRC) {
				snprintf(Out, sizeof(Out), "IRC: sendq: %d, recvq: %d", IRC->SendqSize(), IRC->RecvqSize());
				m_Owner->Notice(Out);

				CChannel** Chans = IRC->GetChannels();

				m_Owner->Notice("Channels:");

				for (int i = 0; i < IRC->GetChannelCount(); i++) {
					if (Chans[i])
						m_Owner->Notice(Chans[i]->GetName());
				}

				m_Owner->Notice("End of CHANNELS.");
			}

			snprintf(Out, sizeof(Out), "Uptime: %d seconds", m_Owner->IRCUptime());
			m_Owner->Notice(Out);

			return false;
		} else if (strcmpi(Command, "bwho") == 0) {
			CIRCConnection* IRC = m_Owner->GetIRCConnection();

			CBouncerUser** Users = g_Bouncer->GetUsers();
			int lUsers = g_Bouncer->GetUserCount();

			for (int i = 0; i < lUsers; i++) {
				const char* Server, *ClientAddr;
				CBouncerUser* User = Users[i];

				if (!User)
					continue;

				if (User->GetIRCConnection()) {
					Server = User->GetIRCConnection()->GetServer();
				} else {
					Server = NULL;
				}

				if (User->GetClientConnection()) {
					SOCKET Sock = User->GetClientConnection()->GetSocket();

					sockaddr_in saddr;
					socklen_t saddr_size = sizeof(saddr);

					getpeername(Sock, (sockaddr*)&saddr, &saddr_size);

					hostent* hent = gethostbyaddr((const char*)&saddr.sin_addr, sizeof(in_addr), AF_INET);

					if (hent)
						ClientAddr = hent->h_name;
					else
						ClientAddr = inet_ntoa(saddr.sin_addr);
				} else
					ClientAddr = NULL;

				if (Users[i]) {
					snprintf(Out, sizeof(Out), "%s%s%s%s(%s)@%s [%s] :%s", User->IsLocked() ? "!" : "", User->IsAdmin() ? "@" : "", ClientAddr ? "*" : "", User->GetUsername(), User->GetNick(), ClientAddr ? ClientAddr : "", Server ? Server : "", User->GetRealname());
					m_Owner->Notice(Out);
				}
			}

			return false;
		} else if (strcmpi(Command, "playprivatelog") == 0) {
			m_Owner->GetLog()->PlayToUser(m_Owner);

			return false;
		} else if (strcmpi(Command, "eraseprivatelog") == 0) {
			m_Owner->GetLog()->Clear();
			m_Owner->Notice("Done.");

			return false;
		} else if (strcmpi(Command, "playmainlog") == 0 && m_Owner->IsAdmin()) {
			g_Bouncer->GetLog()->PlayToUser(m_Owner);

			return false;
		} else if (strcmpi(Command, "erasemainlog") == 0 && m_Owner->IsAdmin()) {
			g_Bouncer->GetLog()->Clear();
			m_Owner->Notice("Done.");

			return false;
		} else if (strcmpi(Command, "nick") == 0) {
			if (argc >= 2) {
				free(m_Nick);
				m_Nick = strdup(Args);
				m_Owner->GetConfig()->WriteString("user.nick", Args);
			}
		} else if (strcmpi(Command, "whois") == 0) {
			if (argc >= 2) {
				const char* Nick = Args;

				if (strcmpi("-sbnc", Nick) == 0) {
					WriteLine(":bouncer 311 %s -sBNC core bnc.server * :shroudBNC", m_Nick);
					WriteLine(":bouncer 319 %s -sBNC :/dev/null", m_Nick);
					WriteLine(":bouncer 312 %s -sBNC bnc.server :shroudBNC Server", m_Nick);
					WriteLine(":bouncer 318 %s -sBNC :End of /WHOIS list.", m_Nick);

					return false;
				}
			}
		} else if (strcmpi(Command, "privmsg") == 0 && Args && strcmpi(Args, "-sbnc") == 0) {
			char* Cmd = strdup(argv[2]);
			ParseLine(Cmd);
			free(Cmd);

			return false;
		} else if (strcmpi(Command, "password") == 0) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: PASSWORD new-password");
				return false;
			}

			if (strlen(Args) < 6 || strstr(Args, " ") || argv[2]) {
				m_Owner->Notice("Your password is too short or contains invalid characters.");
				return false;
			}

			m_Owner->SetPassword(Args);
			m_Owner->Notice("Password changed.");

			return false;
		} else if (strcmpi(Command, "bhelp") == 0) {
			CBouncerUser* User = m_Owner;

			User->Notice("sBNC0.1 - an object-oriented IRC bouncer - Help");
			User->Notice("------------");
			User->Notice("Admin commands:");
			User->Notice("ADDUSER - creates a new user");
			User->Notice("DELUSER - removes a user");
			User->Notice("LSMOD - lists loaded modules");
			User->Notice("INSMOD - loads a module");
			User->Notice("RMMOD - unloads a module");
			User->Notice("SIMUL - simulates a command on another user's connection");
			User->Notice("GLOBAL - sends a global notice to all bouncer users");
			User->Notice("BKILL - disconnects a user from the bouncer");
			User->Notice("BDISCONNECT - disconnects a user from the IRC server");
			User->Notice("PLAYMAINLOG - plays the bouncer's log");
			User->Notice("ERASEMAINLOG - erases the bouncer's log");
			User->Notice("BDIE - terminates the bouncer");
			User->Notice("------------");
			User->Notice("User commands:");
			User->Notice("BWHO - shows users");
			User->Notice("STATUS - tells you the current status");
			User->Notice("PLAYPRIVATELOG - plays your message log");
			User->Notice("ERASEPRIVATELOG - erases your message log");
			User->Notice("PASSWORD - changes your password");
			User->Notice("QUIT - disconnects you from the bouncer");
			User->Notice("PERROR - disconnects you from the IRC server");
			User->Notice("SETSERVER - sets your default server");
			User->Notice("SETGECOS - sets your realname");
			User->Notice("SETAWAYNICK - sets your awaynick");
			User->Notice("JUMP - reconnects to the IRC server");
			User->Notice("SYNTH - synthesizes the reply for an ordinary irc-command");
			User->Notice("DIRECT - sends a raw command (bypassing SYNTH)");
			User->Notice("BHELP - oh well, guess what..");
			User->Notice("End of HELP.");

			return false;
		} else if (strcmpi(Command, "lsmod") == 0 && m_Owner->IsAdmin()) {
			CModule** Modules = g_Bouncer->GetModules();
			int Count = g_Bouncer->GetModuleCount();

			for (int i = 0; i < Count; i++) {
				if (!Modules[i]) { continue; }

				snprintf(Out, sizeof(Out), "%d: 0x%x %s", i + 1, Modules[i]->GetHandle(), Modules[i]->GetFilename());
				m_Owner->Notice(Out);
			}

			m_Owner->Notice("End of MODULES.");

			return false;
		} else if (strcmpi(Command, "insmod") == 0 && m_Owner->IsAdmin()) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: INSMOD module-path");
				return false;
			}

			CModule* Module = g_Bouncer->LoadModule(Args);

			if (Module)
				m_Owner->Notice("Module was loaded.");
			else
				m_Owner->Notice("Module could not be loaded.");

			return false;
		} else if (strcmpi(Command, "rmmod") == 0 && m_Owner->IsAdmin()) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: RMMOD module-id");
				return false;
			}

			int idx = atoi(Args);

			if (idx < 1 || idx > g_Bouncer->GetModuleCount())
				m_Owner->Notice("There is no such module.");
			else {
				CModule* Mod = g_Bouncer->GetModules()[idx - 1];

				if (!Mod)
					m_Owner->Notice("This module is already unloaded.");
				else {
					if (g_Bouncer->UnloadModule(Mod))
						m_Owner->Notice("Done.");
					else
						m_Owner->Notice("Failed to unload this module.");
				}
			}

			return false;
		} else if (strcmpi(Command, "setserver") == 0) {
			if (argc < 3) {
				m_Owner->Notice("Syntax: SETSERVER hostname port");
				return false;
			}

			const char* Server = argv[1];
			int Port = atoi(argv[2]);

			m_Owner->GetConfig()->WriteString("user.server", Server);
			m_Owner->GetConfig()->WriteInteger("user.port", Port);

			m_Owner->Notice("Done. Use JUMP to connect to the new server.");

			return false;
		} else if (strcmpi(Command, "setgecos") == 0) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: SETGECOS :real name");
				return false;
			}

			m_Owner->GetConfig()->WriteString("user.realname", Args);

			m_Owner->Notice("Done. Use JUMP to active your new gecos information.");

			return false;
		} else if (strcmpi(Command, "bdie") == 0 && m_Owner->IsAdmin()) {
			g_Bouncer->Log("Shutdown requested by %s", m_Owner->GetUsername());
			g_Bouncer->Shutdown();
		} else if (strcmpi(Command, "adduser") == 0 && m_Owner->IsAdmin()) {
			if (argc < 3) {
				m_Owner->Notice("Syntax: ADDUSER username password");
				return false;
			}

			g_Bouncer->CreateUser(argv[1], argv[2]);

			m_Owner->Notice("Done.");

			return false;

		} else if (strcmpi(Command, "deluser") == 0 && m_Owner->IsAdmin()) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: DELUSER username");
				return false;
			}

			g_Bouncer->RemoveUser(argv[1]);

			m_Owner->Notice("Done.");

			return false;
		} else if (strcmpi(Command, "synth") == 0) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: SYNTH command parameter");
				m_Owner->Notice("supported commands are: mode, topic, names, version");

				return false;
			}

			if (strcmpi(argv[1], "mode") == 0 && argc > 2) {
				CIRCConnection* IRC = m_Owner->GetIRCConnection();

				if (IRC) {
					CChannel* Chan = IRC->GetChannel(argv[2]);

					if (Chan) {
						if (Chan->GetCreationTime() != 0) {
							WriteLine(":%s 324 %s %s %s", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Chan->GetChanModes());
							WriteLine(":%s 329 %s %s %d", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Chan->GetCreationTime());
						} else {
							IRC->WriteLine("MODE %s", argv[2]);
						}
					}
				}
			} else if (strcmpi(argv[1], "topic") == 0 && argc > 2) {
				CIRCConnection* IRC = m_Owner->GetIRCConnection();

				if (IRC) {
					CChannel* Chan = IRC->GetChannel(argv[2]);

					if (Chan && Chan->HasTopic() != 0) {
						if (Chan->GetTopic() && *(Chan->GetTopic())) {
							WriteLine(":%s 332 %s %s :%s", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Chan->GetTopic());
							WriteLine(":%s 333 %s %s %s %d", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Chan->GetTopicNick(), Chan->GetTopicStamp());
						}
					} else {
						IRC->WriteLine("TOPIC %s", argv[2]);
					}
				}
			} else if (strcmpi(argv[1], "names") == 0 && argc > 2) {
				CIRCConnection* IRC = m_Owner->GetIRCConnection();

				if (IRC) {
					CChannel* Chan = IRC->GetChannel(argv[2]);

					if (Chan && Chan->HasNames() != 0) {
						char* Nicks = (char*)malloc(1);
						Nicks[0] = '\0';

						CHashtable<CNick*, false>* H = Chan->GetNames();

						int a = 0;

						while (hash_t<CNick*>* NickHash = H->Iterate(a++)) {
							CNick* NickObj = NickHash->Value;

							const char* Prefix = NickObj->GetPrefixes();
							const char* Nick = NickObj->GetNick();

							char outPref[2] = { Chan->GetHighestUserFlag(Prefix), '\0' };

							Nicks = (char*)realloc(Nicks, (Nicks ? strlen(Nicks) : 0) + (*outPref ? strlen(Prefix) : 0) + strlen(Nick) + 2);

							if (*Nicks)
								strcat(Nicks, " ");

							if (*outPref)
								strcat(Nicks, Prefix);

							strcat(Nicks, Nick);

							if (a == 40) {
								WriteLine(":%s 353 %s @ %s :%s", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Nicks);

								Nicks = (char*)realloc(Nicks, 1);
								*Nicks = '\0';
								a = 0;
							}
						}

						if (a) {
							WriteLine(":%s 353 %s = %s :%s", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Nicks);
						}

						free(Nicks);

						WriteLine(":%s 366 %s %s :End of /NAMES list.", IRC->GetServer(), IRC->GetCurrentNick(), argv[2]);
					} else {
						IRC->WriteLine("NAMES %s", argv[2]);
					}
				}
			} else if (strcmpi(argv[1], "version") == 0) {
				CIRCConnection* IRC = m_Owner->GetIRCConnection();

				if (IRC) {
					const char* ServerVersion = IRC->GetServerVersion();
					const char* ServerFeat = IRC->GetServerFeat();

					if (ServerVersion != NULL && ServerFeat != NULL) {
						WriteLine(":%s 351 %s %s %s :%s", IRC->GetServer(), IRC->GetCurrentNick(), ServerVersion, IRC->GetServer(), ServerFeat);

						hash_t<char*>* Features = IRC->GetISupportAll()->GetSettings();
						int FeatureCount = IRC->GetISupportAll()->GetSettingCount();

						char* Feats = (char*)malloc(1);
						Feats[0] = '\0';

						int a = 0;

						for (int i = 0; i < FeatureCount; i++) {
							if (!Features[i].Name || !Features[i].Value)
								continue;

							char* Name = Features[i].Name;
							char* Value = Features[i].Value;

							Feats = (char*)realloc(Feats, (Feats ? strlen(Feats) : 0) + strlen(Name) + 1 + strlen(Value) + 2);

							if (*Feats)
								strcat(Feats, " ");

							strcat(Feats, Name);
							strcat(Feats, "=");
							strcat(Feats, Value);

							if (++a == 11) {
								WriteLine(":%s 005 %s %s :are supported by this server", IRC->GetServer(), IRC->GetCurrentNick(), Feats);

								Feats = (char*)realloc(Feats, 1);
								*Feats = '\0';
								a = 0;
							}
						}

						if (a)
							WriteLine(":%s 005 %s %s :are supported by this server", IRC->GetServer(), IRC->GetCurrentNick(), Feats);

						free(Feats);
					} else
						IRC->WriteLine("VERSION");
				}
			}

			return false;
		} else if (strcmpi(Command, "mode") == 0 || strcmpi(Command, "topic") == 0 || strcmpi(Command, "names") == 0) {
			if (argc == 2) {
				snprintf(Out, sizeof(Out), "SYNTH %s %s", argv[0], argv[1]);
				ParseLine(Out);

				return false;
			}
		} else if (strcmpi(Command, "version") == 0) {
			ParseLine("SYNTH VERSION");

			return false;
		} else if (strcmpi(Command, "setawaynick") == 0) {
			m_Owner->GetConfig()->WriteString("user.awaynick", argc == 1 ? NULL : argv[1]);

			m_Owner->Notice("Done.");

			return false;
		} else if (strcmpi(Command, "direct") == 0) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: DIRECT :command");
				return false;
			}

			CIRCConnection* IRC = m_Owner->GetIRCConnection();

			IRC->InternalWriteLine(argv[1]);

			return false;
		}
	}

	return true;
}

void CClientConnection::ParseLine(const char* Line) {
	const char* Args = ArgTokenize(Line);
	const char** argv = ArgToArray(Args);
	int argc = ArgCount(Args);

	bool Ret = ParseLineArgV(argc, argv);

	ArgFreeArray(argv);
	ArgFree(Args);

	if (m_Owner && Ret) {
		CIRCConnection* IRC = m_Owner->GetIRCConnection();

		if (IRC)
			IRC->InternalWriteLine(Line);
	}
}

void CClientConnection::ValidateUser(void) {
	CBouncerUser* User = g_Bouncer->GetUser(m_Username);

	if (m_Password && User && User->Validate(m_Password)) {
		User->Attach(this);
		//WriteLine(":Notice!sBNC@shroud.nhq NOTICE * :Welcome to the wonderful world of IRC");
	} else {
		g_Bouncer->Log("Wrong password for user %s", m_Username);

		Kill("Unknown user or wrong password.");
	}
}


const char* CClientConnection::GetNick(void) {
	return m_Nick;
}

void CClientConnection::Destroy(void) {
	if (m_Owner) {
		g_Bouncer->Log("%s disconnected.", m_Username);
		m_Owner->SetClientConnection(NULL);
	}

	delete this;
}
