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

/**
 * CClientConnection
 *
 * Constructs a new client connection object.
 *
 * @param Client the client socket
 * @param SSL whether to use SSL
 */
CClientConnection::CClientConnection(SOCKET Client, bool SSL) : CConnection(Client, SSL, Role_Server) {
	m_Nick = NULL;
	m_Password = NULL;
	m_Username = NULL;
	m_PreviousNick = NULL;
	m_PeerName = NULL;
	m_PeerNameTemp = NULL;
	m_ClientLookup = NULL;
	m_CommandList = NULL;
	m_NamesXSupport = false;

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

/**
 * CClientConnection
 *
 * Constructs a new client connection object. This constructor should
 * only be used by ThawObject().
 */
CClientConnection::CClientConnection(void) : CConnection(INVALID_SOCKET, false, Role_Server) {
	m_Nick = NULL;
	m_Password = NULL;
	m_Username = NULL;
	m_PeerName = NULL;
	m_PreviousNick = NULL;
	m_ClientLookup = NULL;
	m_AuthTimer = NULL;
	m_CommandList = NULL;
	m_NamesXSupport = false;
}

/**
 * ~CClientConnection
 *
 * Destructs a client connection object.
 */
CClientConnection::~CClientConnection() {
	ufree(m_Nick);
	ufree(m_Password);
	ufree(m_Username);
	ufree(m_PeerName);
	ufree(m_PreviousNick);

	delete m_ClientLookup;
	delete m_AuthTimer;
}

/**
 * ProcessBncCommand
 *
 * Processes a bouncer command (i.e. /sbnc <command> or /msg -sBNC <command>).
 *
 * @param Subcommand the command's name
 * @param argc number of parameters for the command
 * @param argv arguments for the command
 * @param NoticeUser whether to send replies as notices
 */
bool CClientConnection::ProcessBncCommand(const char *Subcommand, int argc, const char **argv, bool NoticeUser) {
	char *Out;
	CUser *targUser = GetOwner();
	const CVector<CModule *> *Modules;
	bool latchedRetVal = true;

	Modules = g_Bouncer->GetModules();

/**
 * SENDUSER
 *
 * Sends the specified text to the user.
 *
 * @param Text the text
 */
#define SENDUSER(Text) \
	do { \
		if (NoticeUser) { \
			targUser->RealNotice(Text); \
		} else { \
			targUser->Privmsg(Text); \
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

		if (GetOwner()->IsAdmin()) {
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
#ifndef _WIN32
				"Syntax: reload [filename]\nReloads shroudBNC (a new .so module file can be specified).");
#else
				"Syntax: reload [filename]\nReloads shroudBNC (a new .dll module file can be specified).");
#endif
			AddCommand(&m_CommandList, "die", "Admin", "terminates the bouncer",
				"Syntax: die\nTerminates the bouncer.");
			AddCommand(&m_CommandList, "hosts", "Admin", "lists all hostmasks, which are permitted to use this bouncer",
				"Syntax: hosts\nLists all hosts which are permitted to use this bouncer.");
			AddCommand(&m_CommandList, "hostadd", "Admin", "adds a hostmask",
				"Syntax: hostadd <host>\nAdds a host to the bouncer's hostlist. E.g. *.tiscali.de");
			AddCommand(&m_CommandList, "hostdel", "Admin", "removes a hostmask",
				"Syntax: hostdel <host>\nRemoves a host from the bouncer's hostlist.");
			AddCommand(&m_CommandList, "addlistener", "Admin", "creates an additional listener",
#ifdef USESSL
				"Syntax: addlistener <port> [address] [ssl]\nCreates an additional listener which can be used by clients.");
#else
				"Syntax: addlistener <port> [address]\nCreates an additional listener which can be used by clients.");
#endif

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
		if (!GetOwner()->IsAdmin()) {
			AddCommand(&m_CommandList, "disconnect", "User", "disconnects a user from the irc server",
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

		if (GetOwner()->IsAdmin()) {
			AddCommand(&m_CommandList, "status", "User", "tells you the current status",
				"Syntax: status\nDisplays information about your user. This command is used for debugging.");
		}

		AddCommand(&m_CommandList, "help", "User", "displays a list of commands or information about individual commands",
			"Syntax: help [command]\nDisplays a list of commands or information about individual commands.");
	}

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
		if ((*Modules)[i]->InterceptClientCommand(this, Subcommand, argc, argv, NoticeUser)) {
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
					if (Category) {
						SENDUSER("--");
					}

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

					if (NextLine == NULL) {
						break;
					} else {
						Help = NextLine;
					}
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

	if (strcasecmp(Subcommand, "lsmod") == 0 && GetOwner()->IsAdmin()) {
		for (unsigned int i = 0; i < Modules->GetLength(); i++) {
			asprintf(&Out, "%d: %x %s", i + 1, (*Modules)[i]->GetHandle(), (*Modules)[i]->GetFilename());

			CHECK_ALLOC_RESULT(Out, asprintf) {
				return false;
			} CHECK_ALLOC_RESULT_END;

			SENDUSER(Out);
			free(Out);
		}

		SENDUSER("End of MODULES.");

		return false;
	} else if (strcasecmp(Subcommand, "insmod") == 0 && GetOwner()->IsAdmin()) {
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
	} else if (strcasecmp(Subcommand, "rmmod") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: RMMOD module-id");
			return false;
		}

		unsigned int Index = atoi(argv[1]);

		if (Index == 0 || Index > Modules->GetLength()) {
			SENDUSER("There is no such module.");
		} else {
			CModule *Module = (*Modules)[Index - 1];

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
		CConfig *Config = GetOwner()->GetConfig();

		if (argc < 3) {
			SENDUSER("Configurable settings:");
			SENDUSER("--");

			SENDUSER("password - Set");

			asprintf(&Out, "vhost - %s", GetOwner()->GetVHost() ? GetOwner()->GetVHost() : "Default");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			if (GetOwner()->GetServer() != NULL) {
				asprintf(&Out, "server - %s:%d", GetOwner()->GetServer(), GetOwner()->GetPort());
			} else {
				Out = strdup("server - Not set");
			}
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "serverpass - %s", GetOwner()->GetServerPassword() ? "Set" : "Not set");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "realname - %s", GetOwner()->GetRealname());
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "awaynick - %s", GetOwner()->GetAwayNick() ? GetOwner()->GetAwayNick() : "Not set");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "away - %s", GetOwner()->GetAwayText() ? GetOwner()->GetAwayText() : "Not set");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			asprintf(&Out, "awaymessage - %s", GetOwner()->GetAwayMessage() ? GetOwner()->GetAwayMessage() : "Not set");
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
			asprintf(&Out, "ssl - %s", GetOwner()->GetSSL() ? "On" : "Off");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
#endif

#ifdef IPV6
			asprintf(&Out, "ipv6 - %s", GetOwner()->GetIPv6() ? "On" : "Off");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
#endif

			asprintf(&Out, "timezone - %s%d", (GetOwner()->GetGmtOffset() > 0) ? "+" : "", GetOwner()->GetGmtOffset());
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			const char *AutoModes = GetOwner()->GetAutoModes();
			bool ValidAutoModes = AutoModes && *AutoModes;
			const char *DropModes = GetOwner()->GetDropModes();
			bool ValidDropModes = DropModes && *DropModes;

			const char *AutoModesPrefix = "+", *DropModesPrefix = "-";

			if (!ValidAutoModes || (AutoModes && (*AutoModes == '+' || *AutoModes == '-'))) {
				AutoModesPrefix = "";
			}

			if (!ValidDropModes || (DropModes && (*DropModes == '-' || *DropModes == '+'))) {
				DropModesPrefix = "";
			}

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

			if (GetOwner()->IsAdmin()) {
				asprintf(&Out, "sysnotices - %s", GetOwner()->GetSystemNotices() ? "On" : "Off");
				CHECK_ALLOC_RESULT(Out, asprintf) { } else {
					SENDUSER(Out);
					free(Out);
				} CHECK_ALLOC_RESULT_END;
			}
		} else {
			if (strcasecmp(argv[1], "server") == 0) {
				if (argc > 3) {
					GetOwner()->SetPort(atoi(argv[3]));
					GetOwner()->SetServer(argv[2]);
				} else if (argc > 2) {
					if (strlen(argv[2]) == 0) {
						GetOwner()->SetServer(NULL);
						GetOwner()->SetPort(6667);
					} else {
						GetOwner()->SetPort(6667);
						GetOwner()->SetServer(argv[2]);
					}
				} else {
					SENDUSER("Syntax: /sbnc set server host port");

					return false;
				}
			} else if (strcasecmp(argv[1], "realname") == 0) {
				ArgRejoinArray(argv, 2);
				GetOwner()->SetRealname(argv[2]);
			} else if (strcasecmp(argv[1], "awaynick") == 0) {
				GetOwner()->SetAwayNick(argv[2]);
			} else if (strcasecmp(argv[1], "away") == 0) {
				ArgRejoinArray(argv, 2);
				GetOwner()->SetAwayText(argv[2]);
			} else if (strcasecmp(argv[1], "awaymessage") == 0) {
				ArgRejoinArray(argv, 2);
				GetOwner()->SetAwayMessage(argv[2]);
			} else if (strcasecmp(argv[1], "vhost") == 0) {
				GetOwner()->SetVHost(argv[2]);
			} else if (strcasecmp(argv[1], "serverpass") == 0) {
				GetOwner()->SetServerPassword(argv[2]);
			} else if (strcasecmp(argv[1], "password") == 0) {
				if (strlen(argv[2]) < 6 || argc > 3) {
					SENDUSER("Your password is too short or contains invalid characters.");
					return false;
				} else {
					GetOwner()->SetPassword(argv[2]);
				}
			} else if (strcasecmp(argv[1], "appendtimestamp") == 0) {
				if (strcasecmp(argv[2], "on") == 0) {
					Config->WriteInteger("user.ts", 1);
				} else if (strcasecmp(argv[2], "off") == 0) {
					Config->WriteInteger("user.ts", 0);
				} else {
					SENDUSER("Value must be either 'on' or 'off'.");

					return false;
				}
			} else if (strcasecmp(argv[1], "usequitasaway") == 0) {
				if (strcasecmp(argv[2], "on") == 0) {
					Config->WriteInteger("user.quitaway", 1);
				} else if (strcasecmp(argv[2], "off") == 0) {
					Config->WriteInteger("user.quitaway", 0);
				} else {
					SENDUSER("Value must be either 'on' or 'off'.");

					return false;
				}
			} else if (strcasecmp(argv[1], "automodes") == 0) {
				ArgRejoinArray(argv, 2);
				GetOwner()->SetAutoModes(argv[2]);
			} else if (strcasecmp(argv[1], "dropmodes") == 0) {
				ArgRejoinArray(argv, 2);
				GetOwner()->SetDropModes(argv[2]);
			} else if (strcasecmp(argv[1], "ssl") == 0) {
				if (strcasecmp(argv[2], "on") == 0) {
					GetOwner()->SetSSL(true);
				} else if (strcasecmp(argv[2], "off") == 0) {
					GetOwner()->SetSSL(false);
				} else {
					SENDUSER("Value must be either 'on' or 'off'.");

					return false;
				}
#ifdef IPV6
			} else if (strcasecmp(argv[1], "ipv6") == 0) {
				if (strcasecmp(argv[2], "on") == 0) {
					SENDUSER("Please keep in mind that IPv6 will only work if your server actually has IPv6 connectivity.");

					GetOwner()->SetIPv6(true);
				} else if (strcasecmp(argv[2], "off") == 0) {
					GetOwner()->SetIPv6(false);
				} else {
					SENDUSER("Value must be either 'on' or 'off'.");

					return false;
				}
#endif
			} else if (strcasecmp(argv[1], "timezone") == 0) {
				GetOwner()->SetGmtOffset(atoi(argv[2]));
			} else if (strcasecmp(argv[1], "sysnotices") == 0) {
				if (strcasecmp(argv[2], "on") == 0) {
					GetOwner()->SetSystemNotices(true);
				} else if (strcasecmp(argv[2], "off") == 0) {
					GetOwner()->SetSystemNotices(false);
				} else {
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
			GetOwner()->AddClientCertificate(GetPeerCertificate());

			SENDUSER("Your certificate was stored and will be used for public key authentication.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "showcert") == 0) {
		int i = 0;
		char Buffer[300];
		const CVector<X509 *> *Certificates;
		X509_NAME *name;
		bool First = true;

		Certificates = GetOwner()->GetClientCertificates();

		for (unsigned int i = 0; i < Certificates->GetLength(); i++) {
			X509 *Certificate = (*Certificates)[i];

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
		const CVector<X509 *> *Certificates = GetOwner()->GetClientCertificates();

		if (id == 0 || Certificates->GetLength() > id) {
			Certificate = NULL;
		} else {
			Certificate = (*Certificates)[id - 1];
		}

		if (Certificate != NULL) {
			if (GetOwner()->RemoveClientCertificate(Certificate)) {
				SENDUSER("Done.");
			} else {
				SENDUSER("An error occured while removing the certificate.");
			}
		} else {
			SENDUSER("The ID you specified is not valid. Use the SHOWCERT command to get a list of valid IDs.");
		}

		return false;
#endif
	} else if (strcasecmp(Subcommand, "die") == 0 && GetOwner()->IsAdmin()) {
		g_Bouncer->Log("Shutdown requested by %s", GetOwner()->GetUsername());
		g_Bouncer->Shutdown();

		return false;
	} else if (strcasecmp(Subcommand, "reload") == 0 && GetOwner()->IsAdmin()) {
		if (argc >= 2) {
			g_Bouncer->GetLoaderParameters()->SetModulePath(argv[1]);
		}

		g_Bouncer->Log("Reload requested by %s", GetOwner()->GetUsername());
		g_Bouncer->InitializeFreeze();

		return false;
	} else if (strcasecmp(Subcommand, "adduser") == 0 && GetOwner()->IsAdmin()) {
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

	} else if (strcasecmp(Subcommand, "deluser") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: DELUSER username");
			return false;
		}

		if (strcasecmp(argv[1], GetOwner()->GetUsername()) == 0) {
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
	} else if (strcasecmp(Subcommand, "simul") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 3) {
			SENDUSER("Syntax: SIMUL username :command");
			return false;
		}

		CUser *User = g_Bouncer->GetUser(argv[1]);

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

		CIRCConnection *IRC = GetOwner()->GetIRCConnection();

		ArgRejoinArray(argv, 1);
		IRC->WriteLine("%s", argv[1]);

		return false;
	} else if (strcasecmp(Subcommand, "gvhost") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 2) {
			const char *Ip = g_Bouncer->GetConfig()->ReadString("system.vhost");

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
			const char *Motd = g_Bouncer->GetMotd();

			asprintf(&Out, "Current MOTD: %s", Motd ? Motd : "(none)");
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			if (Motd != NULL && GetOwner()->IsAdmin()) {
				if (NoticeUser) {
					SENDUSER("Use /sbnc motd remove to remove the motd.");
				} else {
					SENDUSER("Use /msg -sBNC motd remove to remove the motd.");
				}
			}
		} else if (GetOwner()->IsAdmin()) {
			ArgRejoinArray(argv, 1);

			if (strcasecmp(argv[1], "remove") == 0) {
				g_Bouncer->SetMotd(NULL);
			} else {
				g_Bouncer->SetMotd(argv[1]);
			}

			SENDUSER("Done.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "global") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: GLOBAL :text");
			return false;
		}

		ArgRejoinArray(argv, 1);
		g_Bouncer->GlobalNotice(argv[1]);
		return false;
	} else if (strcasecmp(Subcommand, "kill") == 0 && GetOwner()->IsAdmin()) {
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

		asprintf(&Out, "You were disconnected from the bouncer. (Requested by %s: %s)", GetOwner()->GetUsername(), Reason);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			return false;
		} CHECK_ALLOC_RESULT_END;

		if (User != NULL && User->GetClientConnection() != NULL) {
			User->GetClientConnection()->Kill(Out);
			SENDUSER("Done.");
		} else {
			SENDUSER("There is no such user or that user is not currently logged in.");
		}

		free(Out);

		return false;
	} else if (strcasecmp(Subcommand, "disconnect") == 0) {
		CUser *User;

		if (GetOwner()->IsAdmin() && argc >= 2) {
			User = g_Bouncer->GetUser(argv[1]);
		} else {
			User = GetOwner();
		}

		if (User == NULL) {
			SENDUSER("There is no such user.");
			return false;
		}

		CIRCConnection *IRC = User->GetIRCConnection();

		User->MarkQuitted(true);

		if (IRC == NULL) {
			if (User == GetOwner()) {
				SENDUSER("You are not connected to a server.");
			} else {
				SENDUSER("The user is not connected to a server.");
			}

			return false;
		}

		IRC->Kill("Requested.");

		SENDUSER("Done.");

		return false;
	} else if (strcasecmp(Subcommand, "jump") == 0) {
		if (GetOwner()->GetIRCConnection()) {
			GetOwner()->GetIRCConnection()->Kill("Reconnecting");

			GetOwner()->SetIRCConnection(NULL);
		}

		if (GetOwner()->GetServer() == NULL) {
			SENDUSER("Cannot reconnect: You haven't set a server yet.");
		} else {
			GetOwner()->ScheduleReconnect(5);
		}

		return false;
	} else if (strcasecmp(Subcommand, "status") == 0) {
		asprintf(&Out, "Username: %s", GetOwner()->GetUsername());
		CHECK_ALLOC_RESULT(Out, asprintf) { } else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

		asprintf(&Out, "This is shroudBNC %s", g_Bouncer->GetBouncerVersion());
		CHECK_ALLOC_RESULT(Out, asprintf) { } else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

		if (GetOwner()->IsAdmin()) {
			asprintf(&Out, "Using module %s", g_Bouncer->GetLoaderParameters()->GetModulePath());
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
		}

		asprintf(&Out, "You are %san admin.", GetOwner()->IsAdmin() ? "" : "not ");
		CHECK_ALLOC_RESULT(Out, asprintf) { } else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

		asprintf(&Out, "Client: sendq: %d, recvq: %d", GetSendqSize(), GetRecvqSize());
		CHECK_ALLOC_RESULT(Out, asprintf) { } else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

		CIRCConnection *IRC = GetOwner()->GetIRCConnection();

		if (IRC) {
			asprintf(&Out, "IRC: sendq: %d, recvq: %d", IRC->GetSendqSize(), IRC->GetRecvqSize());
			CHECK_ALLOC_RESULT(Out, asprintf) { } else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			SENDUSER("Channels:");

			int a = 0;

			while (hash_t<CChannel *> *Chan = IRC->GetChannels()->Iterate(a++)) {
				SENDUSER(Chan->Name);
			}

			SENDUSER("End of CHANNELS.");
		}

		asprintf(&Out, "Uptime: %d seconds", GetOwner()->GetIRCUptime());
		CHECK_ALLOC_RESULT(Out, asprintf) { } else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

		return false;
	} else if (strcasecmp(Subcommand, "impulse") == 0 && GetOwner()->IsAdmin()) {
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
	} else if (strcasecmp(Subcommand, "who") == 0 && GetOwner()->IsAdmin()) {
		char **Keys = g_Bouncer->GetUsers()->GetSortedKeys();
		int Count = g_Bouncer->GetUsers()->GetLength();

		for (int i = 0; i < Count; i++) {
			const char *Server, *ClientAddr;
			CUser *User = g_Bouncer->GetUser(Keys[i]);

			if (User == NULL) {
				continue;
			}

			if (User->GetIRCConnection()) {
				Server = User->GetIRCConnection()->GetServer();
			} else {
				Server = NULL;
			}

			if (User->GetClientConnection() != NULL) {
				ClientAddr = User->GetClientConnection()->GetPeerName();
			} else {
				ClientAddr = NULL;
			}

			const char *LastSeen;

			if (User->GetLastSeen() == 0) {
				LastSeen = "Never";
			} else if (User->GetClientConnection() != NULL) {
				LastSeen = "Now";
			} else {
				LastSeen = GetOwner()->FormatTime(User->GetLastSeen());
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
	} else if (strcasecmp(Subcommand, "addlistener") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 2) {
#ifdef USESSL
			SENDUSER("Syntax: addlistener <port> [address] [ssl]");
#else
			SENDUSER("Syntax: addlistener <port> [address]");
#endif

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

#ifdef USESSL
		if (argc > 3) {
			if (atoi(argv[3]) != 0 || strcasecmp(argv[3], "ssl") == 0) {
				SSL = true;
			}
		}
#endif

		RESULT<bool> Result = g_Bouncer->AddAdditionalListener(Port, Address, SSL);

		if (IsError(Result)) {
			SENDUSER(GETDESCRIPTION(Result));

			return false;
		}

		SENDUSER("Done.");

		return false;
	} else if (strcasecmp(Subcommand, "dellistener") == 0 && GetOwner()->IsAdmin()) {
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
	} else if (strcasecmp(Subcommand, "listeners") == 0 && GetOwner()->IsAdmin()) {
		if (g_Bouncer->GetMainListener() != NULL) {
			asprintf(&Out, "Main listener: port %d", g_Bouncer->GetMainListener()->GetPort());
		} else {
			Out = strdup("Main listener: none");
		}

		CHECK_ALLOC_RESULT(&Out, asprintf) {} else {
			SENDUSER(Out);
			free(Out);
		} CHECK_ALLOC_RESULT_END;

#ifdef USESSL
		if (g_Bouncer->GetMainSSLListener() != NULL) {
			asprintf(&Out, "Main SSL listener: port %d", g_Bouncer->GetMainSSLListener()->GetPort());
		} else {
			Out = strdup("Main SSL listener: none");
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
#ifdef USESSL
			if ((*Listeners)[i].SSL) {
				if ((*Listeners)[i].BindAddress != NULL) {
					asprintf(&Out, "Port: %d (SSL, bound to %s)", (*Listeners)[i].Port, (*Listeners)[i].BindAddress);
				} else {
					asprintf(&Out, "Port: %d (SSL)", (*Listeners)[i].Port);
				}
			} else {
#endif
				if ((*Listeners)[i].BindAddress != NULL) {
					asprintf(&Out, "Port: %d (bound to %s)", (*Listeners)[i].Port, (*Listeners)[i].BindAddress);
				} else {
					asprintf(&Out, "Port: %d", (*Listeners)[i].Port);
				}
#ifdef USESSL
			}
#endif
			CHECK_ALLOC_RESULT(Out, asprintf) {} else {
				SENDUSER(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;
		}

		SENDUSER("End of LISTENERS.");

		return false;
	} else if (strcasecmp(Subcommand, "read") == 0) {
		GetOwner()->GetLog()->PlayToUser(GetOwner(), NoticeUser);

		if (!GetOwner()->GetLog()->IsEmpty()) {
			if (NoticeUser) {
				GetOwner()->RealNotice("End of LOG. Use '/sbnc erase' to remove this log.");
			} else {
				GetOwner()->Privmsg("End of LOG. Use '/msg -sBNC erase' to remove this log.");
			}
		} else {
			SENDUSER("Your personal log is empty.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "erase") == 0) {
		if (GetOwner()->GetLog()->IsEmpty()) {
			SENDUSER("Your personal log is empty.");
		} else {
			GetOwner()->GetLog()->Clear();
			SENDUSER("Done.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "playmainlog") == 0 && GetOwner()->IsAdmin()) {
		g_Bouncer->GetLog()->PlayToUser(GetOwner(), NoticeUser);

		if (!g_Bouncer->GetLog()->IsEmpty()) {
			if (NoticeUser) {
				GetOwner()->RealNotice("End of LOG. Use /sbnc erasemainlog to remove this log.");
			} else {
				GetOwner()->Privmsg("End of LOG. Use /msg -sBNC erasemainlog to remove this log.");
			}
		} else {
			SENDUSER("The main log is empty.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "erasemainlog") == 0 && GetOwner()->IsAdmin()) {
		g_Bouncer->GetLog()->Clear();
		g_Bouncer->Log("User %s erased the main log", GetOwner()->GetUsername());
		SENDUSER("Done.");

		return false;
	} else if (strcasecmp(Subcommand, "hosts") == 0 && GetOwner()->IsAdmin()) {
		const CVector<char *> *Hosts = g_Bouncer->GetHostAllows();

		SENDUSER("Hostmasks");
		SENDUSER("---------");

		for (unsigned int i = 0; i < Hosts->GetLength(); i++) {
			SENDUSER((*Hosts)[i]);
		}

		if (Hosts->GetLength() == 0) {
			SENDUSER("*");
		}

		SENDUSER("End of HOSTS.");

		return false;
	} else if (strcasecmp(Subcommand, "hostadd") == 0 && GetOwner()->IsAdmin()) {
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
	} else if (strcasecmp(Subcommand, "hostdel") == 0 && GetOwner()->IsAdmin()) {
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
	} else if (strcasecmp(Subcommand, "admin") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: ADMIN username");

			return false;
		}

		CUser *User = g_Bouncer->GetUser(argv[1]);

		if (User) {
			User->SetAdmin(true);

			SENDUSER("Done.");
		} else {
			SENDUSER("There's no such user.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "unadmin") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: UNADMIN username");

			return false;
		}

		CUser *User = g_Bouncer->GetUser(argv[1]);
		
		if (User) {
			User->SetAdmin(false);

			SENDUSER("Done.");
		} else {
			SENDUSER("There's no such user.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "suspend") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: SUSPEND username :reason");

			return false;
		}

		CUser *User = g_Bouncer->GetUser(argv[1]);

		if (User) {
			User->Lock();

			if (User->GetClientConnection() != NULL) {
				User->GetClientConnection()->Kill("Your account has been suspended.");
			}

			if (User->GetIRCConnection() != NULL) {
				User->GetIRCConnection()->Kill("Requested.");
			}

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
	} else if (strcasecmp(Subcommand, "unsuspend") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 2) {
			SENDUSER("Syntax: UNSUSPEND username");

			return false;
		}

		CUser *User = g_Bouncer->GetUser(argv[1]);
		
		if (User) {
			User->Unlock();

			g_Bouncer->Log("User %s has been unsuspended.", User->GetUsername());

			User->SetSuspendReason(NULL);

			SENDUSER("Done.");
		} else {
			SENDUSER("There's no such user.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "resetpass") == 0 && GetOwner()->IsAdmin()) {
		if (argc < 3) {
			SENDUSER("Syntax: RESETPASS username new-password");

			return false;
		}

		CUser *User = g_Bouncer->GetUser(argv[1]);
		
		if (User) {
			User->SetPassword(argv[2]);

			SENDUSER("Done.");
		} else {
			SENDUSER("There's no such user.");
		}

		return false;
	} else if (strcasecmp(Subcommand, "partall") == 0) {
		if (GetOwner()->GetIRCConnection()) {
			const char *Channels = GetOwner()->GetConfigChannels();

			if (Channels != NULL) {
				GetOwner()->GetIRCConnection()->WriteLine("PART %s", Channels);
			}
		}

		GetOwner()->SetConfigChannels(NULL);

		SENDUSER("Done.");

		return false;
	}

	if (NoticeUser) {
		GetOwner()->RealNotice("Unknown command. Try /sbnc help");
	} else {
		SENDUSER("Unknown command. Try /msg -sBNC help");
	}

	return false;
}

/**
 * ParseLineArgV
 *
 * Parses and processes a line which was sent by the user.
 *
 * @param argc number of tokens in the line
 * @param argv tokenized line
 */
bool CClientConnection::ParseLineArgV(int argc, const char **argv) {
	char *Out;

	if (argc == 0) {
		return false;
	}

	const CVector<CModule *> *Modules = g_Bouncer->GetModules();

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
		if (!(*Modules)[i]->InterceptClientMessage(this, argc, argv)) {
			return false;
		}
	}

	const char *Command = argv[0];

	if (GetOwner() == NULL) {
		if (strcasecmp(Command, "nick") == 0 && argc > 1) {
			const char *Nick = argv[1];

			if (m_Nick != NULL) {
				if (strcmp(m_Nick, Nick) != 0) {
					WriteLine(":%s!ident@sbnc NICK :%s", m_Nick, Nick);
				}
			}

			ufree(m_Nick);
			m_Nick = ustrdup(Nick);

			if (m_Username != NULL && m_Password != NULL) {
				ValidateUser();
			} else if (m_Username != NULL) {
				WriteUnformattedLine(":Notice!notice@shroudbnc.info NOTICE * :*** This server requires a password. Use /QUOTE PASS thepassword to supply a password now.");
			}
		} else if (strcasecmp(Command, "pass") == 0) {
			if (argc < 2) {
				WriteLine(":bouncer 461 %s :Not enough parameters", m_Nick);
			} else {
				m_Password = ustrdup(argv[1]);
			}

			if (m_Nick != NULL && m_Username != NULL && m_Password != NULL) {
				ValidateUser();
			}

			return false;
		} else if (strcasecmp(Command, "user") == 0 && argc > 1) {
			if (m_Username) {
				WriteLine(":bouncer 462 %s :You may not reregister", m_Nick);
			} else {
				if (!argv[1]) {
					WriteLine(":bouncer 461 %s :Not enough parameters", m_Nick);
				} else {
					const char *Username = argv[1];

					m_Username = ustrdup(Username);
				}
			}

			bool ValidSSLCert = false;

			if (m_Nick != NULL && m_Username != NULL) {
				if (m_Password != NULL || GetPeerCertificate() != NULL) {
					ValidSSLCert = ValidateUser();
				}

				if (m_Password == NULL && !ValidSSLCert) {
					WriteUnformattedLine(":Notice!notice@shroudbnc.info NOTICE * :*** This server requires a password. Use /QUOTE PASS thepassword to supply a password now.");
				}
			}

			return false;
		} else if (strcasecmp(Command, "quit") == 0) {
			Kill("*** Thanks for flying with shroudBNC :P");

			return false;
		}
	}

	if (GetOwner() != NULL) {
		if (strcasecmp(Command, "quit") == 0) {
			bool QuitAsAway = (GetOwner()->GetConfig()->ReadInteger("user.quitaway") != 0);

			if (QuitAsAway && argc > 1 && argv[1][0] != '\0') {
				GetOwner()->SetAwayText(argv[1]);
			}

			Kill("*** Thanks for flying with shroudBNC :P");
			return false;
		} else if (strcasecmp(Command, "nick") == 0) {
			if (argc >= 2) {
				ufree(m_Nick);
				m_Nick = ustrdup(argv[1]);
				GetOwner()->GetConfig()->WriteString("user.nick", argv[1]);
			}
		} else if (argc > 1 && strcasecmp(Command, "join") == 0) {
			CIRCConnection *IRC;
			const char *Key;

			if (argc > 2 && strstr(argv[0], ",") == NULL && strstr(argv[1], ",") == NULL) {
				GetOwner()->GetKeyring()->SetKey(argv[1], argv[2]);
			} else if (GetOwner()->GetKeyring() != NULL && (Key = GetOwner()->GetKeyring()->GetKey(argv[1])) != NULL && (IRC = GetOwner()->GetIRCConnection()) != NULL) {
				IRC->WriteLine("JOIN %s %s", argv[1], Key);

				return false;
			}
		} else if (strcasecmp(Command, "whois") == 0) {
			if (argc >= 2) {
				const char *Nick = argv[1];

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

				IRC = GetOwner()->GetIRCConnection();

				if (IRC != NULL) {
					Server = IRC->GetServer();
				} else {
					Server = "bouncer";
				}

				Ident = GetOwner()->GetIdent();

				if (Ident == NULL) {
					Ident = GetOwner()->GetUsername();
				}

				WriteLine(":%s 302 %s :%s=+%s@%s", Server, m_Nick, m_Nick, Ident, m_PeerName);

				return false;
			}
		} else if (strcasecmp(Command, "protoctl") == 0) {
			if (argc > 1 && strcasecmp(argv[1], "namesx") == 0) {
				m_NamesXSupport = true;

				return false;
			}
		} else if (strcasecmp(Command, "sbnc") == 0) {
			return ProcessBncCommand(argv[1], argc - 1, &argv[1], true);
		} else if (strcasecmp(Command, "synth") == 0) {
			if (argc < 2) {
				GetOwner()->Privmsg("Syntax: SYNTH command parameter");
				GetOwner()->Privmsg("supported commands are: mode, topic, names, version, who");

				return false;
			}

			if (strcasecmp(argv[1], "mode") == 0 && argc > 2) {
				CIRCConnection *IRC = GetOwner()->GetIRCConnection();

				if (IRC) {
					CChannel *Chan = IRC->GetChannel(argv[2]);

					if (argc == 3) {
						if (Chan && Chan->AreModesValid()) {
							WriteLine(":%s 324 %s %s %s", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], (const char *)Chan->GetChannelModes());
							WriteLine(":%s 329 %s %s %d", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Chan->GetCreationTime());
						} else
							IRC->WriteLine("MODE %s", argv[2]);
					} else if (argc == 4 && strcmp(argv[3],"+b") == 0) {
						if (Chan && Chan->HasBans()) {
							CBanlist *Bans = Chan->GetBanlist();

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
				CIRCConnection *IRC = GetOwner()->GetIRCConnection();

				if (IRC) {
					CChannel *Chan = IRC->GetChannel(argv[2]);

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
				CIRCConnection *IRC = GetOwner()->GetIRCConnection();

				if (IRC) {
					CChannel *Chan = IRC->GetChannel(argv[2]);

					if (Chan && Chan->HasNames() != 0) {
						char *Nicks = (char *)malloc(1);
						Nicks[0] = '\0';

						const CHashtable<CNick *, false, 64> *H = Chan->GetNames();

						int a = 0;

						while (hash_t<CNick *> *NickHash = H->Iterate(a++)) {
							size_t Size;
							CNick *NickObj = NickHash->Value;

							const char *Prefix = NickObj->GetPrefixes();
							const char *Nick = NickObj->GetNick();

							const char *outPref;
							char outPrefTemp[2] = { IRC->GetHighestUserFlag(Prefix), '\0' };

							if (m_NamesXSupport) {
								if (Prefix != NULL) {
									outPref = Prefix;
								} else {
									outPref = "";
								}
							} else {
								outPref = outPrefTemp;
							}

							if (Nick == NULL) {
								continue;
							}

							Size = (Nicks ? strlen(Nicks) : 0) + strlen(outPref) + strlen(Nick) + 2;
							Nicks = (char *)realloc(Nicks, Size);

							if (Nicks == NULL) {
								Kill("CClientConnection::ParseLineArgV: realloc() failed. Please reconnect.");

								return false;
							}

							if (Nicks[0] != '\0') {
								strmcat(Nicks, " ", Size);
							}

							strmcat(Nicks, outPref, Size);
							strmcat(Nicks, Nick, Size);

							if (strlen(Nicks) > 400) {
								WriteLine(":%s 353 %s = %s :%s", IRC->GetServer(), IRC->GetCurrentNick(), argv[2], Nicks);

								Nicks = (char *)realloc(Nicks, 1);

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
			} else if (strcasecmp(argv[1], "who") == 0 && argc > 2) {
				CIRCConnection *IRC = GetOwner()->GetIRCConnection();

				if (IRC) {
					CChannel *Channel = IRC->GetChannel(argv[2]);

					if (Channel && g_CurrentTime - GetOwner()->GetLastSeen() < 300 && Channel->SendWhoReply(true)) {
						Channel->SendWhoReply(false);
					} else {
						IRC->WriteLine("WHO %s", argv[2]);
					}
				}
			} else if (strcasecmp(argv[1], "version") == 0 && argc == 2) {
				CIRCConnection *IRC = GetOwner()->GetIRCConnection();

				if (IRC != NULL) {
					const char *ServerVersion = IRC->GetServerVersion();
					const char *ServerFeat = IRC->GetServerFeat();

					if (ServerVersion != NULL && ServerFeat != NULL) {
						WriteLine(":%s 351 %s %s %s :%s", IRC->GetServer(), IRC->GetCurrentNick(), ServerVersion, IRC->GetServer(), ServerFeat);
					} else {
						IRC->WriteLine("VERSION");

						return false;
					}

					char *Feats = (char *)malloc(1);
					Feats[0] = '\0';

					int a = 0, i = 0;

					while (hash_t<char *> *Feat = IRC->GetISupportAll()->Iterate(i++)) {
						size_t Size;
						char *Name = Feat->Name;
						char *Value = Feat->Value;

						Size = (Feats ? strlen(Feats) : 0) + strlen(Name) + 1 + strlen(Value) + 2;
						Feats = (char *)realloc(Feats, Size);

						if (Feats == NULL) {
							Kill("CClientConnection::ParseLineArgV: realloc() failed. Please reconnect.");

							return false;
						}


						if (Feats[0] != '\0') {
							strmcat(Feats, " ", Size);
						}

						strmcat(Feats, Name, Size);

						if (Value != NULL && Value[0] != '\0') {
							strmcat(Feats, "=", Size);
							strmcat(Feats, Value, Size);
						}

						if (++a == 11) {
							WriteLine(":%s 005 %s %s :are supported by this server", IRC->GetServer(), IRC->GetCurrentNick(), Feats);

							Feats = (char *)realloc(Feats, 1);

							if (Feats == NULL) {
								Kill("CClientConnection::ParseLineArgV: realloc() failed. Please reconnect.");

								return false;
					
							}

							*Feats = '\0';
							a = 0;
						}
					}

					if (a > 0) {
						WriteLine(":%s 005 %s %s :are supported by this server", IRC->GetServer(), IRC->GetCurrentNick(), Feats);
					}

					free(Feats);
				}
			}

			return false;
		} else if (strcasecmp(Command, "mode") == 0 || strcasecmp(Command, "topic") == 0 ||
				strcasecmp(Command, "names") == 0 || strcasecmp(Command, "who") == 0) {
			if (argc == 2 || (strcasecmp(Command, "mode") == 0 && argc == 3) && strcmp(argv[2],"+b") == 0) {
				if (argc == 2) {
					asprintf(&Out, "SYNTH %s :%s", argv[0], argv[1]);
				} else {
					asprintf(&Out, "SYNTH %s %s :%s", argv[0], argv[1], argv[2]);
				}

			CHECK_ALLOC_RESULT(Out, asprintf) {} else {
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

/**
 * ParseLine
 *
 * Tokenizes, parses and processes a line which was sent by the user.
 *
 * @param Line the line
 */
void CClientConnection::ParseLine(const char *Line) {
	if (strlen(Line) > 512) {
		return; // protocol violation
	}

	bool ReturnValue;
	tokendata_t Args;
	const char **argv, **real_argv;
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

	if (GetOwner() != NULL && ReturnValue) {
		CIRCConnection *IRC = GetOwner()->GetIRCConnection();

		if (IRC != NULL) {
			IRC->WriteLine("%s", Line);
		}
	}
}

/**
 * ValidateUser
 *
 * Logs in a user.
 */
bool CClientConnection::ValidateUser(void) {
	bool Force = false;
	CUser *User;
	bool Blocked = true, Valid = false;
	sockaddr *Remote;

	Remote = GetRemoteAddress();

	if (Remote == NULL) {
		return false;
	}

#ifdef USESSL
	int Count = 0;
	bool MatchUsername = false;
	X509 *PeerCert = NULL;
	CUser *AuthUser = NULL;

	if (IsSSL() && (PeerCert = (X509 *)GetPeerCertificate()) != NULL) {
		int i = 0;

		if (!g_Bouncer->GetConfig()->ReadInteger("system.dontmatchuser")) {
			CUser *User = g_Bouncer->GetUser(m_Username);

			if (User != NULL && User->FindClientCertificate(PeerCert)) {
				AuthUser = User;
				MatchUsername = true;
				Count = 1;
			}
		} else {
			while (hash_t<CUser *> *UserHash = g_Bouncer->GetUsers()->Iterate(i++)) {
				if (UserHash->Value->FindClientCertificate(PeerCert)) {
					AuthUser = UserHash->Value;
					Count++;

					if (strcasecmp(UserHash->Name, m_Username) == 0) {
						MatchUsername = true;
					}
				}
			}
		}

		X509_free(PeerCert);

		if (AuthUser && Count == 1) { // found a single user who has this public certificate
			ufree(m_Username);
			m_Username = ustrdup(AuthUser->GetUsername());
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

/**
 * GetNick
 *
 * Returns the current nick of the user.
 */
const char *CClientConnection::GetNick(void) const {
	return m_Nick;
}

/**
 * Destroy
 *
 * Destroys the client connection object.
 */
void CClientConnection::Destroy(void) {
	if (GetOwner() != NULL) {
		GetOwner()->SetClientConnection(NULL);
	}

	delete this;
}

/**
 * SetPeerName
 *
 * Sets the hostname of the peer.
 *
 * @param PeerName the peer's name
 * @param LookupFailure whether an error occurred while looking
 *						up the remote user's hostname
 */
void CClientConnection::SetPeerName(const char *PeerName, bool LookupFailure) {
	sockaddr *Remote;

	if (m_PeerName != NULL) {
		ufree(m_PeerName);
	}

	m_PeerName = ustrdup(PeerName);

	Remote = GetRemoteAddress();

	if (!g_Bouncer->CanHostConnect(m_PeerName) && (Remote == NULL || !g_Bouncer->CanHostConnect(IpToString(Remote)))) {
		g_Bouncer->Log("Attempted login from %s[%s]: Host does not match any host allows.", m_PeerName, (Remote != NULL) ? IpToString(Remote) : "unknown ip");

		FlushSendQ();

		Kill("Your host is not allowed to use this bouncer.");

		return;
	}

	ProcessBuffer();
}

/**
 * GetPeerName
 *
 * Returns the peer's hostname.
 */
const char *CClientConnection::GetPeerName(void) const {
	return m_PeerName;
}

/**
 * AsyncDnsFinishedClient
 *
 * Called by the CDnsQuery class when the DNS query for the client's
 * hostname has finished.
 *
 * @param Reponse the response from the DNS server
 */
void CClientConnection::AsyncDnsFinishedClient(hostent *Response) {
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
			m_PeerNameTemp = ustrdup(Response->h_name);

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
					ufree(m_PeerNameTemp);

					WriteLine(":Notice!notice@shroudbnc.info NOTICE * :*** Forward DNS reply received. (%s)", m_PeerName);

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

/**
 * GetClassName
 *
 * Returns the classname.
 */
const char *CClientConnection::GetClassName(void) const {
	return "CClientConnection";
}

/**
 * Read
 *
 * Called when data can be read for this connection.
 *
 * @param DontProcess whether to process the data
 */
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

/**
 * WriteUnformattedLine
 *
 * Sends a line to the client.
 *
 * @param Line the line
 */
void CClientConnection::WriteUnformattedLine(const char *Line) {
	CConnection::WriteUnformattedLine(Line);

	if (GetOwner() != NULL && !GetOwner()->IsAdmin() && GetSendqSize() > g_Bouncer->GetSendqSize() * 1024) {
		FlushSendQ();
		CConnection::WriteUnformattedLine("");
		Kill("SendQ exceeded.");
	}
}

/**
 * Freeze
 *
 * Persists this client connection object.
 *
 * @param Box the box which should be used for storing the persisted data
 */
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

/**
 * Thaw
 *
 * Depersists a client connection object.
 *
 * @param Box the box which was used for storing the connection object
 * @param Owner the user
 */
RESULT<CClientConnection *> CClientConnection::Thaw(CAssocArray *Box, CUser *Owner) {
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

	Client->SetOwner(Owner);
	Client->SetSocket(Socket);

	Temp = Box->ReadString("~client.peername");

	if (Temp != NULL) {
		Client->m_PeerName = nstrdup(Temp);
	} else {
		if (Client->GetRemoteAddress() != NULL) {
			Client->m_PeerName = nstrdup(IpToString(Client->GetRemoteAddress()));
		} else {
			Client->m_PeerName = nstrdup("<unknown>");
		}
	}

	Temp = Box->ReadString("~client.nick");

	if (Temp != NULL) {
		Client->m_Nick = nstrdup(Temp);
	} else {
		delete Client;

		THROW(CClientConnection *, Generic_Unknown, "Persistent data is invalid: Missing nickname.");
	}

	RETURN(CClientConnection *, Client);
}

/**
 * Kill
 *
 * Sends an error message to the client and closes the connection.
 *
 * @param Error the error message
 */
void CClientConnection::Kill(const char *Error) {
	if (GetOwner() != NULL) {
		GetOwner()->SetClientConnection(NULL);
		SetOwner(NULL);
	}

	WriteLine(":Notice!notice@shroudbnc.info NOTICE * :%s", Error);

	CConnection::Kill(Error);
}

/**
 * GetCommandList
 *
 * Returns a list of commands (used by the "help" command).
 */
commandlist_t *CClientConnection::GetCommandList(void) {
	return &m_CommandList;
}

/**
 * SetPreviousNick
 *
 * Sets the nick which was previously used for an IRC connection.
 *
 * @param Nick the nick
 */
void CClientConnection::SetPreviousNick(const char *Nick) {
	ufree(m_PreviousNick);
	m_PreviousNick = ustrdup(Nick);
}

/**
 * GetPreviousNick
 *
 * Returns the previous nick.
 */
const char *CClientConnection::GetPreviousNick(void) const {
	return m_PreviousNick;
}

/**
 * Hijack
 *
 * Detaches the socket from this connection object and returns it.
 */
SOCKET CClientConnection::Hijack(void) {
	SOCKET Socket;

	Socket = GetSocket();
	g_Bouncer->UnregisterSocket(Socket);
	SetSocket(INVALID_SOCKET);

	Kill("Hijacked!");

	return Socket;
}

/**
 * ClientAuthTimer
 *
 * Closes client connections which have timed out (due to the client
 * not sending a username/password).
 *
 * @param Now the current time
 * @param Client the client connection object
 */
bool ClientAuthTimer(time_t Now, void *Client) {
	CClientConnection *ClientConnection = (CClientConnection *)Client;

	ClientConnection->Kill("*** Connection timed out. Please reconnect and log in.");
	ClientConnection->m_AuthTimer = NULL;

	return false;
}
