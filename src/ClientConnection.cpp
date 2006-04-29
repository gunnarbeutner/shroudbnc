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

IMPL_DNSEVENTPROXY(CClientConnection, AsyncDnsFinishedClient)

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CClientConnection::CClientConnection(SOCKET Client, bool SSL) : CConnection(Client, SSL, Role_Server) {
	m_Nick = NULL;
	m_Password = NULL;
	m_Username = NULL;
	m_PreviousNick = NULL;
	m_PeerName = NULL;
	m_PeerNameTemp = NULL;
	m_ClientLookup = NULL;
	m_CommandList = NULL;

	if (g_Bouncer->GetStatus() == STATUS_PAUSE) {
		Kill("Sorry, no new connections can be accepted at this moment. Please try again later.");

		return;
	}

	if (Client != INVALID_SOCKET) {
		WriteLine(":Notice!notice@shroudbnc.info NOTICE * :*** shroudBNC %s - Copyright (C) 2005 Gunnar Beutner", g_Bouncer->GetBouncerVersion());

		m_ClientLookup = new CDnsQuery(this, USE_DNSEVENTPROXY(CClientConnection, AsyncDnsFinishedClient));

		sockaddr *Remote = GetRemoteAddress();

		if (Remote == NULL) {
			Kill("Internal error: GetRemoteAddress() failed. Could not look up your hostname.");

			return;
		}

		WriteLine(":Notice!notice@shroudbnc.info NOTICE * :*** Doing reverse DNS lookup on %s...", IpToString(Remote));

		m_ClientLookup->GetHostByAddr(Remote);
	}

	m_AuthTimer = new CTimer(30, false, ClientAuthTimer, this);
}

CClientConnection::CClientConnection(void) : CConnection(INVALID_SOCKET, false, Role_Server) {
	m_Nick = NULL;
	m_Password = NULL;
	m_Username = NULL;
	m_PeerName = NULL;
	m_PreviousNick = NULL;
	m_ClientLookup = NULL;
	m_AuthTimer = NULL;
}

CClientConnection::~CClientConnection() {
	free(m_Nick);
	free(m_Password);
	free(m_Username);
	free(m_PeerName);
	free(m_PreviousNick);

	delete m_ClientLookup;
	delete m_AuthTimer;
}

bool CClientConnection::ProcessBncCommand(const char* Subcommand, int argc, const char** argv, bool NoticeUser) {
	char *Out;
	CUser *targUser = m_Owner;
	const CVector<CModule *> *Modules;
	bool latchedRetVal = true;

	Modules = g_Bouncer->GetModules();

#define SENDUSER(Text) \
	do { \
		if (NoticeUser) { \
			targUser->RealNotice(Text); \
		} else { \
			targUser->Notice(Text); \
		} \
	} while (0)

	if (argc < 1) {
		if (NoticeUser) {
			SENDUSER("You need to specify a command. Try /sbnc help");
		} else {
			SENDUSER("You need to specify a command. Try /msg -sBNC help");
		}

		return false;
	}

	if (strcasecmp(Subcommand, "help") == 0) {
		if (argc <= 1) {
			SENDUSER("--The following commands are available to you--");
			SENDUSER("--Used as '/sbnc <command>', or '/msg -sbnc <command>'");
		}

		FlushCommands(&m_CommandList);

		if (m_Owner->IsAdmin()) {
			AddCommand(&m_CommandList, "adduser", "Admin", "creates a new user",
				"Syntax: adduser <username> <password>\nCreates a new user.");
			AddCommand(&m_CommandList, "deluser", "Admin", "removes a user",
				"Syntax: deluser <username>\nDeletes a user.");
			AddCommand(&m_CommandList, "resetpass", "Admin", "sets a user's password",
				"Syntax: resetpass <user> <password>\nResets another user's password.");
			AddCommand(&m_CommandList, "who", "Admin", "shows users",
				"Syntax: who\nShows a list of all users.\nFlags (which are displayed in front of the username):\n"
				"@ user is an admin\n* user is currently logged in\n! user is suspended");
			AddCommand(&m_CommandList, "admin", "Admin", "gives someone admin privileges",
				"Syntax: admin <username>\nGives admin privileges to a user.");
			AddCommand(&m_CommandList, "unadmin", "Admin", "removes someone's admin privileges",
				"Syntax: unadmin <username>\nRemoves someone's admin privileges.");
			AddCommand(&m_CommandList, "suspend", "Admin", "suspends a user",
				"Syntax: suspend <username> [reason]\nSuspends an account. An optional reason can be specified.");
			AddCommand(&m_CommandList, "unsuspend", "Admin", "unsuspends a user",
				"Syntax: unsuspend <username>\nRemoves a suspension from the specified account.");
			AddCommand(&m_CommandList, "lsmod", "Admin", "lists loaded modules",
				"Syntax: lsmod\nLists all currently loaded modules.");
			AddCommand(&m_CommandList, "insmod", "Admin", "loads a module",
				"Syntax: insmod <filename>\nLoads a module.");
			AddCommand(&m_CommandList, "rmmod", "Admin", "unloads a module",
				"Syntax: rmmod <index>\nUnloads a module. Use the \"lsmod\" command to list all modules.");
			AddCommand(&m_CommandList, "simul", "Admin", "simulates a command on another user's connection",
				"Syntax: simul <username> <command>\nExecutes a command in another user's context.");
			AddCommand(&m_CommandList, "global", "Admin", "sends a global notice to all bouncer users",
				"Syntax: global <text>\nSends a notice to all currently connected users.");
			AddCommand(&m_CommandList, "kill", "Admin", "disconnects a user from the bouncer",
				"Syntax: kill <username>\nDisconnects a user from the bouncer.");
			AddCommand(&m_CommandList, "disconnect", "Admin", "disconnects a user from the irc server",
				"Syntax: disconnect [username]\nDisconnects a user from the IRC server which he is currently connected to."
				" If you don't specify a username, your own IRC connection will be closed.");
			AddCommand(&m_CommandList, "playmainlog", "Admin", "plays the bouncer's log",
				"Syntax: playmainlog\nDisplays the bouncer's log.");
			AddCommand(&m_CommandList, "erasemainlog", "Admin", "erases the bouncer's log",
				"Syntax: erasemainlog\nErases the bouncer's log.");
			AddCommand(&m_CommandList, "gvhost", "Admin", "sets the default/global vhost",
				"Syntax: gvhost <host>\nSets the bouncer's default vhost.");
			AddCommand(&m_CommandList, "motd", "Admin", "sets the bouncer's motd",
				"Syntax: motd [text]\nShows or modifies the motd.");
			AddCommand(&m_CommandList, "reload", "Admin", "reloads shroudBNC",
				"Syntax: reload [filename]\nReloads shroudBNC (a new .so module file can be specified).");
			AddCommand(&m_CommandList, "die", "Admin", "terminates the bouncer",
				"Syntax: die\nTerminates the bouncer.");
			AddCommand(&m_CommandList, "hosts", "Admin", "lists all hostmasks, which are permitted to use this bouncer",
				"Syntax: hosts\nLists all hosts which are permitted to use this bouncer.");
			AddCommand(&m_CommandList, "hostadd", "Admin", "adds a hostmask",
				"Syntax: hostadd <host>\nAdds a host to the bouncer's hostlist. E.g. *.tiscali.de");
			AddCommand(&m_CommandList, "hostdel", "Admin", "removes a hostmask",
				"Syntax: hostdel <host>\nRemoves a host from the bouncer's hostlist.");
			AddCommand(&m_CommandList, "addlistener", "Admin", "creates an additional listener",
				"Syntax: addlistener <port> [address] [ssl]\nCreates an additional listener which can be used by clients.");
			AddCommand(&m_CommandList, "dellistener", "Admin", "removes a listener",
				"Syntax: dellistener <port>\nRemoves a listener.");
			AddCommand(&m_CommandList, "listeners", "Admin", "lists all listeners",
				"Syntax: listeners\nLists all listeners.");
		}

		AddCommand(&m_CommandList, "read", "User", "plays your message log",
			"Syntax: read\nDisplays your private log.");
		AddCommand(&m_CommandList, "erase", "User", "erases your message log",
			"Syntax: erase\nErases your private log.");
		AddCommand(&m_CommandList, "set", "User", "sets configurable options for your user",
			"Syntax: set [option] [value]\nDisplays or changes configurable options for your user.");
		AddCommand(&m_CommandList, "unset", "User", "restores the default value of an option",
			"Syntax: unset <option>\nRestores the default value of an option.");
		AddCommand(&m_CommandList, "jump", "User", "reconnects to the irc server",
			"Syntax: jump\nReconnects to the irc server.");
		AddCommand(&m_CommandList, "partall", "User", "parts all channels and tells shroudBNC not to rejoin them when you reconnect to a server",
			"Syntax: partall\nParts all channels and tells shroudBNC not to rejoin any channels when you reconnect to a"
			" server.\nThis might be useful if you get disconnected due to an \"Max sendq exceeded\" error.");
		if (!m_Owner->IsAdmin()) {
			AddCommand(&m_CommandList, "disconnect", "Admin", "disconnects a user from the irc server",
				"Syntax: disconnect\nDisconnects you from the irc server.");
		}
#ifdef USESSL
		AddCommand(&m_CommandList, "savecert", "User", "saves your current client certificate for use with public key authentication",
			"Syntax: savecert\nSaves your current client certificate for use with public key authentication.\n"
			"Once you have saved your certificate you can use it for logging in without a password.");
		AddCommand(&m_CommandList, "delcert", "User", "removes a certificate",
			"Syntax: delcert <id>\nRemoves the specified certificate.");
		AddCommand(&m_CommandList, "showcert", "User", "shows information about your certificates",
			"Syntax: showcert\nShows a list of certificates which can be used for logging in.");
#endif

		if (m_Owner->IsAdmin()) {
			AddCommand(&m_CommandList, "status", "User", "tells you the current status",
				"Syntax: status\nDisplays information about your user. This command is used for debugging.");
		}

		AddCommand(&m_CommandList, "help", "User", "displays a list of commands or information about individual commands",
			"Syntax: help [command]\nDisplays a list of commands or information about individual commands.");
	}

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
		if (Modules->Get(i)->InterceptClientCommand(this, Subcommand, argc, argv, NoticeUser)) {
			latchedRetVal = false;
		}
	}

	if (strcasecmp(Subcommand, "help") == 0) {
		if (argc <= 1) {
			// show help
			hash_t<command_t *> *Hash;
			hash_t<command_t *> *CommandList;
			int i = 0;
			size_t Align = 0, Len;

			CommandList = (hash_t<command_t *> *)malloc(sizeof(hash_t<command_t *>) * m_CommandList->GetLength());

			while ((Hash = m_CommandList->Iterate(i++)) != NULL) {
				CommandList[i - 1] = *Hash;

				Len = strlen(Hash->Name);

				if (Len > Align) {
					Align = Len;
				}
			}

			qsort(CommandList, m_CommandList->GetLength(), sizeof(hash_t<command_t *>), CmpCommandT);

			char *Category = NULL;
			char *Format;

			asprintf(&Format, "%%-%ds - %%s", Align);

			for (unsigned int i = 0; i < m_CommandList->GetLength(); i++) {
				if (Category == NULL || strcasecmp(CommandList[i].Value->Category, Category) != 0) {
					if (Category)
						SENDUSER("--");

					Category = CommandList[i].Value->Category;

					asprintf(&Out, "%s commands", Category);
					SENDUSER(Out);
					free(Out);
				}

				asprintf(&Out, Format, CommandList[i].Name, CommandList[i].Value->Description);
				SENDUSER(Out);
				free(Out);
			}

			free(Format);
			free(CommandList);

			SENDUSER("End of HELP.");
		} else {
			command_t *Command = m_CommandList->Get(argv[1]);

			if (Command == NULL) {
				SENDUSER("There is no such command.");
			} else if (Command && Command->HelpText == NULL) {
				SENDUSER("No help is available for this command.");
			} else {
				char *Help = strdup(Command->HelpText);
				char *HelpBase = Help;

				while (true) {
					char *NextLine = strstr(Help, "\n");

					if (NextLine) {
						NextLine[0] = '\0';
						NextLine++;
					}

					SENDUSER(Help);

					if (NextLine == NULL)
						break;
					else
						Help = NextLine;
				}

				free(HelpBase);
			}
		}

		FlushCommands(&m_CommandList);

		return false;
	}

	if (!latchedRetVal) {
		return false;
	}

	if (strcasecmp(Subcommand, "lsmod") == 0 && m_Owner->IsAdmin()) {
		for (unsigned int i = 0; i < Modules->GetLength(); i++) {
			asprintf(&Out, "%d: %x %s", i + 1, Modules->Get(i)->GetHandle(), Modules->Get(i)->GetFilename());

			CHECK_ALLOC_RESULT(Out, asprintf) {
				return false;
			} CHECK_ALLOC_RESULT_END;

			SENDUSER(Out);
			free(Out);
		}

		SENDUSER("End of MODULES.");

		return false;
	} else if (strcasecmp(Subcommand, "insmod") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: INSMOD module-path");
			return false;
		}

		RESULT<CModule *> ModuleResult = g_Bouncer->LoadModule(argv[1]);

		if (!IsError(ModuleResult)) {
			SENDUSER("Module was successfully loaded.");
		} else {
			asprintf(&Out, "Module could not be loaded: %s", GETDESCRIPTION(ModuleResult));

			CHECK_ALLOC_RESULT(Out, asprintf) {
				SENDUSER("Module could not be loaded.");

				return false;
			} CHECK_ALLOC_RESULT_END;

			SENDUSER(Out);

			free(Out);
		}

		return false;
	} else if (strcasecmp(Subcommand, "rmmod") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: RMMOD module-id");
			return false;
		}

		unsigned int Index = atoi(argv[1]);

		if (Index == 0 || Index > Modules->GetLength()) {
			SENDUSER("There is no such module.");
		} else {
			CModule *Module = Modules->Get(Index - 1);

			if (g_Bouncer->UnloadModule(Module)) {
				SENDUSER("Done.");
			} else {
				SENDUSER("Failed to unload this module.");
			}
		}

		return false;
	} else if (strcasecmp(Subcommand, "unset") == 0) {
		if (argc < 2) {
			SENDUSER("Syntax: unset option");
		} else {
			if (NoticeUser) {
				asprintf(&Out, "SBNC SET %s :", argv[1]);
			} else {
				asprintf(&Out, "PRIVMSG -sBNC :SET %s :", argv[1]);
			}

			CHECK_ALLOC_RESULT(Out, asprintf) {} else {
				ParseLine(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
		}

		return false;
	} else if (strcasecmp(Subcommand, "set") == 0) {
		CConfig *Config = m_Owner->GetConfig();

		if (argc < 3) {
			SENDUSER("Configurable settings:");
			SENDUSER("--");

			SENDUSER("password - Set");

			asprintf(&Out, "vhost - %s", m_Owner->GetVHost() ? m_Owner->GetVHost() : "Default");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			if (m_Owner->GetServer() != NULL) {
				asprintf(&Out, "server - %s:%d", m_Owner->GetServer(), m_Owner->GetPort());
			} else {
				Out = strdup("server - Not set");
			}
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "serverpass - %s", m_Owner->GetServerPassword() ? "Set" : "Not set");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "realname - %s", m_Owner->GetRealname());
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "awaynick - %s", m_Owner->GetAwayNick() ? m_Owner->GetAwayNick() : "Not set");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "away - %s", m_Owner->GetAwayText() ? m_Owner->GetAwayText() : "Not set");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "awaymessage - %s", m_Owner->GetAwayMessage() ? m_Owner->GetAwayMessage() : "Not set");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "appendtimestamp - %s", Config->ReadInteger("user.ts") ? "On" : "Off");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "usequitasaway - %s", Config->ReadInteger("user.quitaway") ? "On" : "Off");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

#ifdef USESSL
			asprintf(&Out, "ssl - %s", m_Owner->GetSSL() ? "On" : "Off");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
#endif

#ifdef IPV6
			asprintf(&Out, "ipv6 - %s", m_Owner->GetIPv6() ? "On" : "Off");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
#endif

			asprintf(&Out, "timezone - %s%d", (m_Owner->GetGmtOffset() > 0) ? "+" : "", m_Owner->GetGmtOffset());
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			const char *AutoModes = m_Owner->GetAutoModes();
			bool ValidAutoModes = AutoModes && *AutoModes;
			const char *DropModes = m_Owner->GetDropModes();
			bool ValidDropModes = DropModes && *DropModes;

			const char *AutoModesPrefix = "+", *DropModesPrefix = "-";

			if (!ValidAutoModes || (AutoModes && (*AutoModes == '+' || *AutoModes == '-')))
				AutoModesPrefix = "";

			if (!ValidDropModes || (DropModes && (*DropModes == '-' || *DropModes == '+')))
				DropModesPrefix = "";

			asprintf(&Out, "automodes - %s%s", AutoModesPrefix, ValidAutoModes ? AutoModes : "Not set");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "dropmodes - %s%s", DropModesPrefix, ValidDropModes ? DropModes : "Not set");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			if (m_Owner->IsAdmin()) {
				asprintf(&Out, "sysnotices - %s", m_Owner->GetSystemNotices() ? "On" : "Off");
				CHECK_ALLOC_RESULT(Out, asprintf) { } else {
					SENDUSER(Out);
					free(Out);
				} CHECK_ALLOC_RESULT_END;
			}
		} else {
			if (strcasecmp(argv[1], "server") == 0) {
				if (argc > 3) {
					m_Owner->SetPort(atoi(argv[3]));
					m_Owner->SetServer(argv[2]);
				} else if (argc > 2 && strlen(argv[2]) == 0) {
					m_Owner->SetServer(NULL);
					m_Owner->SetPort(6667);
				} else {
					SENDUSER("Syntax: /sbnc set server host port");

					return false;
				}
			} else if (strcasecmp(argv[1], "realname") == 0) {
				ArgRejoinArray(argv, 2);
				m_Owner->SetRealname(argv[2]);
			} else if (strcasecmp(argv[1], "awaynick") == 0) {
				m_Owner->SetAwayNick(argv[2]);
			} else if (strcasecmp(argv[1], "away") == 0) {
				ArgRejoinArray(argv, 2);
				m_Owner->SetAwayText(argv[2]);
			} else if (strcasecmp(argv[1], "awaymessage") == 0) {
				ArgRejoinArray(argv, 2);
				m_Owner->SetAwayMessage(argv[2]);
			} else if (strcasecmp(argv[1], "vhost") == 0) {
				m_Owner->SetVHost(argv[2]);
			} else if (strcasecmp(argv[1], "serverpass") == 0) {
				m_Owner->SetServerPassword(argv[2]);
			} else if (strcasecmp(argv[1], "password") == 0) {
				if (strlen(argv[2]) < 6 || argc > 3) {
					SENDUSER("Your password is too short or contains invalid characters.");
					return false;
				} else {
					m_Owner->SetPassword(argv[2]);
				}
			} else if (strcasecmp(argv[1], "appendtimestamp") == 0) {
				if (strcasecmp(argv[2], "on") == 0)
					Config->WriteInteger("user.ts", 1);
				else if (strcasecmp(argv[2], "off") == 0)
					Config->WriteInteger("user.ts", 0);
				else {
					SENDUSER("Value must be either 'on' or 'off'.");

					return false;
				}
			} else if (strcasecmp(argv[1], "usequitasaway") == 0) {
				if (strcasecmp(argv[2], "on") == 0)
					Config->WriteInteger("user.quitaway", 1);
				else if (strcasecmp(argv[2], "off") == 0)
					Config->WriteInteger("user.quitaway", 0);
				else {
					SENDUSER("Value must be either 'on' or 'off'.");

					return false;
				}
			} else if (strcasecmp(argv[1], "automodes") == 0) {
				ArgRejoinArray(argv, 2);
				m_Owner->SetAutoModes(argv[2]);
			} else if (strcasecmp(argv[1], "dropmodes") == 0) {
				ArgRejoinArray(argv, 2);
				m_Owner->SetDropModes(argv[2]);
			} else if (strcasecmp(argv[1], "ssl") == 0) {
				if (strcasecmp(argv[2], "on") == 0)
					m_Owner->SetSSL(true);
				else if (strcasecmp(argv[2], "off") == 0)
					m_Owner->SetSSL(false);
				else {
					SENDUSER("Value must be either 'on' or 'off'.");

					return false;
				}
#ifdef IPV6
			} else if (strcasecmp(argv[1], "ipv6") == 0) {
				if (strcasecmp(argv[2], "on") == 0) {
					SENDUSER("Please keep in mind that IPv6 will only work if your server actually has IPv6 connectivity.");

					m_Owner->SetIPv6(true);
				} else if (strcasecmp(argv[2], "off") == 0)
					m_Owner->SetIPv6(false);
				else {
					SENDUSER("Value must be either 'on' or 'off'.");

					return false;
				}
#endif
			} else if (strcasecmp(argv[1], "timezone") == 0) {
				m_Owner->SetGmtOffset(atoi(argv[2]));
			} else if (strcasecmp(argv[1], "sysnotices") == 0) {
				if (strcasecmp(argv[2], "on") == 0)
					m_Owner->SetSystemNotices(true);
				else if (strcasecmp(argv[2], "off") == 0)
					m_Owner->SetSystemNotices(false);
				else {
					SENDUSER("Value must be either 'on' or 'off'.");

					return false;
				}
			} else {
				SENDUSER("Unknown setting.");
				return false;
			}

			SENDUSER("Done.");
		}

		return false;
#ifdef USESSL
	} else if (strcasecmp(Subcommand, "savecert") == 0) {
		if (!IsSSL()) {
			SENDUSER("Error: You are not using an SSL-encrypted connection.");
		} else if (GetPeerCertificate() == NULL) {
			SENDUSER("Error: You are not using a client certificate.");
		} else {
			m_Owner->AddClientCertificate(GetPeerCertificate());

			SENDUSER("Your certificate was stored and will be used for public key authentication.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "showcert") == 0) {
		int i = 0;
		char Buffer[300];
		const CVector<X509 *> *Certificates;
		X509_NAME* name;
		bool First = true;

		Certificates = m_Owner->GetClientCertificates();

		for (unsigned int i = 0; i < Certificates->GetLength(); i++) {
			X509 *Certificate = Certificates->Get(i);

			if (First == false) {
				SENDUSER("---");
			} else {
				First = false;
			}

			asprintf(&Out, "Client Certificate #%d", i + 1);
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			name = X509_get_issuer_name(Certificate);
			X509_NAME_oneline(name, Buffer, sizeof(Buffer));

			asprintf(&Out, "issuer: %s", Buffer);
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			name = X509_get_subject_name(Certificate);
			X509_NAME_oneline(name, Buffer, sizeof(Buffer));

			asprintf(&Out, "subject: %s", Buffer);
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
		}

		SENDUSER("End of CERTIFICATES.");

		return false;
	} else if (strcasecmp(Subcommand, "delcert") == 0) {
		unsigned int id;

		if (argc < 2) {
			SENDUSER("Syntax: delcert ID");
			
			return false;
		}

		id = atoi(argv[1]);

		X509 *Certificate;
		const CVector<X509 *> *Certificates = m_Owner->GetClientCertificates();

		if (id == 0 || Certificates->GetLength() > id) {
			Certificate = NULL;
		} else {
			Certificate = Certificates->Get(id - 1);
		}

		if (Certificate != NULL) {
			if (m_Owner->RemoveClientCertificate(Certificate)) {
				SENDUSER("Done.");
			} else {
				SENDUSER("An error occured while removing the certificate.");
			}
		} else {
			SENDUSER("The ID you specified is not valid. Use the SHOWCERT command to get a list of valid IDs.");
		}

		return false;
#endif
	} else if (strcasecmp(Subcommand, "die") == 0 && m_Owner->IsAdmin()) {
		g_Bouncer->Log("Shutdown requested by %s", m_Owner->GetUsername());
		g_Bouncer->Shutdown();

		return false;
	} else if (strcasecmp(Subcommand, "reload") == 0 && m_Owner->IsAdmin()) {
		if (argc >= 2) {
			g_Bouncer->GetLoaderParameters()->SetModulePath(argv[1]);
		}

		g_Bouncer->Log("Reload requested by %s", m_Owner->GetUsername());
		g_Bouncer->InitializeFreeze();

		return false;
	} else if (strcasecmp(Subcommand, "adduser") == 0 && m_Owner->IsAdmin()) {
		if (argc < 3) {
			SENDUSER("Syntax: ADDUSER username password");
			return false;
		}

		if (!g_Bouncer->IsValidUsername(argv[1])) {
			SENDUSER("Could not create user: The username must be alpha-numeric.");
			return false;
		}

		g_Bouncer->CreateUser(argv[1], argv[2]);

		SENDUSER("Done.");

		return false;

	} else if (strcasecmp(Subcommand, "deluser") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: DELUSER username");
			return false;
		}

		if (strcasecmp(argv[1], m_Owner->GetUsername()) == 0) {
			SENDUSER("You cannot remove yourself.");

			return false;
		}

		RESULT<bool> Result = g_Bouncer->RemoveUser(argv[1]);

		if (IsError(Result)) {
			SENDUSER(GETDESCRIPTION(Result));
		} else {
			SENDUSER("Done.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "simul") == 0 && m_Owner->IsAdmin()) {
		if (argc < 3) {
			SENDUSER("Syntax: SIMUL username :command");
			return false;
		}

		CUser* User = g_Bouncer->GetUser(argv[1]);

		if (User) {
			ArgRejoinArray(argv, 2);
			User->Simulate(argv[2], this);

			SENDUSER("Done.");
		} else {
			asprintf(&Out, "No such user: %s", argv[1]);
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
		}

		return false;
	} else if (strcasecmp(Subcommand, "direct") == 0) {
		if (argc < 2) {
			SENDUSER("Syntax: DIRECT :command");
			return false;
		}

		CIRCConnection* IRC = m_Owner->GetIRCConnection();

		ArgRejoinArray(argv, 1);
		IRC->WriteLine("%s", argv[1]);

		return false;
	} else if (strcasecmp(Subcommand, "gvhost") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			const char* Ip = g_Bouncer->GetConfig()->ReadString("system.vhost");

			asprintf(&Out, "Current global VHost: %s", Ip ? Ip : "(none)");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
		} else {
			g_Bouncer->GetConfig()->WriteString("system.vhost", argv[1]);
			SENDUSER("Done.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "motd") == 0) {
		if (argc < 2) {
			const char* Motd = g_Bouncer->GetMotd();

			asprintf(&Out, "Current MOTD: %s", Motd ? Motd : "(none)");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			if (Motd != NULL && m_Owner->IsAdmin()) {
				if (NoticeUser) {
					SENDUSER("Use /sbnc motd remove to remove the motd.");
				} else {
					SENDUSER("Use /msg -sBNC motd remove to remove the motd.");
				}
			}
		} else if (m_Owner->IsAdmin()) {
			ArgRejoinArray(argv, 1);

			if (strcasecmp(argv[1], "remove") == 0) {
				g_Bouncer->SetMotd(NULL);
			} else {
				g_Bouncer->SetMotd(argv[1]);
			}

			SENDUSER("Done.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "global") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: GLOBAL :text");
			return false;
		}

		ArgRejoinArray(argv, 1);
		g_Bouncer->GlobalNotice(argv[1]);
		return false;
	} else if (strcasecmp(Subcommand, "kill") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: KILL username [reason]");
			return false;
		}

		CUser *User = g_Bouncer->GetUser(argv[1]);

		const char *Reason = "No reason specified.";

		if (argc >= 3) {
			ArgRejoinArray(argv, 2);
			Reason = argv[2];
		}

		asprintf(&Out, "You were disconnected from the bouncer. (Requested by %s: %s)", m_Owner->GetUsername(), Reason);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			return false;
		} CHECK_ALLOC_RESULT_END;

		if (User && User->GetClientConnection()) {
			User->GetClientConnection()->Kill(Out);
			SENDUSER("Done.");
		} else {
			SENDUSER("There is no such user or that user is not currently logged in.");
		}

		free(Out);

		return false;
	} else if (strcasecmp(Subcommand, "disconnect") == 0) {
		CUser *User;

		if (m_Owner->IsAdmin() && argc >= 2) {
			User = g_Bouncer->GetUser(argv[1]);
		} else {
			User = m_Owner;
		}

		if (User == NULL) {
			SENDUSER("There is no such user.");
			return false;
		}

		CIRCConnection *IRC = User->GetIRCConnection();

		if (IRC == NULL) {
			if (User == m_Owner) {
				SENDUSER("You are not connected to a server.");
			} else {
				SENDUSER("The user is not connected to a server.");
			}

			return false;
		}

		User->MarkQuitted(true);

		IRC->Kill("Requested.");

		SENDUSER("Done.");

		return false;
	} else if (strcasecmp(Subcommand, "jump") == 0) {
		if (m_Owner->GetIRCConnection()) {
			m_Owner->GetIRCConnection()->Kill("Reconnecting");

			m_Owner->SetIRCConnection(NULL);
		}

		m_Owner->ScheduleReconnect(5);
		return false;
	} else if (strcasecmp(Subcommand, "status") == 0) {
		asprintf(&Out, "Username: %s", m_Owner->GetUsername());
		CHECK_ALLOC_RESULT(Out, asprintf) { } else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

		asprintf(&Out, "This is shroudBNC %s", g_Bouncer->GetBouncerVersion());
		CHECK_ALLOC_RESULT(Out, asprintf) { } else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

		asprintf(&Out, "You are %san admin.", m_Owner->IsAdmin() ? "" : "not ");
		CHECK_ALLOC_RESULT(Out, asprintf) { } else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

		asprintf(&Out, "Client: sendq: %d, recvq: %d", GetSendqSize(), GetRecvqSize());
		CHECK_ALLOC_RESULT(Out, asprintf) { } else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

		CIRCConnection* IRC = m_Owner->GetIRCConnection();

		if (IRC) {
			asprintf(&Out, "IRC: sendq: %d, recvq: %d", IRC->GetSendqSize(), IRC->GetRecvqSize());
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			SENDUSER("Channels:");

			int a = 0;

			while (hash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
				SENDUSER(Chan->Name);
			}

			SENDUSER("End of CHANNELS.");
		}

		asprintf(&Out, "Uptime: %d seconds", m_Owner->GetIRCUptime());
		CHECK_ALLOC_RESULT(Out, asprintf) { } else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

		return false;
	} else if (strcasecmp(Subcommand, "impulse") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: impulse command");

			return false;
		}

		const char *Reply = g_Bouncer->DebugImpulse(atoi(argv[1]));

		if (Reply != NULL) {
			SENDUSER(Reply);
		} else {
			SENDUSER("No return value.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "who") == 0 && m_Owner->IsAdmin()) {
		int i = 0;

		while (hash_t<CUser *> *UserHash = g_Bouncer->GetUsers()->Iterate(i++)) {
			const char* Server, *ClientAddr;
			CUser* User = UserHash->Value;

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

			const char *LastSeen;

			if (User->GetLastSeen() == 0) {
				LastSeen = "Never";
			} else if (User->GetClientConnection() != NULL) {
				LastSeen = "Now";
			} else {
				LastSeen = m_Owner->FormatTime(User->GetLastSeen());
			}

			asprintf(&Out, "%s%s%s%s(%s)@%s [%s] [Last seen: %s] :%s",
				User->IsLocked() ? "!" : "",
				User->IsAdmin() ? "@" : "",
				ClientAddr ? "*" : "",
				User->GetUsername(),
				User->GetIRCConnection() ? (User->GetIRCConnection()->GetCurrentNick() ? User->GetIRCConnection()->GetCurrentNick() : "<none>") : User->GetNick(),
				ClientAddr ? ClientAddr : "",
				Server ? Server : "",
				LastSeen,
				User->GetRealname());
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
		}

		SENDUSER("End of USERS.");

		return false;
	} else if (strcasecmp(Subcommand, "addlistener") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: addlistener <port> [address] [ssl]");

			return false;
		}

		unsigned int Port = atoi(argv[1]);

		if (Port <= 1024 || Port >= 65534) {
			SENDUSER("You did not specify a valid port.");

			return false;
		}

		const char *Address = NULL;

		if (argc > 2) {
			Address = argv[2];
		}

		bool SSL = false;

		if (argc > 3) {
			if (atoi(argv[3]) != 0 || strcasecmp(argv[3], "ssl") == 0) {
				SSL = true;
			}
		}

		RESULT<bool> Result = g_Bouncer->AddAdditionalListener(Port, Address, SSL);

		if (IsError(Result)) {
			SENDUSER(GETDESCRIPTION(Result));

			return false;
		}

		SENDUSER("Done.");

		return false;
	} else if (strcasecmp(Subcommand, "dellistener") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: dellistener <port>");

			return false;
		}

		RESULT<bool> Result = g_Bouncer->RemoveAdditionalListener(atoi(argv[1]));

		if (Result) {
			SENDUSER("Done.");
		} else {
			SENDUSER("There is no such listener.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "listeners") == 0 && m_Owner->IsAdmin()) {
		asprintf(&Out, "Main listener: port %d", g_Bouncer->GetMainListener()->GetPort());
		CHECK_ALLOC_RESULT(&Out, asprintf) {} else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

#ifdef USESSL
		if (g_Bouncer->GetMainSSLListener() != NULL) {
			asprintf(&Out, "Main SSL listener: port %d", g_Bouncer->GetMainSSLListener()->GetPort());
		} else {
			asprintf(&Out, "Main SSL listener: none");
		}
		CHECK_ALLOC_RESULT(&Out, asprintf) {} else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;
#endif

		SENDUSER("---");
		SENDUSER("Additional listeners:");

		CVector<additionallistener_t> *Listeners = g_Bouncer->GetAdditionalListeners();

		for (unsigned int i = 0; i < Listeners->GetLength(); i++) {
			if ((*Listeners)[i].SSL) {
				if ((*Listeners)[i].BindAddress != NULL) {
					asprintf(&Out, "Port: %d (SSL, bound to %s)", (*Listeners)[i].Port, (*Listeners)[i].BindAddress);
				} else {
					asprintf(&Out, "Port: %d (SSL)", (*Listeners)[i].Port);
				}
			} else {
				if ((*Listeners)[i].BindAddress != NULL) {
					asprintf(&Out, "Port: %d (bound to %s)", (*Listeners)[i].Port, (*Listeners)[i].BindAddress);
				} else {
					asprintf(&Out, "Port: %d", (*Listeners)[i].Port);
				}
			}
			CHECK_ALLOC_RESULT(Out, asprintf) {} else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
		}

		SENDUSER("End of LISTENERS.");

		return false;
	} else if (strcasecmp(Subcommand, "read") == 0) {
		m_Owner->GetLog()->PlayToUser(m_Owner, NoticeUser);

		if (!m_Owner->GetLog()->IsEmpty()) {
			if (NoticeUser)
				m_Owner->RealNotice("End of LOG. Use '/sbnc erase' to remove this log.");
			else
				m_Owner->Notice("End of LOG. Use '/msg -sBNC erase' to remove this log.");
		} else {
			SENDUSER("Your personal log is empty.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "erase") == 0) {
		if (m_Owner->GetLog()->IsEmpty()) {
			SENDUSER("Your personal log is empty.");
		} else {
			m_Owner->GetLog()->Clear();
			SENDUSER("Done.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "playmainlog") == 0 && m_Owner->IsAdmin()) {
		g_Bouncer->GetLog()->PlayToUser(m_Owner, NoticeUser);

		if (!g_Bouncer->GetLog()->IsEmpty()) {
			if (NoticeUser)
				m_Owner->RealNotice("End of LOG. Use /sbnc erasemainlog to remove this log.");
			else
				m_Owner->Notice("End of LOG. Use /msg -sBNC erasemainlog to remove this log.");
		} else {
			SENDUSER("The main log is empty.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "erasemainlog") == 0 && m_Owner->IsAdmin()) {
		g_Bouncer->GetLog()->Clear();
		g_Bouncer->Log("User %s erased the main log", m_Owner->GetUsername());
		SENDUSER("Done.");

		return false;
	} else if (strcasecmp(Subcommand, "hosts") == 0 && m_Owner->IsAdmin()) {
		const CVector<char *> *Hosts = g_Bouncer->GetHostAllows();

		SENDUSER("Hostmasks");
		SENDUSER("---------");

		for (unsigned int i = 0; i < Hosts->GetLength(); i++) {
			SENDUSER(Hosts->Get(i));
		}

		if (Hosts->GetLength() == 0)
			SENDUSER("*");

		SENDUSER("End of HOSTS.");

		return false;
	} else if (strcasecmp(Subcommand, "hostadd") == 0 && m_Owner->IsAdmin()) {
		RESULT<bool> Result;

		if (argc <= 1) {
			SENDUSER("Syntax: HOSTADD hostmask");

			return false;
		}

		Result = g_Bouncer->AddHostAllow(argv[1]);

		if (IsError(Result)) {
			SENDUSER(GETDESCRIPTION(Result));
		} else {
			SENDUSER("Done.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "hostdel") == 0 && m_Owner->IsAdmin()) {
		RESULT<bool> Result;

		if (argc <= 1) {
			SENDUSER("Syntax: HOSTDEL hostmask");

			return false;
		}

		Result = g_Bouncer->RemoveHostAllow(argv[1]);

		if (IsError(Result)) {
			SENDUSER(GETDESCRIPTION(Result));
		} else {
			SENDUSER("Done.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "admin") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: ADMIN username");

			return false;
		}

		CUser* User = g_Bouncer->GetUser(argv[1]);

		if (User) {
			User->SetAdmin(true);

			SENDUSER("Done.");
		} else {
			SENDUSER("There's no such user.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "unadmin") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: UNADMIN username");

			return false;
		}

		CUser* User = g_Bouncer->GetUser(argv[1]);
		
		if (User) {
			User->SetAdmin(false);

			SENDUSER("Done.");
		} else {
			SENDUSER("There's no such user.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "suspend") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: SUSPEND username :reason");

			return false;
		}

		CUser* User = g_Bouncer->GetUser(argv[1]);

		if (User) {
			User->Lock();

			if (User->GetClientConnection())
				User->GetClientConnection()->Kill("Your account has been suspended.");

			if (User->GetIRCConnection())
				User->GetIRCConnection()->Kill("Requested.");

			User->MarkQuitted(true);

			if (argc > 2) {
				ArgRejoinArray(argv, 2);
				User->SetSuspendReason(argv[2]);
			} else {
				User->SetSuspendReason("Suspended.");
			}

			g_Bouncer->Log("User %s has been suspended.", User->GetUsername());

			SENDUSER("Done.");
		} else {
			SENDUSER("There's no such user.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "unsuspend") == 0 && m_Owner->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: UNSUSPEND username");

			return false;
		}

		CUser* User = g_Bouncer->GetUser(argv[1]);
		
		if (User) {
			User->Unlock();

			g_Bouncer->Log("User %s has been unsuspended.", User->GetUsername());

			User->SetSuspendReason(NULL);

			SENDUSER("Done.");
		} else {
			SENDUSER("There's no such user.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "resetpass") == 0 && m_Owner->IsAdmin()) {
		if (argc < 3) {
			SENDUSER("Syntax: RESETPASS username new-password");

			return false;
		}

		CUser* User = g_Bouncer->GetUser(argv[1]);
		
		if (User) {
			User->SetPassword(argv[2]);

			SENDUSER("Done.");
		} else {
			SENDUSER("There's no such user.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "partall") == 0) {
		if (m_Owner->GetIRCConnection()) {
			const char* Channels = m_Owner->GetConfigChannels();

			if (Channels)
				m_Owner->GetIRCConnection()->WriteLine("PART %s", Channels);
		}

		m_Owner->SetConfigChannels(NULL);

		SENDUSER("Done.");

		return false;
	}

	if (NoticeUser) {
		m_Owner->RealNotice("Unknown command. Try /sbnc help");
	} else {
		SENDUSER("Unknown command. Try /msg -sBNC help");
	}

	return false;
}

bool CClientConnection::ParseLineArgV(int argc, const char** argv) {
	char* Out;

	if (argc == 0)
		return false;

	const CVector<CModule *> *Modules = g_Bouncer->GetModules();

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
		if (!Modules->Get(i)->InterceptClientMessage(this, argc, argv))
			return false;
	}

	const char* Command = argv[0];

	if (!m_Owner) {
		if (strcasecmp(Command, "nick") == 0 && argc > 1) {
			const char* Nick = argv[1];

			if (m_Nick != NULL) {
				if (strcmp(m_Nick, Nick) != 0) {
					if (m_Owner)
						m_Owner->GetConfig()->WriteString("user.nick", Nick);

					WriteLine(":%s!ident@sbnc NICK :%s", m_Nick, Nick);
				}
			}

			free(m_Nick);
			m_Nick = strdup(Nick);

			if (m_Username && m_Password)
				ValidateUser();
			else if (m_Username)
				WriteUnformattedLine(":Notice!notice@shroudbnc.info NOTICE * :*** This server requires a password. Use /QUOTE PASS thepassword to supply a password now.");
		} else if (strcasecmp(Command, "pass") == 0) {
			if (argc < 2) {
				WriteLine(":bouncer 461 %s :Not enough parameters", m_Nick);
			} else {
				m_Password = strdup(argv[1]);
			}

			if (m_Nick && m_Username && m_Password)
				ValidateUser();

			return false;
		} else if (strcasecmp(Command, "user") == 0 && argc > 1) {
			if (m_Username) {
				WriteLine(":bouncer 462 %s :You may not reregister", m_Nick);
			} else {
				if (!argv[1]) {
					WriteLine(":bouncer 461 %s :Not enough parameters", m_Nick);
				} else {
					const char* Username = argv[1];

					m_Username = strdup(Username);
				}
			}

			bool ValidSSLCert = false;

			if (m_Nick && m_Username && (m_Password || GetPeerCertificate() != NULL))
				ValidSSLCert = ValidateUser();

			if (m_Nick && m_Username && !m_Password && !ValidSSLCert)
				WriteUnformattedLine(":Notice!notice@shroudbnc.info NOTICE * :*** This server requires a password. Use /QUOTE PASS thepassword to supply a password now.");

			return false;
		} else if (strcasecmp(Command, "quit") == 0) {
			Kill("*** Thanks for flying with shroudBNC :P");
			return false;
		}
	}

	if (m_Owner) {
		if (strcasecmp(Command, "quit") == 0) {
			bool QuitAsAway = (m_Owner->GetConfig()->ReadInteger("user.quitaway") != 0);

			if (QuitAsAway && argc > 1 && *argv[1])
				m_Owner->SetAwayText(argv[1]);

			Kill("*** Thanks for flying with shroudBNC :P");
			return false;
		} else if (strcasecmp(Command, "nick") == 0) {
			if (argc >= 2) {
				free(m_Nick);
				m_Nick = strdup(argv[1]);
				m_Owner->GetConfig()->WriteString("user.nick", argv[1]);
			}
		} else if (argc > 1 && strcasecmp(Command, "join") == 0) {
			CIRCConnection* IRC;
			const char* Key;

			if (argc > 2 && strstr(argv[0], ",") == NULL && strstr(argv[1], ",") == NULL)
				m_Owner->GetKeyring()->SetKey(argv[1], argv[2]);
			else if (m_Owner->GetKeyring() && (Key = m_Owner->GetKeyring()->GetKey(argv[1])) && (IRC = m_Owner->GetIRCConnection())) {
				IRC->WriteLine("JOIN %s %s", argv[1], Key);

				return false;
			}
		} else if (strcasecmp(Command, "whois") == 0) {
			if (argc >= 2) {
				const char* Nick = argv[1];

				if (strcasecmp("-sbnc", Nick) == 0) {
					WriteLine(":bouncer 311 %s -sBNC core shroudbnc.info * :shroudBNC", m_Nick);
					WriteLine(":bouncer 312 %s -sBNC shroudbnc.info :shroudBNC IRC Proxy", m_Nick);
					WriteLine(":bouncer 318 %s -sBNC :End of /WHOIS list.", m_Nick);

					return false;
				}
			}
		} else if (argc > 2 && strcasecmp(Command, "privmsg") == 0 && strcasecmp(argv[1], "-sbnc") == 0) {
			tokendata_t Tokens;
			
			Tokens = ArgTokenize2(argv[2]);

			const char **Arr = ArgToArray2(Tokens);

			ProcessBncCommand(Arr[0], ArgCount2(Tokens), Arr, false);

			ArgFreeArray(Arr);

			return false;
		} else if (strcasecmp(Command, "userhost") == 0) {
			if (argc == 2 && strcasecmp(argv[1], m_Nick) == 0) {
				const char *Server, *Ident;
				CIRCConnection *IRC;

				IRC = m_Owner->GetIRCConnection();

				if (IRC != NULL) {
					Server = IRC->GetServer();
				} else {
					Server = "bouncer";
				}

				Ident = m_Owner->GetIdent();

				if (Ident == NULL) {
					Ident = m_Owner->GetUsername();
				}

				WriteLine(":%s 302 %s :%s=+%s@%s", Server, m_Nick, m_Nick, Ident, m_PeerName);

				return false;
			}
		} else if (strcasecmp(Command, "sbnc") == 0) {
			return ProcessBncCommand(argv[1], argc - 1, &argv[1], true);
		} else if (strcasecmp(Command, "synth") == 0) {
			if (argc < 2) {
				m_Owner->Notice("Syntax: SYNTH command parameter");
				m_Owner->Notice("supported commands are: mode, topic, names, version, who");

				return false;
			}

			if (strcasecmp(argv[1], "mode") == 0 && argc > 2) {
				CIRCConnection* IRC = m_Owner->GetIRCConnection();

				if (IRC) {
					CChannel* Chan = IRC->GetChannel(argv[2]);

					if (argc == 3) {
						if (Chan && Chan->AreModesValid()) {
							WriteLine(":%s 324 %s %s %s", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], (const char *)Chan->GetChannelModes());
							WriteLine(":%s 329 %s %s %d", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Chan->GetCreationTime());
						} else
							IRC->WriteLine("MODE %s", argv[2]);
					} else if (argc == 4 && strcmp(argv[3],"+b") == 0) {
						if (Chan && Chan->HasBans()) {
							CBanlist* Bans = Chan->GetBanlist();

							int i = 0; 

							while (const hash_t<ban_t *> *BanHash = Bans->Iterate(i++)) {
								ban_t *Ban = BanHash->Value;

								WriteLine(":%s 367 %s %s %s %s %d", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Ban->Mask, Ban->Nick, Ban->Timestamp);
							}

							WriteLine(":%s 368 %s %s :End of Channel Ban List", IRC->GetServer(), IRC->GetCurrentNick(), argv[2]);
						} else
							IRC->WriteLine("MODE %s +b", argv[2]);
					}
				}
			} else if (strcasecmp(argv[1], "topic") == 0 && argc > 2) {
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
			} else if (strcasecmp(argv[1], "names") == 0 && argc > 2) {
				CIRCConnection* IRC = m_Owner->GetIRCConnection();

				if (IRC) {
					CChannel* Chan = IRC->GetChannel(argv[2]);

					if (Chan && g_CurrentTime - m_Owner->GetLastSeen() < 300 && Chan->HasNames() != 0) {
						char* Nicks = (char*)malloc(1);
						Nicks[0] = '\0';

						const CHashtable<CNick*, false, 64>* H = Chan->GetNames();

						int a = 0;

						while (hash_t<CNick*>* NickHash = H->Iterate(a++)) {
							CNick* NickObj = NickHash->Value;

							const char* Prefix = NickObj->GetPrefixes();
							const char* Nick = NickObj->GetNick();

							char outPref[2] = { IRC->GetHighestUserFlag(Prefix), '\0' };

							if (Nick == NULL)
								continue;

							Nicks = (char*)realloc(Nicks, (Nicks ? strlen(Nicks) : 0) + strlen(outPref) + strlen(Nick) + 2);

							if (Nicks == NULL) {
								Kill("CClientConnection::ParseLineArgV: realloc() failed. Please reconnect.");

								return false;
							}

							if (*Nicks)
								strcat(Nicks, " ");

							strcat(Nicks, outPref);
							strcat(Nicks, Nick);

							if (strlen(Nicks) > 400) {
								WriteLine(":%s 353 %s = %s :%s", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Nicks);

								Nicks = (char*)realloc(Nicks, 1);

								if (Nicks == NULL) {
									Kill("CClientConnection::ParseLineArgV: realloc() failed. Please reconnect.");

									return false;
								}

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
			} else if (strcasecmp(argv[1], "who") == 0) {
				CIRCConnection* IRC = m_Owner->GetIRCConnection();

				if (IRC) {
					CChannel *Channel = IRC->GetChannel(argv[2]);

					if (Channel && g_CurrentTime - m_Owner->GetLastSeen() < 300 && Channel->SendWhoReply(true)) {
						Channel->SendWhoReply(false);
					} else {
						IRC->WriteLine("WHO %s", argv[2]);
					}
				}
			} else if (strcasecmp(argv[1], "version") == 0 && argc == 2) {
				CIRCConnection* IRC = m_Owner->GetIRCConnection();

				if (IRC) {
					const char* ServerVersion = IRC->GetServerVersion();
					const char* ServerFeat = IRC->GetServerFeat();

					if (ServerVersion != NULL && ServerFeat != NULL) {
						WriteLine(":%s 351 %s %s %s :%s", IRC->GetServer(), IRC->GetCurrentNick(), ServerVersion, IRC->GetServer(), ServerFeat);
					}

					char* Feats = (char*)malloc(1);
					Feats[0] = '\0';

					int a = 0, i = 0;

					while (hash_t<char*>* Feat = IRC->GetISupportAll()->Iterate(i++)) {
						char* Name = Feat->Name;
						char* Value = Feat->Value;

						Feats = (char*)realloc(Feats, (Feats ? strlen(Feats) : 0) + strlen(Name) + 1 + strlen(Value) + 2);

						if (Feats == NULL) {
							Kill("CClientConnection::ParseLineArgV: realloc() failed. Please reconnect.");

							return false;
						}


						if (*Feats)
							strcat(Feats, " ");

						strcat(Feats, Name);

						if (Value && *Value) {
							strcat(Feats, "=");
							strcat(Feats, Value);
						}

						if (++a == 11) {
							WriteLine(":%s 005 %s %s :are supported by this server", IRC->GetServer(), IRC->GetCurrentNick(), Feats);

							Feats = (char*)realloc(Feats, 1);

							if (Feats == NULL) {
								Kill("CClientConnection::ParseLineArgV: realloc() failed. Please reconnect.");

								return false;
					
							}

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
		} else if (strcasecmp(Command, "mode") == 0 || strcasecmp(Command, "topic") == 0 ||
				strcasecmp(Command, "names") == 0 || strcasecmp(Command, "who") == 0) {
			if (argc == 2 || (strcasecmp(Command, "mode") == 0 && argc == 3) && strcmp(argv[2],"+b") == 0) {
				if (argc == 2)
					asprintf(&Out, "SYNTH %s %s", argv[0], argv[1]);
				else
					asprintf(&Out, "SYNTH %s %s %s", argv[0], argv[1], argv[2]);

			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				ParseLine(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

				return false;
			}
		} else if (strcasecmp(Command, "version") == 0) {
			ParseLine("SYNTH VERSION");

			return false;
		}
	}

	return true;
}

void CClientConnection::ParseLine(const char* Line) {
	if (strlen(Line) > 512)
		return; // protocol violation

	bool ReturnValue;
	tokendata_t Args;
	const char** argv, ** real_argv;
	int argc;

	Args = ArgTokenize2(Line);
	argv = real_argv = ArgToArray2(Args);

	CHECK_ALLOC_RESULT(argv, ArgToArray2)  {
		return;
	} CHECK_ALLOC_RESULT_END;

	argc = ArgCount2(Args);

	if (argc > 0) {
		if (argv[0][0] == ':') {
			argv = &argv[1];
			argc--;
		}
	}

	if (argc > 0) {
		ReturnValue = ParseLineArgV(argc, argv);
	} else {
		ReturnValue = true;
	}

	ArgFreeArray(real_argv);

	if (m_Owner != NULL && ReturnValue) {
		CIRCConnection* IRC = m_Owner->GetIRCConnection();

		if (IRC)
			IRC->WriteLine("%s", Line);
	}
}

bool CClientConnection::ValidateUser(void) {
	bool Force = false;
	CUser* User;
	bool Blocked = true, Valid = false;
	sockaddr *Remote;

	Remote = GetRemoteAddress();

	if (Remote == NULL) {
		return false;
	}

#ifdef USESSL
	int Count = 0;
	bool MatchUsername = false;
	X509* PeerCert = NULL;
	CUser* AuthUser = NULL;

	if (IsSSL() && (PeerCert = (X509*)GetPeerCertificate()) != NULL) {
		int i = 0;

		while (hash_t<CUser *> *UserHash = g_Bouncer->GetUsers()->Iterate(i++)) {
			if (UserHash->Value->FindClientCertificate(PeerCert)) {
				AuthUser = UserHash->Value;
				Count++;

				if (strcasecmp(UserHash->Name, m_Username) == 0) {
					MatchUsername = true;
				}
			}
		}

		X509_free(PeerCert);

		if (AuthUser && Count == 1) { // found a single user who has this public certificate
			free(m_Username);
			m_Username = strdup(AuthUser->GetUsername());
			Force = true;
		} else if (MatchUsername == true && Count > 1) // found more than one user with that certificate
			Force = true;
	}

	if (AuthUser == NULL && m_Password == NULL) {
		g_Bouncer->Log("No valid client certificates found for login from %s[%s]", m_PeerName, IpToString(Remote));

		return false;
	}
#endif

	User = g_Bouncer->GetUser(m_Username);

	if (User) {
		Blocked = User->IsIpBlocked(Remote);
		Valid = (Force || User->CheckPassword(m_Password));
	}

	if ((m_Password || Force) && User && !Blocked && Valid) {
		User->Attach(this);
		//WriteLine(":Notice!notice@shroudbnc.info NOTICE * :Welcome to the wonderful world of IRC");
	} else {
		if (User != NULL) {
			if (!Blocked) {
				User->LogBadLogin(Remote);
			}

			if (Blocked) {
				g_Bouncer->Log("Blocked login attempt from %s[%s] for %s", m_PeerName, IpToString(Remote), m_Username);
			} else {
				g_Bouncer->Log("Wrong password for user %s (from %s[%s])", m_Username, m_PeerName, IpToString(Remote));
			}
		} else {
			g_Bouncer->Log("Login attempt for unknown user %s from %s[%s]", m_Username, m_PeerName, IpToString(Remote));
		}

		Kill("*** Unknown user or wrong password.");

		return false;
	}

	delete m_AuthTimer;
	m_AuthTimer = NULL;

	return true;
}

const char* CClientConnection::GetNick(void) const {
	return m_Nick;
}

void CClientConnection::Destroy(void) {
	if (m_Owner) {
		g_Bouncer->Log("%s disconnected.", m_Owner->GetUsername());
		m_Owner->SetClientConnection(NULL);
	}

	delete this;
}

void CClientConnection::SetPeerName(const char* PeerName, bool LookupFailure) {
	sockaddr *Remote;

	if (m_PeerName != NULL) {
		free(m_PeerName);
	}

	m_PeerName = strdup(PeerName);

	Remote = GetRemoteAddress();

	if (!g_Bouncer->CanHostConnect(m_PeerName) && (Remote == NULL || !g_Bouncer->CanHostConnect(IpToString(Remote)))) {
		g_Bouncer->Log("Attempted login from %s[%s]: Host does not match any host allows.", m_PeerName, (Remote != NULL) ? IpToString(Remote) : "unknown ip");

		FlushSendQ();

		Kill("Your host is not allowed to use this bouncer.");

		return;
	}

	ProcessBuffer();
}

const char* CClientConnection::GetPeerName(void) const {
	return m_PeerName;
}

void CClientConnection::AsyncDnsFinishedClient(hostent* Response) {
	int i = 0;
	sockaddr *Remote;

	Remote = GetRemoteAddress();

	if (Response == NULL) {
		WriteLine(":Notice!notice@shroudbnc.info NOTICE * :*** Reverse DNS query failed. Using IP address as your hostname.");

		if (Remote != NULL) {
			SetPeerName(IpToString(Remote), true);
		} else {
			Kill("Failed to look up IP address.");
		}
	} else {
		if (m_PeerNameTemp == NULL) {
			m_PeerNameTemp = strdup(Response->h_name);

			WriteLine(":Notice!notice@shroudbnc.info NOTICE * :*** Reverse DNS reply received (%s).", Response->h_name);
			WriteLine(":Notice!notice@shroudbnc.info NOTICE * :*** Doing forward DNS lookup...");
			m_ClientLookup->GetHostByName(Response->h_name, Response->h_addrtype);
		} else {
			sockaddr *saddr = NULL;

			while (Response && Response->h_addr_list[i] != NULL) {
				sockaddr_in sin;
#ifdef IPV6
				sockaddr_in6 sin6;
#endif

				if (Response->h_addrtype == AF_INET) {
					sin.sin_family = AF_INET;
					sin.sin_addr.s_addr = (*(in_addr *)Response->h_addr_list[i]).s_addr;
					sin.sin_port = 0;
					saddr = (sockaddr *)&sin;
#ifdef IPV6
				} else {
					sin6.sin6_family = AF_INET6;
					memcpy(&sin6.sin6_addr.s6_addr, &(Response->h_addr_list[i]), sizeof(in6_addr));
					sin6.sin6_port = 0;
					saddr = (sockaddr *)&sin6;
#endif
				}

				if (CompareAddress(saddr, Remote) == 0) {
					SetPeerName(m_PeerNameTemp, false);
					free(m_PeerNameTemp);

					WriteLine(":Notice!notice@shroudbnc.info NOTICE * :*** Forward DNS reply received. (%s)", m_PeerName);

					// TODO: destroy query

					return;
				}

				i++;
			}

			if (saddr != NULL) {
				WriteLine(":Notice!notice@shroudbnc.info NOTICE * :*** Forward DNS reply received. (%s)", IpToString(saddr));
			} else {
				WriteLine(":Notice!notice@shroudbnc.info NOTICE * :*** Forward DNS reply received.");
			}

			WriteLine(":Notice!notice@shroudbnc.info NOTICE * :*** Forward and reverse DNS replies do not match. Using IP address instead.");

			if (Remote != NULL) {
				SetPeerName(IpToString(Remote), true);
			} else {
				Kill("Failed to look up IP address.");
			}
		}
	}
}

const char *CClientConnection::GetClassName(void) const {
	return "CClientConnection";
}

bool CClientConnection::Read(bool DontProcess) {
	bool Ret;

	if (m_PeerName != NULL) {
		Ret = CConnection::Read(false);
	} else {
		return CConnection::Read(true);
	}

	if (Ret && GetRecvqSize() > 5120) {
		Kill("RecvQ exceeded.");
	}

	return Ret;
}

void CClientConnection::WriteUnformattedLine(const char *Line) {
	CConnection::WriteUnformattedLine(Line);

	if (m_Owner && !m_Owner->IsAdmin() && GetSendqSize() > g_Bouncer->GetSendqSize() * 1024) {
		FlushSendQ();
		CConnection::WriteUnformattedLine("");
		Kill("SendQ exceeded.");
	}
}

RESULT<bool> CClientConnection::Freeze(CAssocArray *Box) {
	// too bad we can't preserve ssl encrypted connections
	if (m_PeerName == NULL || GetSocket() == INVALID_SOCKET || IsSSL() || m_Nick == NULL) {
		RETURN(bool, false);
	}

	Box->AddString("~client.peername", m_PeerName);
	Box->AddString("~client.nick", m_Nick);
	Box->AddInteger("~client.fd", GetSocket());

	// protect the socket from being closed
	g_Bouncer->UnregisterSocket(GetSocket());
	SetSocket(INVALID_SOCKET);

	Destroy();

	RETURN(bool, true);
}

RESULT<CClientConnection *> CClientConnection::Thaw(CAssocArray *Box) {
	SOCKET Socket;
	CClientConnection *Client;
	const char *Temp;

	if (Box == NULL) {
		THROW(CClientConnection *, Generic_InvalidArgument, "Box cannot be NULL.");
	}

	Socket = Box->ReadInteger("~client.fd");

	if (Socket == INVALID_SOCKET) {
		RETURN(CClientConnection *, NULL);
	}

	Client = new CClientConnection();

	Client->SetSocket(Socket);

	Temp = Box->ReadString("~client.peername");

	if (Temp != NULL) {
		Client->m_PeerName = strdup(Temp);
	} else {
		Client->m_PeerName = strdup(IpToString(Client->GetRemoteAddress()));
	}

	Temp = Box->ReadString("~client.nick");

	if (Temp != NULL) {
		Client->m_Nick = strdup(Temp);
	} else {
		delete Client;

		THROW(CClientConnection *, Generic_Unknown, "Persistent data is invalid: Missing nickname.");
	}

	RETURN(CClientConnection *, Client);
}

void CClientConnection::Kill(const char *Error) {
	if (m_Owner) {
		m_Owner->SetClientConnection(NULL);
		m_Owner = NULL;
	}

	WriteLine(":Notice!notice@shroudbnc.info NOTICE * :%s", Error);

	CConnection::Kill(Error);
}

commandlist_t *CClientConnection::GetCommandList(void) {
	return &m_CommandList;
}


void CClientConnection::SetPreviousNick(const char *Nick) {
	free(m_PreviousNick);
	m_PreviousNick = strdup(Nick);
}

const char *CClientConnection::GetPreviousNick(void) const {
	return m_PreviousNick;
}

SOCKET CClientConnection::Hijack(void) {
	SOCKET Socket;

	Socket = GetSocket();
	g_Bouncer->UnregisterSocket(Socket);
	SetSocket(INVALID_SOCKET);

	Kill("Hijacked!");

	return Socket;
}

bool ClientAuthTimer(time_t Now, void *Client) {
	CClientConnection *ClientConnection = (CClientConnection *)Client;

	ClientConnection->Kill("*** Connection timed out. Please reconnect and log in.");
	ClientConnection->m_AuthTimer = NULL;

	return false;
}
