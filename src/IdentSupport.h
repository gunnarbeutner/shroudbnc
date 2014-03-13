/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2014 Gunnar Beutner                                      *
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

#ifndef IDENTSUPPORT_H
#define IDENTSUPPORT_H

/**
 * CIdentSupport
 *
 * Used for "communicating" with ident daemons.
 */
class SBNCAPI CIdentSupport {
	char *m_Ident; /**< the ident */
public:
#ifndef SWIG
	CIdentSupport(void);
	~CIdentSupport(void);
#endif /* SWIG */

	void SetIdent(const char *Ident);
	const char *GetIdent(void) const;
};

#endif /* IDENTSUPPORT_H */
