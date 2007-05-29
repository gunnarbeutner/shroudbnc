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
CTrafficStats::CTrafficStats(safe_box_t Box) {
	SetBox(Box);

	m_PreviousSave = 0;

	if (Box == NULL) {
		m_Inbound = 0;
		m_Outbound = 0;
	} else {
		m_Inbound = safe_get_integer(Box, "Inbound");
		m_Outbound = safe_get_integer(Box, "Outbound");
	}
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

	if (m_PreviousSave < g_CurrentTime - 300 && GetBox() != NULL) {
		safe_put_integer(GetBox(), "Inbound", m_Inbound);
		m_PreviousSave = g_CurrentTime;
	}
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

	if (m_PreviousSave < g_CurrentTime - 300 && GetBox() != NULL) {
		safe_put_integer(GetBox(), "Outbound", m_Outbound);
		m_PreviousSafe = g_CurrentTime;
	}
}

/**
 * GetOutbound
 *
 * Returns the amount of used outbound traffic (in bytes).
 */
unsigned int CTrafficStats::GetOutbound(void) const {
	return m_Outbound;
}
