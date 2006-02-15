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

/**
 * GenericDnsQueryCallback
 *
 * Used as a thunk between c-ares' C-style callbacks and shroudBNC's
 * object-oriented dns class.
 *
 * @param Cookie a pointer to a CDnsQuery object
 * @param Status the status of the dns query
 * @param HostEntity the response for the dns query (can be NULL)
 */
void GenericDnsQueryCallback(void *Cookie, int Status, hostent *HostEntity) {
	CDnsQuery *Query = (CDnsQuery *)Cookie;

	Query->AsyncDnsEvent(Status, HostEntity);
}

/**
 * CDnsQuery
 *
 * Constructs a new DNS query object.
 *
 * @param EventInterface an object to which DNS callbacks are routed
 * @param EventFunction a function which is called when DNS queries are completed
 * @param Timeout specifies the timeout value for dns queries
 *
 * \see IMPL_DNSEVENTPROXY
 * \see USE_DNSEVENTPROXY
 */

CDnsQuery::CDnsQuery(void *EventInterface, DnsEventFunction EventFunction, int Timeout) {
	ares_options Options;

	m_EventObject = EventInterface;
	m_EventFunction = EventFunction;

	ares_init(&m_Channel);

	Options.timeout = Timeout;
	ares_init_options(&m_Channel, &Options, ARES_OPT_TIMEOUT);

	g_Bouncer->RegisterDnsQuery(this);
}

/**
 * ~CDnsQuery
 *
 * Destructs a DNS query object.
 */
CDnsQuery::~CDnsQuery(void) {
	ares_destroy(m_Channel);

	g_Bouncer->UnregisterDnsQuery(this);
}

/**
 * GetHostByName
 *
 * Asynchronously performs the task of the "gethostbyname" function
 * and calls the callback function once the operation has completed.
 *
 * @param Host the hostname
 * @param Family the address family (AF_INET or AF_INET6)
 */
void CDnsQuery::GetHostByName(const char *Host, int Family) {
	ares_gethostbyname(m_Channel, Host, Family, GenericDnsQueryCallback, this);
}

/**
 * GetHostByAddr
 *
 * Asynchronously performs the task of the "gethostbyaddr" function
 * and calls the callback function once the operation has completed.
 *
 * @param Address the address for which the hostname should be looked up
 */
void CDnsQuery::GetHostByAddr(sockaddr *Address) {
	void *IpAddr;

#ifdef IPV6
	if (Address->sa_family == AF_INET) {
#endif
		IpAddr = &(((sockaddr_in *)Address)->sin_addr);
#ifdef IPV6
	} else {
		IpAddr = &(((sockaddr_in6 *)Address)->sin6_addr);
	}
#endif

	ares_gethostbyaddr(m_Channel, IpAddr, INADDR_LEN(Address->sa_family), Address->sa_family, GenericDnsQueryCallback, this);
}

/**
 * GetChannel
 *
 * Returns the underlying c-ares channel object.
 */
ares_channel CDnsQuery::GetChannel(void) {
	return m_Channel;
}

/**
 * AsyncDnsEvent
 *
 * Used by GenericDnsQueryCallback to notify the object of the status
 * of a dns query.
 *
 * @param Status the status
 * @param Response the response for the dns query
 */
void CDnsQuery::AsyncDnsEvent(int Status, hostent *Response) {
	(*m_EventFunction)(m_EventObject, Response);
}
