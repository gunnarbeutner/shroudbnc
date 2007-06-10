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

typedef struct logline_s {
	unsigned int Timestamp;
	char *Type;
	char *Source;
	char *Text;
	
	logline_s *Previous;
	logline_s *Next;
} logline_t;

typedef enum playtype_e {
	Play_Privmsg,
	Play_Evt
} playtype_t;

class CLogBuffer {
private:
	logline_t *m_LogBegin;
	logline_t *m_LogEnd;
	unsigned int m_Count;
	unsigned int m_MaxCount;

	void Clear(void);
public:
	CLogBuffer(unsigned int MaxCount = 30);
	~CLogBuffer(void);

	bool LogEvent(const char *Type, const char *Source, const char *Text);
	bool PlayLog(CClientConnection *Client, const char *Channel, playtype_t Type);
};
