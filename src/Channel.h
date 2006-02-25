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

/**
 * chanmode_s
 *
 * A channel mode and its parameter.
 */
typedef struct chanmode_s {
	char Mode; /**< the channel mode */
	char *Parameter; /**< the associated parameter, or NULL if there is none */
} chanmode_t;

/* Forward declaration of some required classes */
class CNick;
class CBanlist;
class CIRCConnection;

#ifdef SWIGINTERFACE
%template(CHashtableCNick) CHashtable<class CNick *, false, 64>;
#endif

/**
 * CChannel
 *
 * Represents an IRC channel.
 */
class CChannel : public COwnedObject<CIRCConnection>, public CZoneObject<CChannel, 128> {
	char *m_Name; /**< the name of the channel */
	time_t m_Creation; /**< the time when the channel was created */

	chanmode_t *m_Modes; /**< the channel modes */
	int m_ModeCount; /**< the number of channel modes */
	bool m_ModesValid; /**< indicates whether the channelmodes are known */
	char *m_TempModes; /**< string-representation of the channel modes, used
							by GetChannelModes() */

	char *m_Topic; /**< the channel's topic */
	char *m_TopicNick; /**< the nick of the user who set the topic */
	time_t m_TopicStamp; /**< the time when the topic was set */
	int m_HasTopic; /**< indicates whether there is actually a topic */

	CHashtable<CNick *, false, 64> m_Nicks; /**< a list of nicks who are
												 on this channel */
	bool m_HasNames; /**< indicates whether m_Nicks is valid */

	CBanlist *m_Banlist; /**< a list of bans for this channel */
	bool m_HasBans; /**< indicates whether the banlist is known */

	chanmode_t *AllocSlot(void);
	chanmode_t *FindSlot(char Mode);
public:
#ifndef SWIG
	CChannel(const char *Name, CIRCConnection *Owner);
#endif
	virtual ~CChannel(void);

#ifndef SWIG
	RESULT(bool) Freeze(CAssocArray *Box);
	static RESULT(CChannel *) Thaw(CAssocArray *Box);
#endif

	virtual const char *GetName(void) const;

	virtual RESULT(const char *) GetChannelModes(void);
	virtual void ParseModeChange(const char *source, const char *modes, int pargc, const char **pargv);

	virtual time_t GetCreationTime(void) const;
	virtual void SetCreationTime(time_t T);

	virtual const char *GetTopic(void) const;
	virtual void SetTopic(const char *Topic);

	virtual const char *GetTopicNick(void) const;
	virtual void SetTopicNick(const char *Nick);

	virtual time_t GetTopicStamp(void) const;
	virtual void SetTopicStamp(time_t TS);

	virtual int HasTopic(void) const;
	virtual void SetNoTopic(void);

	virtual void AddUser(const char *Nick, const char *ModeChar);
	virtual void RemoveUser(const char *Nick);
	virtual void RenameUser(const char *Nick, const char *NewNick);

	virtual bool HasNames(void) const;
	virtual void SetHasNames(void);
	virtual const CHashtable<CNick *, false, 64> *GetNames(void) const;

	virtual void ClearModes(void);
	virtual bool AreModesValid(void) const;
	virtual void SetModesValid(bool Valid);

	virtual CBanlist *GetBanlist(void);
	virtual void SetHasBans(void);
	virtual bool HasBans(void) const;

	virtual bool SendWhoReply(bool Simulate) const;
};
