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
	m_Nicks = new CHashtable<CNick*, false, 64>();

	if (m_Nicks == NULL) {
		LOGERROR("new operator failed. Could not create nick list.");

		Owner->Kill("Internal error.");
	} else {
		m_Nicks->RegisterValueDestructor(DestroyObject<CNick>);
	}

	m_HasNames = false;
	m_ModesValid = false;
	m_Banlist = new CBanlist();

	if (m_Banlist == NULL) {
		LOGERROR("new operator failed. Could not create ban list.");
	}

	m_HasBans = false;
	m_TempModes = NULL;
}

CChannel::~CChannel() {
	free(m_Name);

	free(m_Topic);
	free(m_TopicNick);
	free(m_TempModes);

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
	size_t Size;

	if (m_TempModes)
		return m_TempModes;

	Size = 1024;
	m_TempModes = (char*)malloc(Size);

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

			if (strlen(m_TempModes) + strlen(m_Modes[i].Parameter) > Size) {
				Size += 1024;
				m_TempModes = (char*)realloc(m_TempModes, Size);

				if (m_TempModes == NULL)
					return "";
			}

			strcat(m_TempModes, m_Modes[i].Parameter);
		}
	}

	return m_TempModes;
}

void CChannel::ParseModeChange(const char* source, const char* modes, int pargc, const char** pargv) {
	bool flip = true;
	int p = 0;

	/* free any cached chanmodes */
	if (m_TempModes) {
		free(m_TempModes);
		m_TempModes = NULL;
	}

	CVector<CModule *> *Modules = g_Bouncer->GetModules();

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

			for (int i = 0; i < Modules->Count(); i++) {
				Modules->Get(i)->SingleModeChange(m_Owner, m_Name, source, flip, Cur, pargv[p]);
			}

			if (flip && Cur == 'o' && strcasecmp(pargv[p], m_Owner->GetCurrentNick()) == 0) {
				SetModesValid(false);

				if (!m_Owner->GetOwningClient()->GetClientConnection())
					m_Owner->WriteLine("MODE %s", m_Name);
			}

			p++;

			continue;
		}

		chanmode_t* Slot = FindSlot(Cur);

		int ModeType = m_Owner->RequiresParameter(Cur);

		if (Cur == 'b' && m_Banlist) {
			if (flip)
				m_Banlist->SetBan(pargv[p], source, time(NULL));
			else
				m_Banlist->UnsetBan(pargv[p]);
		}

		if (Cur == 'k' && flip && strcmp(pargv[p], "*") != 0)
			m_Owner->GetOwningClient()->GetKeyring()->AddKey(m_Name, pargv[p]);

		for (int i = 0; i < Modules->Count(); i++) {
			Modules->Get(i)->SingleModeChange(m_Owner, m_Name, source, flip, Cur, ((flip && ModeType) || (!flip && ModeType && ModeType != 1)) ? pargv[p] : NULL);
		}

		if (flip) {
			if (Slot)
				free(Slot->Parameter);
			else
				Slot = AllocSlot();

			if (Slot == NULL) {
				if (ModeType)
					p++;

				continue;
			}

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
	chanmode_t* Modes;

	for (int i = 0; i < m_ModeCount; i++) {
		if (m_Modes[i].Mode == '\0')
			return &m_Modes[i];
	}

	Modes = (chanmode_t*)realloc(m_Modes, sizeof(chanmode_t) * ++m_ModeCount);

	if (Modes == NULL) {
		LOGERROR("realloc() failed. Could not allocate slot for channel mode.");

		return NULL;
	}

	m_Modes = Modes;
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
	CNick* NickObj;

	if (m_Nicks == NULL)
		return;

	NickObj = new CNick(this, Nick);

	if (NickObj == NULL) {
		LOGERROR("new operator failed. Could not add user (%s).", Nick);

		return;
	}

	NickObj->SetPrefixes(ModeChars);

	m_Nicks->Add(Nick, NickObj);
}

void CChannel::RemoveUser(const char* Nick) {
	if (m_Nicks)
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

CHashtable<CNick*, false, 64>* CChannel::GetNames(void) {
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

CIRCConnection *CChannel::GetOwner(void) {
	return m_Owner;
}

bool CChannel::SendWhoReply(bool Simulate) {
	char CopyIdent[50];
	char *Ident, *Host, *Site;
	const char *Server, *Realname, *SiteTemp;
	CClientConnection *Client = m_Owner->GetOwningClient()->GetClientConnection();

	if (Client == NULL)
		return true;

	if (!HasNames())
		return false;

	int a = 0;

	while (hash_t<CNick*>* NickHash = GetNames()->Iterate(a++)) {
		CNick* NickObj = NickHash->Value;

		if ((SiteTemp = NickObj->GetSite()) == NULL)
			return false;

		Site = const_cast<char*>(SiteTemp);
		Host = strchr(Site, '@');

		if (Host == NULL) {
			free(Site);

			return false;
		}

		strncpy(CopyIdent, Site, min(Host - Site - 1, sizeof(CopyIdent)));
		CopyIdent[Host - Site] = '\0';

		Ident = CopyIdent;

		Host++;

		Server = NickObj->GetServer();
		if (Server == NULL)
			Server = "*.unknown.org";

		Realname = NickObj->GetRealname();
		if (Realname == NULL)
			Realname = "3 Unknown Client";

		if (!Simulate) {
			Client->WriteLine(":%s 352 %s %s %s %s %s %s H :%s", m_Owner->GetServer(), m_Owner->GetCurrentNick(),
				m_Name, Ident, Host, Server, NickObj->GetNick(), Realname);
		}
	}

	if (!Simulate)
		Client->WriteLine(":%s 315 %s %s :End of /WHO list.", m_Owner->GetServer(), m_Owner->GetCurrentNick(), m_Name);

	return true;
}
