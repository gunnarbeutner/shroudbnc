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
#include "DnsEvents.h"
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

	m_Peer = Peer;
	m_PeerName = NULL;

	InternalWriteLine(":Notice!sBNC@shroud.nhq NOTICE * :*** shroudBNC" BNCVERSION);
	InternalWriteLine(":Notice!sBNC@shroud.nhq NOTICE * :*** Looking up your hostname");

//#ifdef ASYNC_DNS
	adns_submit_reverse(g_adns_State, (const sockaddr*)&m_Peer, adns_r_ptr, (adns_queryflags)0, static_cast<CDnsEvents*>(this), &m_PeerA);

	m_PeerName = NULL;
//#else
//	hostent* hent = gethostbyaddr((const char*)&Peer.sin_addr, sizeof(in_addr), AF_INET);
//
//	if (hent)
//		m_PeerName = strdup(hent->h_name);
//	else
//		m_PeerName = strdup(inet_ntoa(Peer.sin_addr));
//
//	WriteLine(":Notice!sBNC@shroud.nhq NOTICE * :*** Found your hostname (%s)", m_PeerName);
//#endif

	g_Bouncer->RegisterSocket(Client, (CSocketEvents*)this);
}

CClientConnection::~CClientConnection() {
	free(m_Nick);
	free(m_Password);
	free(m_Username);
	free(m_PeerName);
}

connection_role_e CClientConnection::GetRole(void) {
	return Role_Client;
}

bool CClientConnection::ProcessBncCommand(const char* Subcommand, int argc, const char** argv, bool NoticeUser) {
	char Out[1024];
	CBouncerUser* targUser = m_Owner;

#define SENDUSER(Text) { \
	if (NoticeUser) { \
		targUser->RealNotice(Text); \
	} else { \
		targUser->Notice(Text); \
	} \
	}

	if (argc < 1) {
		SENDUSER("Try /sbnc help");
		return false;
	}

	if (strcmpi(Subcommand, "help") == 0) {
		SENDUSER("--The following commands are available to you--");
		SENDUSER("--Used as '/sbnc <command>', or '/msg -sbnc <command>'");
		SENDUSER("--");

		if (m_Owner->IsAdmin()) {
			SENDUSER("Admin commands:");
			SENDUSER("adduser       - creates a new user");
			SENDUSER("deluser       - removes a user");
			SENDUSER("who           - shows users");
			SENDUSER("lsmod         - lists loaded modules");
			SENDUSER("insmod        - loads a module");
			SENDUSER("rmmod         - unloads a module");
			SENDUSER("simul         - simulates a command on another user's connection");
			SENDUSER("global        - sends a global notice to all bouncer users");
			SENDUSER("kill          - disconnects a user from the bouncer");
			SENDUSER("disconnect    - disconnects a user from the IRC server");
			SENDUSER("playmainlog   - plays the bouncer's log");
			SENDUSER("erasemainlog  - erases the bouncer's log");
			SENDUSER("gvhost        - sets the default/global vhost");
			SENDUSER("motd          - sets the bouncer's MOTD");
			SENDUSER("die           - terminates the bouncer");
			SENDUSER("--");
			SENDUSER("User commands:");
		}

		SENDUSER("read          - plays your message log");
		SENDUSER("erase         - erases your message log");
		SENDUSER("set           - sets configurable settings for your user");
		SENDUSER("jump          - reconnects to the IRC server");
		SENDUSER("hosts         - lists all hostmasks, which are permitted to use this account");
		SENDUSER("hostadd       - adds a hostmask");
		SENDUSER("hostdel       - removes a hostmask");

		if (m_Owner->IsAdmin()) {
			SENDUSER("status        - tells you the current status");
		}

		SENDUSER("help          - oh well, guess what..");
	}

	CModule** Modules = g_Bouncer->GetModules();
	int Count = g_Bouncer->GetModuleCount();
	bool latchedRetVal = true;

	for (int i = 0; i < Count; i++) {
		if (!Modules[i]) { continue; }

		if (Modules[i]->InterceptClientCommand(this, Subcommand, argc, argv, NoticeUser))
			latchedRetVal = false;
	}

	if (strcmpi(Subcommand, "help") == 0) {
		SENDUSER("End of HELP.");

		return false;
	}

	if (!latchedRetVal)
		return false;

	if (strcmpi(Subcommand, "lsmod") == 0 && m_Owner->IsAdmin()) {
		CModule** Modules = g_Bouncer->GetModules();
		int Count = g_Bouncer->GetModuleCount();

		for (int i = 0; i < Count; i++) {
			if (!Modules[i]) { continue; }

			snprintf(Out, sizeof(Out), "%d: 0x%x %s", i + 1, (unsigned int)Modules[i]->GetHandle(), Modules[i]->GetFilename());
			SENDUSER(Out);
		}

		SENDUSER("End of MODULES.");

		return false;
	} else if (strcmpi(Subcommand, "insmod") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: INSMOD module-path");
			return false;
		}

		CModule* Module = g_Bouncer->LoadModule(argv[1]);

		if (Module) {
			SENDUSER("Module was loaded.");
		} else {
			SENDUSER("Module could not be loaded.");
		}

		return false;
	} else if (strcmpi(Subcommand, "rmmod") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: RMMOD module-id");
			return false;
		}

		int idx = atoi(argv[1]);

		if (idx < 1 || idx > g_Bouncer->GetModuleCount()) {
			SENDUSER("There is no such module.");
		} else {
			CModule* Mod = g_Bouncer->GetModules()[idx - 1];

			if (!Mod) {
				SENDUSER("This module is already unloaded.");
			} else {
				if (g_Bouncer->UnloadModule(Mod)) {
					SENDUSER("Done.");
				} else {
					SENDUSER("Failed to unload this module.");
				}
			}
		}

		return false;
	} else if (strcmpi(Subcommand, "set") == 0) {
		CBouncerConfig* Config = m_Owner->GetConfig();
		if (argc < 3) {
			SENDUSER("Configurable settings:");
			SENDUSER("--");

			SENDUSER("password - Set");

			snprintf(Out, sizeof(Out), "vhost - %s", Config->ReadString("user.ip") ? Config->ReadString("user.ip") : "Default");
			SENDUSER(Out);

			snprintf(Out, sizeof(Out), "server - %s:%d", Config->ReadString("user.server"), Config->ReadInteger("user.port"));
			SENDUSER(Out);

			snprintf(Out, sizeof(Out), "serverpass - %s", Config->ReadString("user.spass") ? "Set" : "Not set");
			SENDUSER(Out);

			snprintf(Out, sizeof(Out), "realname - %s", m_Owner->GetRealname());
			SENDUSER(Out);

			snprintf(Out, sizeof(Out), "awaynick - %s", Config->ReadString("user.awaynick") ? Config->ReadString("user.awaynick") : "Not set");
			SENDUSER(Out);

			snprintf(Out, sizeof(Out), "away - %s", Config->ReadString("user.away") ? Config->ReadString("user.away") : "Not set");
			SENDUSER(Out);
		} else {
			if (argc > 3 && strcmpi(argv[1], "server") == 0) {
				Config->WriteString("user.server", argv[2]);
				Config->WriteInteger("user.port", atoi(argv[3]));
			} else if (strcmpi(argv[1], "realname") == 0) {
				Config->WriteString("user.realname", argv[2]);
			} else if (strcmpi(argv[1], "awaynick") == 0) {
				Config->WriteString("user.awaynick", argv[2]);
			} else if (strcmpi(argv[1], "away") == 0) {
				Config->WriteString("user.away", argv[2]);
			} else if (strcmpi(argv[1], "vhost") == 0) {
				Config->WriteString("user.ip", argv[2]);
			} else if (strcmpi(argv[1], "serverpass") == 0) {
				Config->WriteString("user.spass", argv[2]);
			} else if (strcmpi(argv[1], "password") == 0) {
				if (strlen(argv[2]) < 6 || argc > 3) {
					SENDUSER("Your password is too short or contains invalid characters.");
					return false;
				} else {
					m_Owner->SetPassword(argv[2]);
				}
			} else {
				SENDUSER("Unknown setting");
				return false;
			}

			SENDUSER("Done.");
		}

		return false;
	} else if (strcmpi(Subcommand, "die") == 0 && m_Owner->IsAdmin()) {
		g_Bouncer->Log("Shutdown requested by %s", m_Owner->GetUsername());
		g_Bouncer->Shutdown();

		return false;
	} else if (strcmpi(Subcommand, "adduser") == 0 && m_Owner->IsAdmin()) {
		if (argc < 3) {
			SENDUSER("Syntax: ADDUSER username password");
			return false;
		}

		g_Bouncer->CreateUser(argv[1], argv[2]);

		SENDUSER("Done.");

		return false;

	} else if (strcmpi(Subcommand, "deluser") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: DELUSER username");
			return false;
		}

		g_Bouncer->RemoveUser(argv[1]);

		SENDUSER("Done.");

		return false;
	} else if (strcmpi(Subcommand, "simul") == 0 && m_Owner->IsAdmin()) {
		if (argc < 3) {
			SENDUSER("Syntax: SIMUL username :command");
			return false;
		}

		CBouncerUser* User = g_Bouncer->GetUser(argv[1]);

		if (User)
			User->Simulate(argv[2]);
		else {
			snprintf(Out, sizeof(Out), "No such user: %s", argv[1]);
			SENDUSER(Out);
		}

		return false;
	} else if (strcmpi(Subcommand, "direct") == 0) {
		if (argc < 2) {
			SENDUSER("Syntax: DIRECT :command");
			return false;
		}

		CIRCConnection* IRC = m_Owner->GetIRCConnection();

		IRC->InternalWriteLine(argv[1]);

		return false;
	} else if (strcmpi(Subcommand, "gvhost") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			const char* Ip = g_Bouncer->GetConfig()->ReadString("system.ip");

			snprintf(Out, sizeof(Out), "Current global VHost: %s", Ip ? Ip : "(none)");
			SENDUSER(Out);
		} else {
			g_Bouncer->GetConfig()->WriteString("system.ip", argv[1]);
			SENDUSER("Done.");
		}

		return false;
	} else if (strcmpi(Subcommand, "motd") == 0) {
		if (argc < 2) {
			const char* Motd = g_Bouncer->GetConfig()->ReadString("system.motd");

			snprintf(Out, sizeof(Out), "Current MOTD: %s", Motd ? Motd : "(none)");
			SENDUSER(Out);
		} else if (m_Owner->IsAdmin()) {
			g_Bouncer->GetConfig()->WriteString("system.motd", argv[1]);
			SENDUSER("Done.");
		}

		return false;
	} else if (strcmpi(Subcommand, "global") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: GLOBAL :text");
			return false;
		}

		g_Bouncer->GlobalNotice(argv[1]);
		return false;
	} else if (strcmpi(Subcommand, "kill") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: BKILL username");
			return false;
		}
		
		snprintf(Out, sizeof(Out), "SIMUL %s :QUIT", argv[1]);
		ParseLine(Out);

		return false;
	} else if (strcmpi(Subcommand, "disconnect") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: BKILL username");
			return false;
		}

		snprintf(Out, sizeof(Out), "SIMUL %s :PERROR :Requested.", argv[1]);
		ParseLine(Out);

		return false;
	} else if (strcmpi(Subcommand, "jump") == 0) {
		m_Owner->Reconnect();
		return false;
	} else if (strcmpi(Subcommand, "status") == 0) {
		snprintf(Out, sizeof(Out), "Username: %s", m_Owner->GetUsername());
		SENDUSER(Out);

		snprintf(Out, sizeof(Out), "You are %san admin.", m_Owner->IsAdmin() ? "" : "not ");
		SENDUSER(Out);

		snprintf(Out, sizeof(Out), "Client: sendq: %d, recvq: %d", SendqSize(), RecvqSize());
		SENDUSER(Out);

		CIRCConnection* IRC = m_Owner->GetIRCConnection();

		if (IRC) {
			snprintf(Out, sizeof(Out), "IRC: sendq: %d, recvq: %d", IRC->SendqSize(), IRC->RecvqSize());
			SENDUSER(Out);

			CChannel** Chans = IRC->GetChannels();

			SENDUSER("Channels:");

			for (int i = 0; i < IRC->GetChannelCount(); i++) {
				if (Chans[i])
					SENDUSER(Chans[i]->GetName());
			}

			SENDUSER("End of CHANNELS.");
		}

		snprintf(Out, sizeof(Out), "Uptime: %d seconds", m_Owner->IRCUptime());
		SENDUSER(Out);

		return false;
	} else if (strcmpi(Subcommand, "who") == 0 && m_Owner->IsAdmin()) {
		CBouncerUser** Users = g_Bouncer->GetUsers();
		int lUsers = g_Bouncer->GetUserCount();

		for (int i = 0; i < lUsers; i++) {
			const char* Server, *ClientAddr;
			CBouncerUser* User = Users[i];

			if (!User)
				continue;

			if (User->GetIRCConnection())
				Server = User->GetIRCConnection()->GetServer();
			else
				Server = NULL;

			if (User->GetClientConnection())
				ClientAddr = User->GetClientConnection()->GetPeerName();
			else
				ClientAddr = NULL;

			snprintf(Out, sizeof(Out), "%s%s%s%s(%s)@%s [%s] :%s", User->IsLocked() ? "!" : "", User->IsAdmin() ? "@" : "", ClientAddr ? "*" : "", User->GetUsername(), User->GetNick(), ClientAddr ? ClientAddr : "", Server ? Server : "", User->GetRealname());
			SENDUSER(Out);
		}

		SENDUSER("End of USERS.");

		return false;
	} else if (strcmpi(Subcommand, "read") == 0) {
		m_Owner->GetLog()->PlayToUser(m_Owner, NoticeUser);

		return false;
	} else if (strcmpi(Subcommand, "erase") == 0) {
		m_Owner->GetLog()->Clear();
		SENDUSER("Done.");

		return false;
	} else if (strcmpi(Subcommand, "playmainlog") == 0 && m_Owner->IsAdmin()) {
		g_Bouncer->GetLog()->PlayToUser(m_Owner, NoticeUser);

		return false;
	} else if (strcmpi(Subcommand, "erasemainlog") == 0 && m_Owner->IsAdmin()) {
		g_Bouncer->GetLog()->Clear();
		g_Bouncer->Log("User %s erased the main log", m_Owner->GetUsername());
		SENDUSER("Done.");

		return false;
	} else if (strcmpi(Subcommand, "hosts") == 0) {
		char** Hosts = m_Owner->GetHostAllows();
		unsigned int a = 0;

		SENDUSER("Hostmasks");
		SENDUSER("---------");

		for (unsigned int i = 0; i < m_Owner->GetHostAllowCount(); i++) {
			if (Hosts[i]) {
				SENDUSER(Hosts[i]);
				a++;
			}
		}

		if (a == 0)
			SENDUSER("*");

		SENDUSER("End of HOSTS.");

		return false;
	} else if (strcmpi(Subcommand, "hostadd") == 0) {
		if (argc <= 1) {
			SENDUSER("Syntax: HOSTADD hostmask");

			return false;
		}

		char** Hosts = m_Owner->GetHostAllows();
		unsigned int a = 0;

		for (unsigned int i = 0; i < m_Owner->GetHostAllowCount(); i++) {
			if (Hosts[i])
				a++;
		}

		if (m_Owner->CanHostConnect(argv[1]) && a) {
			SENDUSER("This hostmask is already added or another hostmask supercedes it.");
		} else if (a >= 50) {
			SENDUSER("You may not add more than 50 hostmasks.");
		} else {
			m_Owner->AddHostAllow(argv[1]);

			SENDUSER("Done.");
		}

		return false;
	} else if (strcmpi(Subcommand, "hostdel") == 0 && argc > 1) {
		if (argc <= 1) {
			SENDUSER("Syntax: HOSTDEL hostmask");

			return false;
		}

		m_Owner->RemoveHostAllow(argv[1]);

		SENDUSER("Done.");

		return false;
	}
	
	if (NoticeUser)
		m_Owner->RealNotice("Unknown command. Try /sbnc help");
	else
		SENDUSER("Unknown command. Try /msg -sBNC help");

	return false;
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

	if (!m_Owner || !m_Owner->IsConnectedToIRC()) {
		if (strcmpi(Command, "nick") == 0 && argc > 1) {
			const char* Nick = argv[1];

			if (m_Nick != NULL) {
				if (strcmp(m_Nick, Nick) != 0) {
					m_Owner->GetConfig()->WriteString("user.nick", Nick);

					WriteLine(":%s!ident@sbnc NICK :%s", m_Nick, Nick);
				}
			}

			free(m_Nick);
			m_Nick = strdup(Nick);
		} else if (strcmpi(Command, "pass") == 0) {
			if (argc < 2) {
				WriteLine(":bouncer 461 %s :Not enough parameters", m_Nick);
			} else {
				m_Password = strdup(argv[1]);
			}
		} else if (strcmpi(Command, "user") == 0 && argc > 1) {
			if (m_Username) {
				WriteLine(":bouncer 462 %s :You may not reregister", m_Nick);
			} else if (!m_Password) {
				Kill("Use PASS first");
			} else {
				if (!argv[1]) {
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
		} else if (strcmpi(Command, "nick") == 0) {
			if (argc >= 2) {
				free(m_Nick);
				m_Nick = strdup(argv[1]);
				m_Owner->GetConfig()->WriteString("user.nick", argv[1]);
			}
		} else if (strcmpi(Command, "whois") == 0) {
			if (argc >= 2) {
				const char* Nick = argv[1];

				if (strcmpi("-sbnc", Nick) == 0) {
					WriteLine(":bouncer 311 %s -sBNC core bnc.server * :shroudBNC", m_Nick);
					WriteLine(":bouncer 319 %s -sBNC :/dev/null", m_Nick);
					WriteLine(":bouncer 312 %s -sBNC bnc.server :shroudBNC Server", m_Nick);
					WriteLine(":bouncer 318 %s -sBNC :End of /WHOIS list.", m_Nick);

					return false;
				}
			}
		} else if (strcmpi(Command, "perror") == 0) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: PERROR :quit-msg");
				return false;
			}

			CIRCConnection* Conn = m_Owner->GetIRCConnection();

			if (Conn)
				Conn->Kill(argv[1]);

			m_Owner->MarkQuitted();

			return false;
		} else if (strcmpi(Command, "simul") == 0 && m_Owner->IsAdmin()) {
			if (argc < 3) {
				m_Owner->Notice("Syntax: SIMUL username :command");
				return false;
			}

			CBouncerUser* User = g_Bouncer->GetUser(argv[1]);

			if (User)
				User->Simulate(argv[2]);
			else {
				snprintf(Out, sizeof(Out), "No such user: %s", argv[1]);
				m_Owner->Notice(Out);
			}

			return false;
		} else if (argc > 2 && strcmpi(Command, "privmsg") == 0 && strcmpi(argv[1], "-sbnc") == 0) {
			const char* Toks = ArgTokenize(argv[2]);
			const char** Arr = ArgToArray(Toks);

			ProcessBncCommand(Arr[0], ArgCount(Toks), Arr, false);

			ArgFreeArray(Arr);
			ArgFree(Toks);

			return false;
		} else if (strcmpi(Command, "sbnc") == 0) {
			return ProcessBncCommand(argv[1], argc - 1, &argv[1], true);
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

					if (Chan && Chan->GetCreationTime() != 0) {
						WriteLine(":%s 324 %s %s %s", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Chan->GetChanModes());
						WriteLine(":%s 329 %s %s %d", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Chan->GetCreationTime());
					} else
						IRC->WriteLine("MODE %s", argv[2]);
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

							Nicks = (char*)realloc(Nicks, (Nicks ? strlen(Nicks) : 0) + strlen(outPref) + strlen(Nick) + 2);

							if (*Nicks)
								strcat(Nicks, " ");

							strcat(Nicks, outPref);
							strcat(Nicks, Nick);

							if (strlen(Nicks) > 400) {
								WriteLine(":%s 353 %s = %s :%s", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Nicks);

								Nicks = (char*)realloc(Nicks, 1);
								*Nicks = '\0';
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
					}

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
		}
	}

	return true;
}

void CClientConnection::ParseLine(const char* Line) {
	if (strlen(Line) > 512)
		return; // protocol violation

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

	bool Blocked = true, Valid = false, ValidHost = false;

	if (User) {
		Blocked = User->IsIpBlocked(m_Peer);
		Valid = User->Validate(m_Password);
		ValidHost = User->CanHostConnect(m_PeerName);
	}

	if (m_Password && User && !Blocked && Valid && ValidHost) {
		User->Attach(this);
		//WriteLine(":Notice!sBNC@shroud.nhq NOTICE * :Welcome to the wonderful world of IRC");
	} else {
		if (User && !Blocked) {
			User->LogBadLogin(m_Peer);
		}

		if (User && !ValidHost && !Blocked) {
			g_Bouncer->Log("Attempted login from %s for %s denied: Host does not match any host allows.", inet_ntoa(m_Peer.sin_addr), m_Username);
		} else if (User && Blocked) {
			g_Bouncer->Log("Blocked login attempt from %s for %s", inet_ntoa(m_Peer.sin_addr), m_Username);
		} else if (User) {
			g_Bouncer->Log("Wrong password for user %s", m_Username);
		}

		Kill("*** Unknown user or wrong password.");
	}
}


const char* CClientConnection::GetNick(void) {
	return m_Nick;
}

void CClientConnection::Destroy(void) {
	if (m_Owner) {
		g_Bouncer->Log("%s disconnected.", m_Username);
		m_Owner->SetClientConnection(NULL);

		delete this;
	}
}

void CClientConnection::SetOwner(CBouncerUser* Owner) {
	m_Owner = Owner;
}

bool CClientConnection::ReadLine(char** Out) {
	if (m_PeerName)
		return CConnection::ReadLine(Out);
	else
		return false;
}

void CClientConnection::SetPeerName(const char* PeerName) {
	WriteLine(":Notice!sBNC@shroud.nhq NOTICE * :*** Found your hostname (%s)", PeerName);

	m_PeerName = strdup(PeerName);

	char* Line;

	while (ReadLine(&Line)) {
		ParseLine(Line);

		free(Line);
	}
}

adns_query CClientConnection::GetPeerDNSQuery(void) {
	return m_PeerA;
}

sockaddr_in CClientConnection::GetPeer(void) {
	return m_Peer;
}

const char* CClientConnection::GetPeerName(void) {
	return m_PeerName;
}

void CClientConnection::AsyncDnsFinished(adns_query* query, adns_answer* response) {
	if (response->status != adns_s_ok)
		
		SetPeerName(inet_ntoa(GetPeer().sin_addr));
	else
		SetPeerName(*response->rrs.str);
}
