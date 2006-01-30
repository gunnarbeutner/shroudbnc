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
 * CTrafficStats
 *
 * Records traffic statistics for a user.
 */
class CTrafficStats {
public:
#ifndef SWIG
	CTrafficStats(void);

	bool Freeze(CAssocArray *Box);
	static CTrafficStats *Unfreeze(CAssocArray *Box);
#endif

	virtual void AddInbound(unsigned int Bytes);
	virtual unsigned int GetInbound(void);

	virtual void AddOutbound(unsigned int Bytes);
	virtual unsigned int GetOutbound(void);
private:
	unsigned int m_Inbound; /**< amount of inbound traffic in bytes */
	unsigned int m_Outbound; /**< amount of outbound traffic in bytes */
};
