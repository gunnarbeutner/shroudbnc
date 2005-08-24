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

void DestroyCChannel(CChannel* P) {
	delete P;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CChannel::CChannel(const char* Name, CIRCConnection* Owner) {
	m_Name = strdup(Name);
	m_Modes = NULL;
	m_ModeCount = 0;
	m_Owner = Owner;
	m_Creation = 0;
	m_Topic = NULL;
	m_TopicNick = NULL;
	m_TopicStamp = 0;
	m_HasTopic = 0;
	m_Nicks = new CHashtable<CNick*, false, 64, true>();
	m_Nicks->RegisterValueDestructor(DestroyCNick);
	m_HasNames = false;
	m_ModesValid = false;
	m_Banlist = new CBanlist();
	m_HasBans = false;
}

CChannel::~CChannel() {
	free(m_Name);

	free(m_Topic);
	free(m_TopicNick);

	delete m_Nicks;

	for (int i = 0; i < m_ModeCount; i++) {
		if (m_Modes[i].Mode != '\0')
			free(m_Modes[i].Parameter);
	}

	free(m_Modes);

	delete m_Banlist;
}

const char* CChannel::GetName(void) {
	return m_Name;
}

const char* CChannel::GetChanModes(void) {
	int i;

	strcpy(m_TempModes, "+");

	for (i = 0; i < m_ModeCount; i++) {
		int ModeType = m_Owner->RequiresParameter(m_Modes[i].Mode);

		if (m_Modes[i].Mode != '\0' && ModeType != 3) {
			char m[2];
			m[0] = m_Modes[i].Mode;
			m[1] = '\0';

			strcat(m_TempModes, m);
		}
	}

	for (i = 0; i < m_ModeCount; i++) {
		int ModeType = m_Owner->RequiresParameter(m_Modes[i].Mode);

		if (m_Modes[i].Mode != '\0' && m_Modes[i].Parameter && ModeType != 3) {
			strcat(m_TempModes, " ");
			strcat(m_TempModes, m_Modes[i].Parameter);
		}
	}

	return m_TempModes;
}

void CChannel::ParseModeChange(const char* source, const char* modes, int pargc, const char** pargv) {
	bool flip = true;
	int p = 0;

	CModule** Modules = g_Bouncer->GetModules();
	int Count = g_Bouncer->GetModuleCount();

	for (unsigned int i = 0; i < strlen(modes); i++) {
		char Cur = modes[i];

		if (Cur == '+') {
			flip = true;
			continue;
		} else if (Cur == '-') {
			flip = false;
			continue;
		} else if (m_Owner->IsNickMode(Cur)) {
			if (p >= pargc)
				return; // should not happen

			CNick* P = m_Nicks->Get(pargv[p]);

			if (P) {
				if (flip)
					P->AddPrefix(m_Owner->PrefixForChanMode(Cur));
				else
					P->RemovePrefix(m_Owner->PrefixForChanMode(Cur));
			}

			for (int i = 0; i < Count; i++) {
				if (Modules[i]) {
					Modules[i]->SingleModeChange(m_Owner, m_Name, source, flip, Cur, pargv[p]);
				}
			}

			if (flip && Cur == 'o' && strcmpi(pargv[p], m_Owner->GetCurrentNick()) == 0) {
				SetModesValid(false);

				if (!m_Owner->GetOwningClient()->GetClientConnection())
					m_Owner->WriteLine("MODE %s", m_Name);
			}

			p++;

			continue;
		}

		chanmode_t* Slot = FindSlot(Cur);

		int ModeType = m_Owner->RequiresParameter(Cur);

		if (Cur == 'b') {
			if (flip)
				m_Banlist->SetBan(pargv[p], source, time(NULL));
			else
				m_Banlist->UnsetBan(pargv[p]);
		}

		if (Cur == 'k' && flip)
			m_Owner->GetOwningClient()->GetKeyring()->AddKey(m_Name, pargv[p]);

		for (int i = 0; i < Count; i++) {
			if (Modules[i]) {
				Modules[i]->SingleModeChange(m_Owner, m_Name, source, flip, Cur, ((flip && ModeType) || (!flip && ModeType && ModeType != 1)) ? pargv[p] : NULL);
			}
		}

		if (flip) {
			if (Slot)
				free(Slot->Parameter);
			else
				Slot = AllocSlot();

			Slot->Mode = Cur;
			
			if (ModeType)
				Slot->Parameter = strdup(pargv[p++]);
			else
				Slot->Parameter = NULL;
		} else {
			if (Slot) {
				Slot->Mode = '\0';
				free(Slot->Parameter);

				Slot->Parameter = NULL;
			}

			if (ModeType && ModeType != 1)
				p++;
		}
	}
}

chanmode_t* CChannel::AllocSlot(void) {
	for (int i = 0; i < m_ModeCount; i++) {
		if (m_Modes[i].Mode == '\0')
			return &m_Modes[i];
	}

	m_Modes = (chanmode_t*)realloc(m_Modes, sizeof(chanmode_t) * ++m_ModeCount);
	m_Modes[m_ModeCount - 1].Parameter = NULL;
	return &m_Modes[m_ModeCount - 1];
}

chanmode_t* CChannel::FindSlot(char Mode) {
	for (int i = 0; i < m_ModeCount; i++) {
		if (m_Modes[i].Mode == Mode)
			return &m_Modes[i];
	}

	return NULL;
}

time_t CChannel::GetCreationTime(void) {
	return m_Creation;
}

void CChannel::SetCreationTime(time_t T) {
	m_Creation = T;
}

const char* CChannel::GetTopic(void) {
	return m_Topic;
}

void CChannel::SetTopic(const char* Topic) {
	free(m_Topic);
	m_Topic = strdup(Topic);
	m_HasTopic = 1;
}

const char* CChannel::GetTopicNick(void) {
	return m_TopicNick;
}

void CChannel::SetTopicNick(const char* Nick) {
	free(m_TopicNick);
	m_TopicNick = strdup(Nick);
	m_HasTopic = 1;
}

time_t CChannel::GetTopicStamp(void) {
	return m_TopicStamp;
}

void CChannel::SetTopicStamp(time_t TS) {
	m_TopicStamp = TS;
	m_HasTopic = 1;
}

int CChannel::HasTopic(void) {
	return m_HasTopic;
}

void CChannel::SetNoTopic(void) {
	m_HasTopic = -1;
}

void CChannel::AddUser(const char* Nick, const char* ModeChars) {
	CNick* N = new CNick(Nick);

	N->SetPrefixes(ModeChars);

	m_Nicks->Add(N->GetNick(), N);
}

void CChannel::RemoveUser(const char* Nick) {
	m_Nicks->Remove(Nick);
}

char CChannel::GetHighestUserFlag(const char* ModeChars) {
	bool flip = false;
	const char* Prefixes = m_Owner->GetISupport("PREFIX");

	if (!ModeChars)
		return '\0';

	for (unsigned int i = 0; i < strlen(Prefixes); i++) {
		if (!flip) {
			if (Prefixes[i] == ')')
				flip = true;

			continue;
		}

		if (strchr(ModeChars, Prefixes[i]))
			return Prefixes[i];
	}

	return '\0';
}

bool CChannel::HasNames(void) {
	return m_HasNames;
}

void CChannel::SetHasNames(void) {
	m_HasNames = true;
}

CHashtable<CNick*, false, 64, true>* CChannel::GetNames(void) {
	return m_Nicks;
}

void CChannel::ClearModes(void) {
	for (int i = 0; i < m_ModeCount; i++) {
		if (m_Modes[i].Mode != '\0') {
			int ModeType = m_Owner->RequiresParameter(m_Modes[i].Mode);

			if (ModeType != 3) {
				free(m_Modes[i].Parameter);

				m_Modes[i].Mode = '\0';
				m_Modes[i].Parameter = NULL;
			}
		}
	}
}


bool CChannel::AreModesValid(void) {
	return m_ModesValid;
}

void CChannel::SetModesValid(bool Valid) {
	m_ModesValid = Valid;
}

CBanlist* CChannel::GetBanlist(void) {
	return m_Banlist;
}

void CChannel::SetHasBans(void) {
	m_HasBans = true;
}

bool CChannel::HasBans(void) {
	return m_HasBans;
}
