// TrafficStats.h: interface for the CTrafficStats class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TRAFFICSTATS_H__FF402157_56A8_4A8F_B5E3_73F557A44D9B__INCLUDED_)
#define AFX_TRAFFICSTATS_H__FF402157_56A8_4A8F_B5E3_73F557A44D9B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CTrafficStats {
public:
	CTrafficStats();
	virtual ~CTrafficStats();

	virtual void AddInbound(unsigned int Bytes);
	virtual unsigned int GetInbound(void);

	virtual void AddOutbound(unsigned int Bytes);
	virtual unsigned int GetOutbound(void);
private:
	unsigned int m_Inbound;
	unsigned int m_Outbound;
};

#endif // !defined(AFX_TRAFFICSTATS_H__FF402157_56A8_4A8F_B5E3_73F557A44D9B__INCLUDED_)
