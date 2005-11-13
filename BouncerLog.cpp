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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBouncerLog::CBouncerLog(const char* Filename) {
	if (Filename) {
		m_File = strdup(Filename);

		if (m_File == NULL) {
			if (g_Bouncer) {
				LOGERROR("strdup() failed.");

				g_Bouncer->Fatal();
			} else {
				printf("CBouncerLog::CBouncerLog: strdup() failed (%s).", Filename);

				exit(1);
			}
		}
	} else
		m_File = NULL;
}

CBouncerLog::~CBouncerLog() {
	free(m_File);
}

void CBouncerLog::PlayToUser(CBouncerUser* User, bool NoticeUser) {
	FILE* Log;

	if (m_File && (Log = fopen(m_File, "r"))) {
		char Line[500];
		while (!feof(Log)) {
			char* n = fgets(Line, sizeof(Line), Log);

			if (n) {
				if (NoticeUser)
					User->RealNotice(Line);
				else
					User->Notice(Line);
			}
		}

		fclose(Log);
	}
}

void CBouncerLog::InternalWriteLine(const char* Line) {
	FILE* Log;

	if (m_File && (Log = fopen(m_File, "a"))) {
		char* Out;
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

void CBouncerLog::WriteLine(const char* Format, ...) {
	char* Out;
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

void CBouncerLog::Clear(void) {
	FILE* Log;
	
	if (m_File && (Log = fopen(m_File, "w")))
		fclose(Log);
}

bool CBouncerLog::IsEmpty(void) {
	FILE* Log;

	if (m_File && (Log = fopen(m_File, "r"))) {
		char Line[500];

		while (!feof(Log)) {
			char* n = fgets(Line, sizeof(Line), Log);

			if (n) {
				fclose(Log);

				return false;
			}
				
		}

		fclose(Log);
	}

	return true;
}
