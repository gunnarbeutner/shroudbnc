/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                      *
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
 * CTrafficStats
 *
 * Constructs a new traffic stats object.
 */
CTrafficStats::CTrafficStats() {
	m_Inbound = 0;
	m_Outbound = 0;
}

/**
 * AddInbound
 *
 * Adds inbound traffic.
 *
 * @param Bytes the amount of traffic
 */
void CTrafficStats::AddInbound(unsigned int Bytes) {
	m_Inbound += Bytes;
}

/**
 * GetInbound
 *
 * Returns the amount of used inbound traffic (in bytes).
 */
unsigned int CTrafficStats::GetInbound(void) const {
	return m_Inbound;
}

/**
 * AddOutbound
 *
 * Adds outbound traffic.
 *
 * @param Bytes the amount of traffic
 */
void CTrafficStats::AddOutbound(unsigned int Bytes) {
	m_Outbound += Bytes;
}

/**
 * GetOutbound
 *
 * Returns the amount of used outbound traffic (in bytes).
 */
unsigned int CTrafficStats::GetOutbound(void) const {
	return m_Outbound;
}

/**
 * Freeze
 *
 * Persists a CTrafficStats object.
 *
 * @param Box the box
 */
RESULT<bool> CTrafficStats::Freeze(CAssocArray *Box) {
	if (Box == NULL) {
		THROW(bool, Generic_InvalidArgument, "Box cannot be NULL.");
	}

	Box->AddInteger("~traffic.in", m_Inbound);
	Box->AddInteger("~traffic.out", m_Outbound);

	delete this;

	RETURN(bool, true);
}

/**
 * Thaw
 *
 * Depersists a CTrafficStats object.
 *
 * @param Box the box
 */
RESULT<CTrafficStats *> CTrafficStats::Thaw(CAssocArray *Box) {
	CTrafficStats *TrafficStats;

	if (Box == NULL) {
		THROW(CTrafficStats *, Generic_InvalidArgument, "Box cannot be NULL.");
	}

	TrafficStats = new CTrafficStats();

	CHECK_ALLOC_RESULT(TrafficStats, new) {
		THROW(CTrafficStats *, Generic_OutOfMemory, "new operator failed.");
	} CHECK_ALLOC_RESULT_END;

	TrafficStats->m_Inbound = Box->ReadInteger("~traffic.in");
	TrafficStats->m_Outbound = Box->ReadInteger("~traffic.out");

	RETURN(CTrafficStats *, TrafficStats);
}
