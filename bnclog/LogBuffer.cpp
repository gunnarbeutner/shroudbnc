/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

#include "../src/StdAfx.h"
#include "LogBuffer.h"

CLogBuffer::CLogBuffer(unsigned int MaxCount) {
	m_LogBegin = NULL;
	m_LogEnd = NULL;
	m_Count = 0;
	m_MaxCount = MaxCount;
}

CLogBuffer::~CLogBuffer(void) {
	Clear();
}

void CLogBuffer::Clear(void) {
	logline_t *Line = m_LogBegin;
	logline_t *Begin = m_LogBegin;

	// Remove lines from the buffer
	while (Line != NULL) {
		free(Line->Source);
		free(Line->Text);

		Line = Line->Next;

		if (Line == Begin) {
			break; // we are at the beginning again
		}
	}

	m_LogBegin = NULL;
	m_LogEnd = NULL;
}

bool CLogBuffer::LogEvent(const char *Type, const char *Source, const char *Text) {
	logline_t *Line;

	if (m_Count >= m_MaxCount) {
		Line = m_LogBegin;
		m_LogBegin = m_LogBegin->Next;
		free(Line->Source);
		free(Line->Text);
	} else {
		Line = (logline_t *)malloc(sizeof(logline_t));

		CHECK_ALLOC_RESULT(Line, malloc) {
			return false;
		} CHECK_ALLOC_RESULT_END;

		m_Count++;

		Line->Previous = m_LogEnd;

		if (m_LogEnd != NULL) {
			m_LogEnd->Next = Line;
		}

		if (m_Count != m_MaxCount) {
			Line->Next = NULL;
		} else {
			Line->Next = m_LogBegin;
		}

		if (m_LogBegin == NULL) {
			m_LogBegin = Line;
		}

		m_LogEnd = Line;
	}

	Line->Timestamp = time(NULL);
	Line->Type = strdup(Type);
	Line->Source = strdup(Source);
	Line->Text = Text != NULL ? strdup(Text) : NULL;

	return true;
}

bool CLogBuffer::PlayLog(CClientConnection *Client, const char *Channel, playtype_t Type) {
	CUser *Owner;
	CIRCConnection *IRC;
	const char *Server = "bouncer";
	logline_t *Line = m_LogBegin;
	logline_t *Begin = m_LogBegin;

	Owner = Client->GetOwner();

	if (Owner != NULL) {
		IRC = Owner->GetIRCConnection();

		if (IRC != NULL) {
			Server = IRC->GetServer();
		}
	}

	while (Line != NULL) {
		if (Type == Play_Evt) {
			if (Line->Text != NULL) {
				Client->WriteLine(":%s LOGEVT %d %s %s %s :%s", Server, Line->Timestamp, Line->Source, Line->Type, Channel, Line->Text);
			} else {
				Client->WriteLine(":%s LOGEVT %d %s %s %s", Server, Line->Timestamp, Line->Source, Line->Type, Channel);
			}
		} else if (Type == Play_Privmsg) {
			const char *Timestamp;

			Timestamp = Owner->FormatTime(Line->Timestamp, "%d.%m. %H:%M:%S");

			if (Line->Text != NULL) {
				Client->WriteLine(":%s PRIVMSG %s :(%s, %s) %s", Line->Source, Channel, Timestamp, Line->Type, Line->Text);
			} else {
				Client->WriteLine(":%s PRIVMSG %s :(%s, %s)", Line->Source, Channel, Timestamp, Line->Type);
			}
		}

		Line = Line->Next;

		if (Line == Begin) {
			break; // we are at the beginning again
		}
	}

	return true;
}
