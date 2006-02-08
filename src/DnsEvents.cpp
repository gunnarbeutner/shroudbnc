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

void GenericDnsQueryCallback(void *Cookie, int Status, hostent *HostEntity) {
	CDnsQuery *Query = (CDnsQuery *)Cookie;

	Query->AsyncDnsEvent(Status, HostEntity);
}

CDnsQuery::CDnsQuery(void *EventInterface, DnsEventFunction EventFunction, int Timeout) {
	ares_options Options;

	m_EventObject = EventInterface;
	m_EventFunction = EventFunction;

	ares_init(&m_Channel);

	Options.timeout = Timeout;
	ares_init_options(&m_Channel, &Options, ARES_OPT_TIMEOUT);

	g_Bouncer->RegisterDnsQuery(this);
}

CDnsQuery::~CDnsQuery(void) {
	ares_destroy(m_Channel);

	g_Bouncer->UnregisterDnsQuery(this);
}

void CDnsQuery::GetHostByName(const char *Host, int Family) {
	ares_gethostbyname(m_Channel, Host, Family, GenericDnsQueryCallback, this);
}

void CDnsQuery::GetHostByAddr(sockaddr *Address) {
	void *IpAddr;

	if (Address->sa_family == AF_INET) {
		IpAddr = &(((sockaddr_in *)Address)->sin_addr);
	} else {
		IpAddr = &(((sockaddr_in6 *)Address)->sin6_addr);
	}

	ares_gethostbyaddr(m_Channel, IpAddr, INADDR_LEN(Address->sa_family), Address->sa_family, GenericDnsQueryCallback, this);
}

ares_channel CDnsQuery::GetChannel(void) {
	return m_Channel;
}

void CDnsQuery::AsyncDnsEvent(int Status, hostent *Response) {
	(*m_EventFunction)(m_EventObject, Response);
}
