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

#include "../StdAfx.h"
#include "Service.h"

#if defined (_WIN32) && !defined(NOSERVICE)
#undef sprintf

SERVICE_STATUS g_ServiceStatus;
SERVICE_STATUS_HANDLE g_ServiceStatusHandle;
extern bool g_Service;

bool InstallService(const char *ExeName) {
	SC_HANDLE hSCM, hService;
	SERVICE_DESCRIPTION ServiceDescription;
	char ExeNameArgs[500];

	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

	if (hSCM == NULL) {
		return false;
	}

	sprintf(ExeNameArgs, "\"%s\" --service", ExeName);

	hService = CreateService(hSCM, "sbnc", "shroudBNC", SERVICE_CHANGE_CONFIG,
		SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE,
		ExeNameArgs, NULL, NULL, NULL,
		NULL, NULL);

	if (hService == NULL && (GetLastError() != ERROR_DUPLICATE_SERVICE_NAME || GetLastError() == ERROR_SERVICE_EXISTS)) {
		CloseServiceHandle(hSCM);

		return false;
	}

	ServiceDescription.lpDescription = "Provides bouncer services for IRC users.";

	ChangeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &ServiceDescription);

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return true;
}

bool UninstallService(void) {
	SC_HANDLE hSCM, hService;

	hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

	if (hSCM == NULL) {
		return false;
	}

	hService = OpenService(hSCM, "sbnc", DELETE);

	if (hService == NULL) {
		return false;
	}

	if (!DeleteService(hService)) {
		CloseServiceHandle(hService);
		CloseServiceHandle(hSCM);

		return false;
	}

	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

	return true;
}

void WINAPI ServiceCtrlHandler(DWORD Opcode) {
	switch (Opcode) {
		case SERVICE_CONTROL_STOP:
			g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
			break;
		case SERVICE_CONTROL_PAUSE:
			g_SetStatusFunc(STATUS_PAUSE);
			g_ServiceStatus.dwCurrentState = SERVICE_PAUSED;
			break;
		case SERVICE_CONTROL_CONTINUE:
			g_SetStatusFunc(STATUS_RUN);
			g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
			break;
		default:
			;// Do nothing.. so far
	}

	SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
}

void ServiceMain(void) {
	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ "sbnc", ServiceStart },
		{ NULL, NULL }
	};

	g_Service = true;

	StartServiceCtrlDispatcher(DispatchTable);
}

void WINAPI ServiceStart(DWORD argc, LPSTR *argv) {
	int ArgC;
	char *ArgV[1];

	g_ServiceStatus.dwServiceType = SERVICE_WIN32;
	g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
	g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
	g_ServiceStatus.dwWin32ExitCode = 0;
	g_ServiceStatus.dwServiceSpecificExitCode = 0;
	g_ServiceStatus.dwCheckPoint = 0;
	g_ServiceStatus.dwWaitHint = 0;

	g_ServiceStatusHandle = RegisterServiceCtrlHandler("shroudBNC", ServiceCtrlHandler);

	if (g_ServiceStatusHandle == (SERVICE_STATUS_HANDLE)0) {
		return;
	}

	// initialization here

	g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;

	if (!SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus)) {
		return;
	}

	ArgC = 1;
	ArgV[0] = "sbncloader";

	main(ArgC, ArgV);

	return;
}
#endif
