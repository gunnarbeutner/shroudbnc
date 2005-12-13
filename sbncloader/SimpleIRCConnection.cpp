#include "StdAfx.h"
#include "SimpleIRCConnection.h"

CSimpleIRCConnection::CSimpleIRCConnection(SOCKET Socket) {
	m_Socket = Socket;
}

 CSimpleIRCConnection::~CSimpleIRCConnection(void)
{
}
