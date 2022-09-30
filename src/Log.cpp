/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2014 Gunnar Beutner                                      *
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
		m_Filename = strdup(g_Bouncer->BuildPathLog(Filename));

		if (AllocFailed(m_Filename)) {}
	} else {
		m_Filename = NULL;
	}

	m_KeepOpen = KeepOpen;
	m_File = NULL;

#ifndef _WIN32
	m_Inode = 0;
	m_Dev = 0;
#endif
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
 * @param Client the user who should receive the log
 * @param Type specifies how the log should be sent, can be one of:
 *             Log_Notices - use IRC notices
 *             Log_Messages - use IRC messages
 *             Log_Motd - use IRC motd replies
 */
void CLog::PlayToUser(CClientConnection *Client, LogType Type) const {
	FILE *LogFile;

	CIRCConnection *IRC = Client->GetOwner()->GetIRCConnection();
	const char *Nick = NULL;
	const char *Server = NULL;

	if (m_File != NULL) {
		fclose(m_File);
	}

	if (m_Filename != NULL && (LogFile = fopen(m_Filename, "r")) != NULL) {
		char Line[500];

		while (!feof(LogFile)) {
			char *LinePtr = fgets(Line, sizeof(Line), LogFile);

			if (LinePtr == NULL) {
				continue;
			}
			char *TempLinePtr = Line;

			while (*TempLinePtr) {
				if (*TempLinePtr == '\r' || *TempLinePtr == '\n') {
					*TempLinePtr = '\0';

					break;
				}

				TempLinePtr++;
			}

			if (Type == Log_Notice) {
				Client->RealNotice(Line);
			} else if (Type == Log_Message) {
				Client->Privmsg(Line);
			} else if (Type == Log_Motd) {
				if (IRC != NULL) {
					Nick = IRC->GetCurrentNick();
					Server = IRC->GetServer();
				} else {
					Nick = Client->GetNick();
					Server = "sbnc.beutner.name";
				}

				if (Nick != NULL) {
					Client->WriteLine(":%s 372 %s :%s", Server, Nick, Line);
				}
			}
		}

		fclose(LogFile);
		m_File = NULL;
	}

	if (Type == Log_Motd && Nick != NULL && Server != NULL) {
		Client->WriteLine(":%s 376 %s :End of /MOTD command.", Server, Nick);
	}
}

/**
 * WriteUnformattedLine
 *
 * Writes a new log entry.
 *
 * @param Line the log entry
 */
void CLog::WriteUnformattedLine(const char *Line) {
	char *Out = NULL, *dupLine;
	size_t StringLength;
	unsigned int a;
	tm Now;
	char strNow[100];
	FILE *LogFile;
#ifndef _WIN32
	struct stat StatBuf;
#endif
	int rc;

	if (Line == NULL) {
		return;
	}

	LogFile = m_File;

#ifndef _WIN32
	if (m_Filename != NULL) {
		rc = lstat(m_Filename, &StatBuf);

		if (m_File != NULL && (rc < 0 || StatBuf.st_ino != m_Inode || StatBuf.st_dev != m_Dev)) {
			fclose(m_File);
			m_File = NULL;
		}

		if (rc == 0) {
			m_Inode = StatBuf.st_ino;
			m_Dev = StatBuf.st_dev;
		}
	}
#endif

	if (m_Filename == NULL || (m_File == NULL && (LogFile = fopen(m_Filename, "a")) == NULL)) {
		return;
	}

	SetPermissions(m_Filename, S_IRUSR | S_IWUSR);

	if (m_KeepOpen) {
		m_File = LogFile;
	}

	Now = *localtime(&g_CurrentTime);

#ifdef _WIN32
	strftime(strNow, sizeof(strNow), "%#c" , &Now);
#else
	strftime(strNow, sizeof(strNow), "%a %B %d %Y %H:%M:%S" , &Now);
#endif

	dupLine = strdup(Line);

	if (AllocFailed(dupLine)) {
		if (!m_KeepOpen)
			fclose(LogFile);

		return;
	}

	StringLength = strlen(dupLine);

	a = 0;

	for (unsigned int i = 0; i <= StringLength; i++) {
		if (dupLine[i] == '\r' || dupLine[i] == '\n') {
			continue;
		}

		dupLine[a] = dupLine[i];
		a++;
	}

	rc = asprintf(&Out, "[%s]: %s\n", strNow, dupLine);

	free(dupLine);

	if (rc < 0) {
		perror("asprintf() failed");

		if (!m_KeepOpen)
			fclose(LogFile);

		return;
	}

	fputs(Out, LogFile);
	printf("%s", Out);

	free(Out);

	if (!m_KeepOpen) {
		fclose(LogFile);
	} else {
		fflush(m_File);
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
void CLog::WriteLine(const char *Format, ...) {
	char *Out;
	va_list marker;

	va_start(marker, Format);
	int rc = vasprintf(&Out, Format, marker);
	va_end(marker);

	if (rc < 0) {
		perror("vasprintf() failed");

		return;
	}

	WriteUnformattedLine(Out);

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
