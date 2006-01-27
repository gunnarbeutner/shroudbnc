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

#include "../src/StdAfx.h"
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#undef getpid
#include <process.h>
#endif

class CUptimeModule;
class CUdpSocket;

// copy: from uptime/uptime.c
typedef struct PackUp
{
        int     regnr;
        int     pid;
        int     type;
        unsigned long   cookie;
        unsigned long   uptime;
        unsigned long   ontime;
        unsigned long   now2;
        unsigned long   sysup;
        char    string[3];

} PackUp;

PackUp upPack;

int uptimeport = 9969;
unsigned long uptimecookie, uptimecount;
char *uptimehost;
// end of copy

#define UPTIME_SBNC 2
#define UPTIME_HOST "uptime.eggheads.org"

CCore *g_Bouncer;
CUptimeModule *g_UptimeMod;
CTimer *g_UptimeTimer;
CUdpSocket *g_UptimeSocket;
bool g_Ret;

bool UptimeTimerProc(time_t Now, void *Cookie);

#ifdef _WIN32
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 ) {
    return TRUE;
}
#else
int main(int argc, char* argv[]) { return 0; }
#endif

class CUdpSocket : CSocketEvents {
	SOCKET m_Socket;

public:
	CUdpSocket(unsigned short Port) {
		unsigned long lTrue = 1;
		sockaddr_in sloc;

		m_Socket = socket(AF_INET, SOCK_DGRAM, 0);

		if (m_Socket == INVALID_SOCKET)
			return;

		memset(&sloc, 0, sizeof(sloc));
		sloc.sin_family = AF_INET;
	#ifdef _WIN32
		sloc.sin_addr.S_un.S_addr = INADDR_ANY;
	#else
		sloc.sin_addr.s_addr = INADDR_ANY;
	#endif

		if (bind(m_Socket, (sockaddr *)&sloc, sizeof(sloc)) < 0) {
			closesocket(m_Socket);
			m_Socket = INVALID_SOCKET;

			return;
		}

		ioctlsocket(m_Socket, FIONBIO, &lTrue);

		g_Bouncer->RegisterSocket(m_Socket, this);
	}

	~CUdpSocket() {
		if (m_Socket != INVALID_SOCKET) {
			g_Bouncer->UnregisterSocket(m_Socket);
			closesocket(m_Socket);
		}
	}

	void Destroy(void) {
		delete this;
	}

	bool IsValid(void) {
		return (m_Socket != INVALID_SOCKET);
	}

	virtual bool Read(bool DontProcess = false) {
		return true;
	}

	virtual void Write(void) {
	}

	virtual void Error(void) {
	}

	bool HasQueuedData(void) {
		return false;
	}

	bool DoTimeout(void) {
		return false;
	}

	bool ShouldDestroy(void) {
		return false;
	}

	const char *ClassName(void) {
		return "CUdpSocket";
	}

	int SendTo(const char *Data, int Length, sockaddr_in Destination) {
		return sendto(m_Socket, Data, Length, 0, (sockaddr *)&Destination, sizeof(Destination));
	}
};

class CUptimeModule : public CModuleFar {
	friend bool UptimeTimerProc(time_t Now, void *Cookie);

	void Destroy(void) {
		g_UptimeSocket->Destroy();
		g_UptimeTimer->Destroy();

		free(uptimehost);

		delete this;
	}

	void Init(CCore* Root) {
		g_Bouncer = Root;

		upPack.regnr = 0;
		upPack.pid = htonl(getpid());
		upPack.type = htonl(UPTIME_SBNC);
		upPack.cookie = 0;
		upPack.uptime = htonl(g_Bouncer->GetStartup());

		uptimecookie = rand();
		uptimecount = 0;

		g_UptimeSocket = new CUdpSocket(uptimeport);

		g_UptimeTimer = g_Bouncer->CreateTimer(6 * 3600, true, UptimeTimerProc, NULL);

		uptimehost = strdup(UPTIME_HOST);

		SendUptime(time(NULL));
	}

	void SendUptime(time_t Now) {
#ifndef _WIN32
		struct stat st;
#endif
		PackUp *mem;
		int len;
		sockaddr_in sloc;
		CUser *FirstUser;
		hostent *hent;
		const char *Server;

		uptimecookie = (uptimecookie + 1) * 18457;
		upPack.cookie = htonl(uptimecookie);

		upPack.now2 = htonl(Now);

#ifdef _WIN32
		upPack.sysup = Now - GetTickCount() / 1000;
#else
		if (stat("/proc",&st) < 0)
			upPack.sysup = 0;
		else
			upPack.sysup = htonl(st.st_ctime);
#endif

		upPack.uptime = htonl(g_Bouncer->GetStartup());

		if (g_Bouncer->GetUserCount() != 0) {
			FirstUser = g_Bouncer->GetUsers()->Iterate(0)->Value;

			upPack.ontime = htonl(Now - FirstUser->IRCUptime());
		} else
			return;

		uptimecount++;

		if (FirstUser->GetIRCConnection() != NULL) {
			Server = FirstUser->GetIRCConnection()->GetServer();
		} else {
			Server = FirstUser->GetServer();
		}

		len = sizeof(upPack) + strlen(FirstUser->GetUsername()) + strlen(Server) + strlen(BNCVERSION);
		mem = (PackUp *)malloc(len);
		memcpy(mem, &upPack, sizeof(upPack));
		snprintf(mem->string, len - sizeof(upPack) + sizeof(mem->string), "%s %s %s", FirstUser->GetUsername(), Server, BNCVERSION);

		sloc.sin_family = AF_INET;
		sloc.sin_port = htons(uptimeport);

		hent = gethostbyname(uptimehost);

		if (hent) {
			in_addr* peer = (in_addr*)hent->h_addr_list[0];

#ifdef _WIN32
			sloc.sin_addr.S_un.S_addr = peer->S_un.S_addr;
#else
			sloc.sin_addr.s_addr = peer->s_addr;
#endif
		} else {
			unsigned long addr = inet_addr(uptimehost);

			if (addr == INADDR_NONE)
				return;

#ifdef _WIN32
			sloc.sin_addr.S_un.S_addr = addr;
#else
			sloc.sin_addr.s_addr = addr;
#endif
		}

 		g_UptimeSocket->SendTo((const char *)mem, len, sloc);

		free(mem);
	}

	bool InterceptIRCMessage(CIRCConnection* IRC, int argc, const char** argv) {
		return true;
	}

	bool InterceptClientMessage(CClientConnection* Client, int argc, const char** argv) {
		return true;
	}

	void AttachClient(const char* Client) {
	}

	void DetachClient(const char* Client) {
	}

	void ServerDisconnect(const char* Client) {
	}

	void ServerConnect(const char* Client) {
	}

	void ServerLogon(const char* Client) {
	}

	void UserLoad(const char* User) {
	}

	void UserCreate(const char* User) {
	}

	void UserDelete(const char* User) {
	}

	void SingleModeChange(CIRCConnection* IRC, const char* Channel, const char* Source, bool Flip, char Mode, const char* Parameter) {
	}

	const char* Command(const char* Cmd, const char* Parameters) {
		return NULL;
	}

	bool InterceptClientCommand(CClientConnection* Connection, const char* Subcommand, int argc, const char** argv, bool NoticeUser) { return false; }
};

bool UptimeTimerProc(time_t Now, void *Cookie) {
	g_UptimeMod->SendUptime(Now);
	return true;
}

extern "C" CModuleFar* bncGetObject(void) {
	g_UptimeMod = new CUptimeModule();
	return (CModuleFar*)g_UptimeMod;
}
