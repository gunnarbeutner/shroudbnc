// DnsEvents.h: interface for the CDnsEvents class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DNSEVENTS_H__C509DE76_DC3C_43C2_AA5D_0F6E529CADEE__INCLUDED_)
#define AFX_DNSEVENTS_H__C509DE76_DC3C_43C2_AA5D_0F6E529CADEE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

struct CDnsEvents {
	virtual void AsyncDnsFinished(adns_query* query, adns_answer* response) = 0;
};

#endif // !defined(AFX_DNSEVENTS_H__C509DE76_DC3C_43C2_AA5D_0F6E529CADEE__INCLUDED_)
