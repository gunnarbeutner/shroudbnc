// TrafficStats.cpp: implementation of the CTrafficStats class.
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "TrafficStats.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTrafficStats::CTrafficStats() {
	m_Inbound = 0;
	m_Outbound = 0;
}

CTrafficStats::~CTrafficStats() {

}

void CTrafficStats::AddInbound(unsigned int Bytes) {
	m_Inbound += Bytes;
}

unsigned int CTrafficStats::GetInbound(void) {
	return m_Inbound;
}

void CTrafficStats::AddOutbound(unsigned int Bytes) {
	m_Outbound += Bytes;
}

unsigned int CTrafficStats::GetOutbound(void) {
	return m_Outbound;
}
