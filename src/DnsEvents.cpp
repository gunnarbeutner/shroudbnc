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

#include "StdAfx.h"

ares_channel CDnsQuery::m_DnsChannel; /**< ares channel object */

/**
 * GenericDnsQueryCallback
 *
 * Used as a thunk between c-ares' C-style callbacks and shroudBNC's
 * object-oriented dns class.
 *
 * @param CookieRaw a pointer to a DnsEventCookie
 * @param Status the status of the dns query
 * @param Timeouts the number of timeouts that occured while querying the dns servers
 * @param HostEntity the response for the dns query (can be NULL)
 */
void GenericDnsQueryCallback(void *CookieRaw, int Status, int Timeouts, hostent *HostEntity) {
	DnsEventCookie *Cookie = (DnsEventCookie *)CookieRaw;

	if (Cookie->Query == NULL) {
		return;
	}

	Cookie->Query->AsyncDnsEvent(Status, HostEntity);
	Cookie->Query->m_PendingQueries--;

	Cookie->RefCount--;

	if (Cookie->RefCount <= 0) {
		delete Cookie;
	}
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
	m_Timeout = Timeout;

	m_EventCookie = new DnsEventCookie;
	m_EventCookie->RefCount = 1;
	m_EventCookie->Query = this;

	m_EventObject = EventInterface;
	m_EventFunction = EventFunction;

	m_PendingQueries = 0;

	if (m_DnsChannel == NULL) {
		ares_options Options;

		Options.timeout = m_Timeout;
		ares_init_options(&m_DnsChannel, &Options, ARES_OPT_TIMEOUT);
	}
}

/**
 * ~CDnsQuery
 *
 * Destroys a CDnsQuery object.
 */
CDnsQuery::~CDnsQuery(void) {
	m_EventCookie->RefCount--;
	m_EventCookie->Query = NULL;

	if (m_EventCookie->RefCount <= 0) {
		delete m_EventCookie;
	}
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
	struct addrinfo hints = {}, *result;

	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_family = Family;

	if (getaddrinfo(Host, NULL, &hints, &result) == 0) {
		struct hostent hent = {};
		char *Address;
		char *AddressList[2];

		hent.h_addrtype = result->ai_family;
		hent.h_length = INADDR_LEN(result->ai_family);

		if (result->ai_family == AF_INET) {
			Address = (char *)&(((sockaddr_in *)result->ai_addr)->sin_addr);
		} else if (result->ai_family == AF_INET6) {
			Address = (char *)&(((sockaddr_in6 *)result->ai_addr)->sin6_addr);
		} else {
			m_EventCookie->RefCount++;
			GenericDnsQueryCallback(m_EventCookie, ARES_EBADNAME, 0, NULL);
			return;
		}

		AddressList[0] = Address;
		AddressList[1] = NULL;

		hent.h_addr_list = AddressList;

		m_EventCookie->RefCount++;
		GenericDnsQueryCallback(m_EventCookie, ARES_SUCCESS, 0, &hent);

		freeaddrinfo(result);

		return;
	}

	m_PendingQueries++;
	m_EventCookie->RefCount++;
	ares_gethostbyname(m_DnsChannel, Host, Family, GenericDnsQueryCallback, m_EventCookie);
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

#ifdef HAVE_IPV6
	if (Address->sa_family == AF_INET) {
#endif /* HAVE_IPV6 */
		IpAddr = &(((sockaddr_in *)Address)->sin_addr);
#ifdef HAVE_IPV6
	} else {
		IpAddr = &(((sockaddr_in6 *)Address)->sin6_addr);
	}
#endif /* HAVE_IPV6 */

	m_PendingQueries++;
	m_EventCookie->RefCount++;
	ares_gethostbyaddr(m_DnsChannel, IpAddr, INADDR_LEN(Address->sa_family),
		Address->sa_family, GenericDnsQueryCallback, m_EventCookie);
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
	if (m_EventFunction != NULL) {
		(*m_EventFunction)(m_EventObject, Response);
	}
}

/**
 * RegisterSockets
 *
 * Registers all DNS sockets and returns a cookie that can be used to
 * unregister the sockets later on - or NULL if no sockets were registered.
 *
 * @return DnsSocketCookie a cookie
 */
DnsSocketCookie *CDnsQuery::RegisterSockets() {
	SOCKET Sockets[ARES_GETSOCK_MAXNUM];
	int Bitmask, Count = 0;
	DnsSocketCookie *Cookie;

	if (m_DnsChannel == NULL) {
		return NULL;
	}

	Bitmask = ares_getsock(m_DnsChannel, Sockets, sizeof(Sockets) / sizeof(*Sockets));

	Cookie = new DnsSocketCookie;

	if (AllocFailed(Cookie)) {
		g_Bouncer->Fatal();
	}

	for (int i = 0; i < (int)(sizeof(Sockets) / sizeof(*Sockets)); i++) {
		if (!ARES_GETSOCK_READABLE(Bitmask, i) && !ARES_GETSOCK_WRITABLE(Bitmask, i)) {
			continue;
		}

		Count++;
		// ctor takes care of registering the socket
#ifdef _MSC_VER
#	pragma warning( push )
#	pragma warning( disable : 4800 )
#endif /* _MSC_VER */
		Cookie->Sockets[Count - 1] = new CDnsSocket(Sockets[i], ARES_GETSOCK_WRITABLE(Bitmask, i));
#ifdef _MSC_VER
#	pragma warning( pop ) 
#endif /* _MSC_VER */
	}

	if (Count == 0) {
		delete Cookie;

		return NULL;
	}

	Cookie->Count = Count;

	return Cookie;
}

/**
 * UnregisterSockets
 *
 * Unregisters a set of DNS sockets.
 *
 * @param Cookie a cookie returned by RegisterSockets
 */
void CDnsQuery::UnregisterSockets(DnsSocketCookie *Cookie) {
	if (Cookie == NULL) {
		return;
	}

	for (int i = 0; i < Cookie->Count; i++) {
		// dtor takes care of un-registering the socket
		Cookie->Sockets[i]->Destroy();

		delete Cookie->Sockets[i];
	}

	delete Cookie;
}

/**
 * ProcessTimeouts
 *
 * Processes timeouts for the DNS sockets.
 */
void CDnsQuery::ProcessTimeouts(void) {
	if (m_DnsChannel != NULL) {
		ares_process_fd(m_DnsChannel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
	}
}

/**
 * GetDnsChannel
 *
 * Returns the c-ares channel object.
 *
 * @return ares_channel the channel object
 */
ares_channel CDnsQuery::GetDnsChannel(void) {
	return m_DnsChannel;
}
