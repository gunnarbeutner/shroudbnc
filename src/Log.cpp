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
 * CLog
 *
 * Constructs a log object.
 *
 * @param Filename the filename of the log, can be NULL to indicate that
 *                 any log messages should be discarded
 * @param KeepOpen whether to keep the file open
 */
CLog::CLog(const char *Filename, bool KeepOpen) {
	if (Filename != NULL) {
		m_Filename = strdup(Filename);

		CHECK_ALLOC_RESULT(m_Filename, ustrdup) {} CHECK_ALLOC_RESULT_END;
	} else {
		m_Filename = NULL;
	}

	m_KeepOpen = KeepOpen;
	m_File = NULL;
}

/**
 * ~CLog
 *
 * Destructs a log object.
 */
CLog::~CLog(void) {
	free(m_Filename);

	if (m_File != NULL) {
		fclose(m_File);
	}
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
void CLog::PlayToUser(CUser *User, int Type) const {
	FILE *LogFile;

	CIRCConnection *IRC = User->GetIRCConnection();
	CClientConnection *Client = User->GetClientConnection();
	const char *Nick;
	const char *Server;

	LogFile = m_File;

	if (m_Filename != NULL && (m_File != NULL || (LogFile = fopen(m_Filename, "r")) != NULL)) {
		char Line[500];

		while (!feof(LogFile)) {
			char *LinePtr = fgets(Line, sizeof(Line), LogFile);

			if (LinePtr == NULL) {
				continue;
			}

			if (Type == Log_Notice) {
				User->RealNotice(Line);
			} else if (Type == Log_Message) {
				User->Privmsg(Line);
			} else if (Type == Log_Motd) {
				if (IRC != NULL) {
					Nick = IRC->GetCurrentNick();
					Server = IRC->GetServer();
				} else {
					Nick = Client->GetNick();
					Server = "bouncer.shroudbnc.info";
				}

				if (Client != NULL) {
					Client->WriteLine(":%s 372 %s :%s", Server,	Nick, Line);
				}
			}
		}

		if (!m_KeepOpen) {
			fclose(LogFile);
		} else {
			m_File = LogFile;
		}
	}

	if (Type == Log_Motd && Client != NULL) {
		Client->WriteLine(":%s 376 %s :End of /MOTD command.", Server, Nick);
	}
}

/**
 * WriteUnformattedLine
 *
 * Writes a new log entry.
 *
 * @param Timestamp a timestamp, if you specify NULL, a suitable default timestamp will be used
 * @param Line the log entry
 */
void CLog::WriteUnformattedLine(const char *Timestamp, const char *Line) {
	char *Out = NULL;
	tm Now;
	time_t CurrentTime;
	char strNow[100];
	FILE *LogFile;

	LogFile = m_File;

	if (m_Filename == NULL || (m_File == NULL && (LogFile = fopen(m_Filename, "a")) == NULL)) {
		return;
	}

	SetPermissions(m_Filename, S_IRUSR | S_IWUSR);

	if (Timestamp == NULL) {
		time(&CurrentTime);

		Now = *localtime(&CurrentTime);

#ifdef _WIN32
		strftime(strNow, sizeof(strNow), "%#c" , &Now);
#else
		strftime(strNow, sizeof(strNow), "%c" , &Now);
#endif

		Timestamp = strNow;
	}

	asprintf(&Out, "%s: %s\n", Timestamp, Line);

	if (Out == NULL) {
		LOGERROR("asprintf() failed.");

		return;
	}

	fputs(Out, LogFile);
	printf("%s", Out);

	free(Out);

	if (!m_KeepOpen) {
		fclose(LogFile);
	} else {
		m_File = LogFile;
	}
}

/**
 * WriteLine
 *
 * Formats a string and writes it into the log.
 *
 * @param Timestamp a timestamp, if you specify NULL, a suitable default timestamp will be used
 * @param Format the format string
 * @param ... parameters used in the format string
 */
void CLog::WriteLine(const char *Timestamp, const char *Format, ...) {
	char *Out;
	va_list marker;

	va_start(marker, Format);
	vasprintf(&Out, Format, marker);
	va_end(marker);

	if (Out == NULL) {
		LOGERROR("vasprintf() failed.");

		return;
	}

	WriteUnformattedLine(Timestamp, Out);

	free(Out);
}

/**
 * Clear
 *
 * Erases the contents of the log.
 */
void CLog::Clear(void) {
	FILE *LogFile;

	if (m_File != NULL) {
		fclose(m_File);
	}

	if (m_Filename != NULL && (LogFile = fopen(m_Filename, "w")) != NULL) {
		SetPermissions(m_Filename, S_IRUSR | S_IWUSR);

		if (!m_KeepOpen) {
			fclose(LogFile);
		} else {
			m_File = LogFile;
		}
	}
}

/**
 * IsEmpty
 *
 * Checks whether the log is empty.
 */
bool CLog::IsEmpty(void) const {
	char Line[500];
	FILE *LogFile;

	if (m_Filename == NULL || (LogFile = fopen(m_Filename, "r")) == NULL) {
		return true;
	}

	while (!feof(LogFile)) {
		char *LinePtr = fgets(Line, sizeof(Line), LogFile);

		if (LinePtr != NULL) {
			fclose(LogFile);

			return false;
		}
	}

	fclose(LogFile);

	return true;
}

/**
 * GetFilename
 *
 * Returns the filename of the log, or NULL
 * if the log is not persistant.
 */
const char *CLog::GetFilename(void) const {
	if (m_Filename[0] != '\0') {
		return m_Filename;
	} else {
		return NULL;
	}
}
