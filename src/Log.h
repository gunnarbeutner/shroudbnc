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

class CUser;

#define Log_Notice 1
#define Log_Message 0
#define Log_Motd 2

/**
 * CLog
 *
 * A log file.
 */
class CLog : public CZoneObject<CLog, 16> {
	char *m_Filename; /**< the filename of the log, can be an empty string */

	void WriteUnformattedLine(const char *Timestamp, const char *Line);
public:
#ifndef SWIG
	CLog(const char *Filename);
	~CLog(void);
#endif

	virtual void Clear(void);
	virtual void WriteLine(const char *Timestamp, const char *Format,...);
	virtual void PlayToUser(CUser *User, int Type);
	virtual bool IsEmpty(void);
	virtual const char *GetFilename(void);
};
