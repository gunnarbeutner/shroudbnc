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

CChannel::CChannel(const char *Name, CIRCConnection *Owner) {
	SetOwner(Owner);

	m_Name = strdup(Name);
	CHECK_ALLOC_RESULT(m_Name, strdup) { } CHECK_ALLOC_RESULT_END;

	m_Modes = NULL;
	m_ModeCount = 0;
	m_Creation = 0;
	m_Topic = NULL;
	m_TopicNick = NULL;
	m_TopicStamp = 0;
	m_HasTopic = 0;

	m_Nicks.RegisterValueDestructor(DestroyObject<CNick>);

	m_HasNames = false;
	m_ModesValid = false;

	m_HasBans = false;
	m_TempModes = NULL;

	m_Banlist = new CBanlist();
}

CChannel::~CChannel() {
	free(m_Name);

	free(m_Topic);
	free(m_TopicNick);
	free(m_TempModes);

	for (int i = 0; i < m_ModeCount; i++) {
		if (m_Modes[i].Mode != '\0') {
			free(m_Modes[i].Parameter);
		}
	}

	free(m_Modes);

	delete m_Banlist;
}

const char *CChannel::GetName(void) const {
	return m_Name;
}

RESULT(const char *) CChannel::GetChannelModes(void) {
	int i;
	size_t Size;
	char *NewTempModes;
	char ModeString[2];
	int ModeType;

	if (m_TempModes != NULL) {
		RETURN(const char *, m_TempModes);
	}

	Size = m_ModeCount + 1024;
	m_TempModes = (char *)malloc(Size);

	CHECK_ALLOC_RESULT(m_TempModes, malloc) {
		THROW(const char *, Generic_OutOfMemory, "malloc() failed.");
	} CHECK_ALLOC_RESULT_END;

	strcpy(m_TempModes, "+");

	for (i = 0; i < m_ModeCount; i++) {
		ModeType = m_Owner->RequiresParameter(m_Modes[i].Mode);

		if (m_Modes[i].Mode != '\0' && ModeType != 3) {
			ModeString[0] = m_Modes[i].Mode;
			ModeString[1] = '\0';

			strcat(m_TempModes, ModeString);
		}
	}

	for (i = 0; i < m_ModeCount; i++) {
		int ModeType = m_Owner->RequiresParameter(m_Modes[i].Mode);

		if (m_Modes[i].Mode != '\0' && m_Modes[i].Parameter && ModeType != 3) {
			strcat(m_TempModes, " ");

			if (strlen(m_TempModes) + strlen(m_Modes[i].Parameter) > Size) {
				Size += strlen(m_Modes[i].Parameter) + 1024;
				NewTempModes = (char*)realloc(m_TempModes, Size);

				CHECK_ALLOC_RESULT(m_TempModes, malloc) {
					free(m_TempModes);

					THROW(const char *, Generic_OutOfMemory, "malloc() failed.");
				} CHECK_ALLOC_RESULT_END;

				m_TempModes = NewTempModes;
			}

			strcat(m_TempModes, m_Modes[i].Parameter);
		}
	}

	RETURN(const char *, m_TempModes);
}

void CChannel::ParseModeChange(const char *Source, const char *Modes, int pargc, const char **pargv) {
	bool Flip = true;
	int p = 0;

	/* free any cached chanmodes */
	if (m_TempModes != NULL) {
		free(m_TempModes);
		m_TempModes = NULL;
	}

	CVector<CModule *> *Modules = g_Bouncer->GetModules();

	for (unsigned int i = 0; i < strlen(Modes); i++) {
		char Current = Modes[i];

		if (Current == '+') {
			Flip = true;
			continue;
		} else if (Current == '-') {
			Flip = false;
			continue;
		} else if (m_Owner->IsNickMode(Current)) {
			if (p >= pargc) {
				return; // should not happen
			}

			CNick *NickObj = m_Nicks.Get(pargv[p]);

			if (NickObj != NULL) {
				if (Flip) {
					NickObj->AddPrefix(m_Owner->PrefixForChanMode(Current));
				} else {
					NickObj->RemovePrefix(m_Owner->PrefixForChanMode(Current));
				}
			}

			for (unsigned int i = 0; i < Modules->GetLength(); i++) {
				Modules->Get(i)->SingleModeChange(m_Owner, m_Name, Source, Flip, Current, pargv[p]);
			}

			if (Flip && Current == 'o' && strcasecmp(pargv[p], m_Owner->GetCurrentNick()) == 0) {
				// invalidate channel modes so we can get channel-modes which require +o (e.g. +k)
				SetModesValid(false);

				// update modes immediatelly if we don't have a client
				if (m_Owner->GetOwner()->GetClientConnection() == NULL) {
					m_Owner->WriteLine("MODE %s", m_Name);
				}
			}

			p++;

			continue;
		}

		chanmode_t *Slot = FindSlot(Current);

		int ModeType = m_Owner->RequiresParameter(Current);

		if (Current == 'b' && m_Banlist != NULL) {
			if (Flip) {
				m_Banlist->SetBan(pargv[p], Source, time(NULL));
			} else {
				m_Banlist->UnsetBan(pargv[p]);
			}
		}

		if (Current == 'k' && Flip && strcmp(pargv[p], "*") != 0) {
			m_Owner->GetOwner()->GetKeyring()->AddKey(m_Name, pargv[p]);
		}

		for (unsigned int i = 0; i < Modules->GetLength(); i++) {
			Modules->Get(i)->SingleModeChange(m_Owner, m_Name, Source, Flip, Current, ((Flip && ModeType != 0) || (!Flip && ModeType != 0 && ModeType != 1)) ? pargv[p] : NULL);
		}

		if (Flip) {
			if (Slot != NULL) {
				free(Slot->Parameter);
			} else {
				Slot = AllocSlot();
			}

			if (Slot == NULL) {
				if (ModeType) {
					p++;
				}

				continue;
			}

			Slot->Mode = Current;
			
			if (ModeType != 0) {
				Slot->Parameter = strdup(pargv[p++]);
			} else {
				Slot->Parameter = NULL;
			}
		} else {
			if (Slot) {
				Slot->Mode = '\0';
				free(Slot->Parameter);

				Slot->Parameter = NULL;
			}

			if (ModeType != 0 && ModeType != 1) {
				p++;
			}
		}
	}
}

chanmode_t *CChannel::AllocSlot(void) {
	chanmode_t *Modes;

	for (int i = 0; i < m_ModeCount; i++) {
		if (m_Modes[i].Mode == '\0') {
			return &m_Modes[i];
		}
	}

	Modes = (chanmode_t *)realloc(m_Modes, sizeof(chanmode_t) * ++m_ModeCount);

	CHECK_ALLOC_RESULT(Modes, realloc) {
		return NULL;
	} CHECK_ALLOC_RESULT_END;

	m_Modes = Modes;
	m_Modes[m_ModeCount - 1].Parameter = NULL;
	return &m_Modes[m_ModeCount - 1];
}

chanmode_t *CChannel::FindSlot(char Mode) {
	for (int i = 0; i < m_ModeCount; i++) {
		if (m_Modes[i].Mode == Mode) {
			return &m_Modes[i];
		}
	}

	return NULL;
}

time_t CChannel::GetCreationTime(void) const {
	return m_Creation;
}

void CChannel::SetCreationTime(time_t Time) {
	m_Creation = Time;
}

const char *CChannel::GetTopic(void) const {
	return m_Topic;
}

void CChannel::SetTopic(const char *Topic) {
	char *NewTopic;

	NewTopic = strdup(Topic);

	CHECK_ALLOC_RESULT(NewTopic, strdup) {
		return;
	} CHECK_ALLOC_RESULT_END;

	free(m_Topic);
	m_Topic = NewTopic;
	m_HasTopic = 1;
}

const char *CChannel::GetTopicNick(void) const {
	return m_TopicNick;
}

void CChannel::SetTopicNick(const char *Nick) {
	char *NewTopicNick;

	NewTopicNick = strdup(Nick);

	CHECK_ALLOC_RESULT(NewTopicNick, strdup) {
		return;
	} CHECK_ALLOC_RESULT_END;

	free(m_TopicNick);
	m_TopicNick = NewTopicNick;
	m_HasTopic = 1;
}

time_t CChannel::GetTopicStamp(void) const {
	return m_TopicStamp;
}

void CChannel::SetTopicStamp(time_t Timestamp) {
	m_TopicStamp = Timestamp;
	m_HasTopic = 1;
}

int CChannel::HasTopic(void) const {
	return m_HasTopic;
}

void CChannel::SetNoTopic(void) {
	m_HasTopic = -1;
}

void CChannel::AddUser(const char *Nick, const char *ModeChars) {
	CNick *NickObj;

	NickObj = new CNick(this, Nick);

	CHECK_ALLOC_RESULT(NickObj, CZone::Allocate) {
		return;
	} CHECK_ALLOC_RESULT_END;

	NickObj->SetPrefixes(ModeChars);

	m_Nicks.Add(Nick, NickObj);
}

void CChannel::RemoveUser(const char *Nick) {
	m_Nicks.Remove(Nick);
}

void CChannel::RenameUser(const char *Nick, const char *NewNick) {
	CNick *NickObj;

	NickObj = m_Nicks.Get(Nick);

	if (NickObj == NULL) {
		return;
	}

	m_Nicks.Remove(Nick, true);
	NickObj->SetNick(NewNick);
	m_Nicks.Add(Nick, NickObj);
}

bool CChannel::HasNames(void) const {
	return m_HasNames;
}

void CChannel::SetHasNames(void) {
	m_HasNames = true;
}

const CHashtable<CNick *, false, 64> *CChannel::GetNames(void) const {
	return &m_Nicks;
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

bool CChannel::AreModesValid(void) const {
	return m_ModesValid;
}

void CChannel::SetModesValid(bool Valid) {
	m_ModesValid = Valid;
}

CBanlist *CChannel::GetBanlist(void) {
	return m_Banlist;
}

void CChannel::SetHasBans(void) {
	m_HasBans = true;
}

bool CChannel::HasBans(void) const {
	return m_HasBans;
}

bool CChannel::SendWhoReply(bool Simulate) const {
	char CopyIdent[50];
	char *Ident, *Host, *Site;
	const char *Server, *Realname, *SiteTemp;
	CClientConnection *Client = m_Owner->GetOwner()->GetClientConnection();

	if (Client == NULL) {
		return true;
	}

	if (!HasNames()) {
		return false;
	}

	int a = 0;

	while (hash_t<CNick *> *NickHash = GetNames()->Iterate(a++)) {
		CNick *NickObj = NickHash->Value;

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

RESULT(bool) CChannel::Freeze(CAssocArray *Box) {
	CAssocArray *Nicks;
	CNick *Nick;
	unsigned int Count, i = 0;
	char *Index;
	RESULT(bool) Result;

	// TODO: persist channel modes
	Box->AddString("~channel.name", m_Name);
	Box->AddInteger("~channel.ts", m_Creation);
	Box->AddString("~channel.topic", m_Topic);
	Box->AddString("~channel.topicnick", m_TopicNick);
	Box->AddInteger("~channel.topicts", m_TopicStamp);
	Box->AddInteger("~channel.hastopic", m_HasTopic);

	Result = FreezeObject<CBanlist>(Box, "~channel.banlist", m_Banlist);
	THROWIFERROR(bool, Result);

	m_Banlist = NULL;

	Nicks = Box->Create();

	Count = m_Nicks.GetLength();

	for (i = 0; i < Count; i++) {
		Nick = m_Nicks.Iterate(i)->Value;

		asprintf(&Index, "%d", i);
		CHECK_ALLOC_RESULT(Index, asprintf) {
			THROW(bool, Generic_OutOfMemory, "asprintf() failed.");
		} CHECK_ALLOC_RESULT_END;
		Result = FreezeObject<CNick>(Nicks, Index, Nick);
		THROWIFERROR(bool, Result);
		free(Index);
	}

	m_Nicks.RegisterValueDestructor(NULL);

	Box->AddBox("~channel.nicks", Nicks);

	delete this;

	RETURN(bool, true);
}

RESULT(CChannel *) CChannel::Thaw(CAssocArray *Box) {
	CAssocArray *NicksBox;
	RESULT(CBanlist *) Banlist;
	CChannel *Channel;
	const char *Name, *Temp;
	unsigned int i = 0;
	char *Index;

	if (Box == NULL) {
		THROW(CChannel *, Generic_InvalidArgument, "Box cannot be NULL.");
	}

	Name = Box->ReadString("~channel.name");

	if (Name == NULL) {
		THROW(CChannel *, Generic_Unknown, "Persistent data is invalid: Missing channelname.");
	}

	Channel = new CChannel(Name, NULL);

	Channel->m_Creation = Box->ReadInteger("~channel.ts");

	Temp = Box->ReadString("~channel.topic");

	if (Temp != NULL) {
		Channel->m_Topic = strdup(Temp);
	}

	Temp = Box->ReadString("~channel.topicnick");

	if (Temp != NULL) {
		Channel->m_TopicNick = strdup(Temp);
	}

	Channel->m_TopicStamp = Box->ReadInteger("~channel.topicts");

	Channel->m_HasTopic = Box->ReadInteger("~channel.hastopic");

	Banlist = ThawObject<CBanlist>(Box, "~channel.banlist");

	if (!IsError(Banlist)) {
		delete Channel->m_Banlist;

		Channel->m_Banlist = Banlist;
	}

	NicksBox = Box->ReadBox("~channel.nicks");

	while (true) {
		asprintf(&Index, "%d", i);

		CHECK_ALLOC_RESULT(Index, asprintf) {
			THROW(CChannel *, Generic_OutOfMemory, "asprintf() failed.");
		} CHECK_ALLOC_RESULT_END;

		CNick *Nick = ThawObject<CNick>(NicksBox, Index);

		if (Nick == NULL) {
			break;
		}

		Nick->SetOwner(Channel);

		Channel->m_Nicks.Add(Nick->GetNick(), Nick);

		free(Index);

		i++;
	}

	RETURN(CChannel *, Channel);
}
