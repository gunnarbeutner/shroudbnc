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

#if !defined(AFX_BOUNCERCORE_H__986A00F2_4825_4A77_B6D9_BEFC05E1C565__INCLUDED_)
#define AFX_BOUNCERCORE_H__986A00F2_4825_4A77_B6D9_BEFC05E1C565__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Timer.h"

#define DEFAULT_SENDQ (10 * 1024)

class CBouncerConfig;
class CBouncerUser;
class CBouncerLog;
class CClientConnection;
class CIRCConnection;
class CIdentSupport;
class CModule;
class CConnection;
class CTimer;
struct CSocketEvents;
struct sockaddr_in;
//template <typename value_type, bool casesensitive, int Size, bool VolatileKeys> class CHashtable;

typedef struct socket_s {
	SOCKET Socket;
	CSocketEvents* Events;
} socket_t;

#ifndef SWIG
typedef struct timerchain_s {
	CTimer* ptr;
	timerchain_s* next;
} timerchain_t;
#endif

class CBouncerCore {
	CBouncerConfig* m_Config;

	SOCKET m_Listener;

	CBouncerUser** m_Users;
	int m_UserCount;

	CModule** m_Modules;
	int m_ModuleCount;

	socket_s* m_OtherSockets;
	int m_OtherSocketCount;

	time_t m_Startup;

	CBouncerLog* m_Log;

	CIdentSupport* m_Ident;

	bool m_LoadingModules;
	bool m_Running;

	int m_argc;
	char** m_argv;

	int m_SendQSizeCache;

#ifndef SWIG
	timerchain_t m_TimerChain;
#endif

	void HandleConnectingClient(SOCKET Client, sockaddr_in Remote);
	void UpdateModuleConfig(void);
	void UpdateUserConfig(void);
	bool Daemonize(void);
public:
#ifndef SWIG
	CBouncerCore(CBouncerConfig* Config, int argc, char** argv);
#endif
	virtual ~CBouncerCore(void);

	virtual void StartMainLoop(void);

	virtual CBouncerUser* GetUser(const char* Name);

	virtual void GlobalNotice(const char* Text, bool AdminOnly = false);

	virtual CBouncerUser** GetUsers(void);
	virtual int GetUserCount(void);

	virtual CModule** GetModules(void);
	virtual int GetModuleCount(void);
	virtual CModule* LoadModule(const char* Filename);
	virtual bool UnloadModule(CModule* Module);

	virtual void SetIdent(const char* Ident);
	virtual const char* GetIdent(void);

	virtual CBouncerConfig* GetConfig(void);

	virtual void RegisterSocket(SOCKET Socket, CSocketEvents* EventInterface);
	virtual void UnregisterSocket(SOCKET Socket);

	virtual SOCKET CreateListener(unsigned short Port, const char* BindIp = NULL);

	virtual void Log(const char* Format, ...);
	virtual CBouncerLog* GetLog(void);

	virtual void Shutdown(void);

	virtual CBouncerUser* CreateUser(const char* Username, const char* Password);
	virtual bool RemoveUser(const char* Username, bool RemoveConfig = true);
	virtual bool IsValidUsername(const char* Username);

	virtual time_t GetStartup(void);

	virtual const char* MD5(const char* String);

	virtual int GetArgC(void);
	virtual char** GetArgV(void);

	virtual CConnection* WrapSocket(SOCKET Socket, sockaddr_in Peer);
	virtual void DeleteWrapper(CConnection* Wrapper);

	virtual void Free(void* Pointer);
	virtual void* Alloc(size_t Size);

	virtual bool IsRegisteredSocket(CSocketEvents* Events);
	virtual SOCKET SocketAndConnect(const char* Host, unsigned short Port, const char* BindIp);

	virtual socket_t* GetSocketByClass(const char* Class, int Index);

	virtual CTimer* CreateTimer(unsigned int Interval, bool Repeat, timerproc Function, void* Cookie);
	virtual void RegisterTimer(CTimer* Timer);
	virtual void UnregisterTimer(CTimer* Timer);

	virtual int GetTimerStats(void);

	virtual bool Match(const char* Pattern, const char* String);

	virtual int GetSendQSize(void);
	virtual void SetSendQSize(int NewSize);
};

extern CBouncerCore* g_Bouncer;

#ifndef SWIG
extern adns_state g_adns_State;
#endif

#endif // !defined(AFX_BOUNCERCORE_H__986A00F2_4825_4A77_B6D9_BEFC05E1C565__INCLUDED_)
