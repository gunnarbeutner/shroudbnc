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
#include "BouncerLog.h"
#include "BouncerUser.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBouncerLog::CBouncerLog(const char* Filename) {
	m_File = strdup(Filename);
}

CBouncerLog::~CBouncerLog() {
	free(m_File);
}

void CBouncerLog::PlayToUser(CBouncerUser* User) {
	FILE* Log = fopen(m_File, "r");

	if (Log) {
		char Line[500];
		while (!feof(Log)) {
			char* n = fgets(Line, sizeof(Line), Log);

			if (n)
				User->Notice(Line);
		}

		fclose(Log);
	}

	User->Notice("End of LOG.");
}

void CBouncerLog::InternalWriteLine(const char* Line) {
	char Out[1024];
	FILE* Log = fopen(m_File, "a");

	if (Log) {
		tm Now;
		time_t tNow;
		char strNow[100];

		time(&tNow);
		Now = *localtime(&tNow);

		strftime(strNow, sizeof(strNow), "%#c" , &Now);
		snprintf(Out, sizeof(Out), "%s %s\n", strNow, Line);

		fputs(Out, Log);

		fclose(Log);
	}
}

void CBouncerLog::WriteLine(const char* Format, ...) {
	char Out[1024];
	va_list marker;

	va_start(marker, Format);
	vsnprintf(Out, sizeof(Out), Format, marker);
	va_end(marker);

	InternalWriteLine(Out);
}

void CBouncerLog::Clear(void) {
	FILE* Log = fopen(m_File, "w");
	
	if (Log)
		fclose(Log);
}

bool CBouncerLog::IsEmpty(void) {
	FILE* Log = fopen(m_File, "r");

	if (Log) {
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
