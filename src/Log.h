/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2011 Gunnar Beutner                                      *
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

#ifndef LOG_H
#define LOG_H

/**
 * LogType
 *
 * The type of the message.
 */
typedef enum {
	Log_Message,
	Log_Notice,
	Log_Motd,
} LogType;

/**
 * CLog
 *
 * A log file.
 */
class SBNCAPI CLog {
	char *m_Filename; /**< the filename of the log, can be an empty string */
	bool m_KeepOpen; /**< should we keep the file open? */
	mutable FILE *m_File; /**< the file */
#ifndef _WIN32
	ino_t m_Inode;
	dev_t m_Dev;
#endif
public:
#ifndef SWIG
	CLog(const char *Filename, bool KeepOpen = false);
	virtual ~CLog(void);
#endif /* SWIG */

	void Clear(void);
	void WriteLine(const char *Format,...);
	void WriteUnformattedLine(const char *Line);
	void PlayToUser(CClientConnection *Client, LogType Type) const;
	bool IsEmpty(void) const;
	const char *GetFilename(void) const;
};

#endif /* LOG_H */
