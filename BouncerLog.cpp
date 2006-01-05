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

/**
 * CBouncerLog
 *
 * Constructs a log object.
 *
 * @param Filename the filename of the log, can be NULL to indicate that
 *                 any log messages should be discarded
 */
CBouncerLog::CBouncerLog(const char *Filename) {
	if (Filename) {
		m_File = strdup(Filename);

		if (m_File == NULL) {
			if (g_Bouncer) {
				LOGERROR("strdup() failed.");

				g_Bouncer->Fatal();
			} else {
				printf("CBouncerLog::CBouncerLog: strdup() failed (%s).",
					Filename);

				exit(1);
			}
		}
	} else
		m_File = NULL;
}

/**
 * ~CBouncerLog
 *
 * Destructs a log object.
 */
CBouncerLog::~CBouncerLog() {
	free(m_File);
}

/**
 * PlayToUser
 *
 * Sends the log to the specified user.
 *
 * @param User the user who should receive the log
 * @param Type specifies how the log should be sent, can be one of:
 *             Log_Notices - use IRC notices
 *             Log_Messages - use IRC messages
 *             Log_Motd - use IRC motd replies
 */
void CBouncerLog::PlayToUser(CBouncerUser *User, int Type) {
	FILE *Log;

	CIRCConnection *IRC = User->GetIRCConnection();
	CClientConnection *Client = User->GetClientConnection();
	const char *Nick;
	const char *Server;

	if (m_File && (Log = fopen(m_File, "r"))) {
		char Line[500];
		while (!feof(Log)) {
			char* n = fgets(Line, sizeof(Line), Log);

			if (n) {
				if (Type == Log_Notice)
					User->RealNotice(Line);
				else if (Type == Log_Message)
					User->Notice(Line);
				else if (Type == Log_Motd) {
					if (IRC) {
						Nick = IRC->GetCurrentNick();
						Server = IRC->GetServer();
					} else {
						Nick = Client->GetNick();
						Server = "bouncer.shroudbnc.org";
					}

					if (Client) {
						Client->WriteLine(":%s 372 %s :%s", Server,
							Nick, Line);
					}
				}
			}
		}

		fclose(Log);
	}

	if (Type == Log_Motd && Client)
		Client->WriteLine(":%s 376 %s :End of /MOTD command.", Server, Nick);
}

/**
 * InternalWriteLine
 *
 * Writes a new log entry.
 *
 * @param Line the log entry
 */
void CBouncerLog::InternalWriteLine(const char* Line) {
	FILE *Log;

	if (m_File && (Log = fopen(m_File, "a")) != NULL) {
		char *Out;
		tm Now;
		time_t tNow;
		char strNow[100];

		time(&tNow);
		Now = *localtime(&tNow);

#ifdef _WIN32
		strftime(strNow, sizeof(strNow), "%#c" , &Now);
#else
		strftime(strNow, sizeof(strNow), "%c" , &Now);
#endif

		asprintf(&Out, "%s %s\n", strNow, Line);

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");

			return;
		}

		fputs(Out, Log);
		printf("%s", Out);

		free(Out);

		fclose(Log);
	}
}

/**
 * WriteLine
 *
 * Formats a string and writes it into the log.
 *
 * @param Format the format string
 * @param ... parameters used in the format string
 */
void CBouncerLog::WriteLine(const char* Format, ...) {
	char *Out;
	va_list marker;

	va_start(marker, Format);
	vasprintf(&Out, Format, marker);
	va_end(marker);

	if (Out == NULL) {
		LOGERROR("vasprintf() failed.");

		return;
	}

	InternalWriteLine(Out);

	free(Out);
}

/**
 * Clear
 *
 * Erases the contents of the log.
 */
void CBouncerLog::Clear(void) {
	FILE *Log;
	
	if (m_File && (Log = fopen(m_File, "w")) != NULL)
		fclose(Log);
}

/**
 * IsEmpty
 *
 * Checks whether the log is empty.
 */
bool CBouncerLog::IsEmpty(void) {
	FILE *Log;

	if (m_File && (Log = fopen(m_File, "r")) != NULL) {
		char Line[500];

		while (!feof(Log)) {
			char *n = fgets(Line, sizeof(Line), Log);

			if (n) {
				fclose(Log);

				return false;
			}
		}

		fclose(Log);
	}

	return true;
}
