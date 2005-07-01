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

#if !defined(AFX_CHANNEL_H__C495C5C9_34AE_49AB_8C67_B8D697AF0651__INCLUDED_)
#define AFX_CHANNEL_H__C495C5C9_34AE_49AB_8C67_B8D697AF0651__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CIRCConnection;

typedef struct chanmode_s {
	char Mode;
	char* Parameter;
} chanmode_t;

class CNick;
template <typename value_type, bool casesensitive> class CHashtable;

class CChannel {
	char* m_Name;

	chanmode_t* m_Modes;
	int m_ModeCount;
	char m_TempModes[1024];

	char* m_Topic;
	char* m_TopicNick;
	time_t m_TopicStamp;
	int m_HasTopic;
	bool m_ModesValid;

	time_t m_Creation;

	CHashtable<CNick*, false>* m_Nicks;

	bool m_HasNames;

	CIRCConnection* m_Owner;

	chanmode_t* AllocSlot(void);
	chanmode_t* FindSlot(char Mode);
public:
	CChannel(const char* Name, CIRCConnection* Owner);
	virtual ~CChannel();

	virtual const char* GetName(void);

	virtual const char* GetChanModes(void);
	virtual void ParseModeChange(const char* source, const char* modes, int pargc, const char** pargv);

	virtual time_t GetCreationTime(void);
	virtual void SetCreationTime(time_t T);

	virtual const char* GetTopic(void);
	virtual void SetTopic(const char* Topic);

	virtual const char* GetTopicNick(void);
	virtual void SetTopicNick(const char* Nick);

	virtual time_t GetTopicStamp(void);
	virtual void SetTopicStamp(time_t TS);

	virtual int HasTopic(void);
	virtual void SetNoTopic(void);

	virtual void AddUser(const char* Nick, const char* ModeChar);
	virtual void RemoveUser(const char* Nick);
	virtual char GetHighestUserFlag(const char* ModeChars);

	virtual bool HasNames(void);
	virtual void SetHasNames(void);
	virtual CHashtable<CNick*, false>* GetNames(void);

	virtual void ClearModes(void);
	virtual bool AreModesValid(void);
	virtual void SetModesValid(bool Valid);
};

#endif // !defined(AFX_CHANNEL_H__C495C5C9_34AE_49AB_8C67_B8D697AF0651__INCLUDED_)
