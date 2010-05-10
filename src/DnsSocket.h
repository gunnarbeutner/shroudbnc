/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

class CDnsSocket : public CSocketEvents {
private:
        SOCKET m_Socket;
        bool m_Outbound;
public:
        CDnsSocket(SOCKET Socket, bool Outbound);

        void Destroy(void);

        int Read(bool DontProcess = false);
        int Write(void);
        void Error(int ErrorCode);

        bool HasQueuedData(void) const;
        bool ShouldDestroy(void) const;

        const char *GetClassName(void) const;
};

typedef struct {
        CDnsSocket *Sockets[ARES_GETSOCK_MAXNUM];
	int Count;
} DnsSocketCookie;

