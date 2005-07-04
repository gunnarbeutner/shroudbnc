// Banlist.h: interface for the CBanlist class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_BANLIST_H__BBD3E3D9_7146_452C_BBB3_8762021E7C62__INCLUDED_)
#define AFX_BANLIST_H__BBD3E3D9_7146_452C_BBB3_8762021E7C62__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef struct ban_s {
	char* Mask;
	char* Nick;
	time_t TS;
} ban_t;

class CBanlist {
public:
	CBanlist();
	virtual ~CBanlist();

	virtual void SetBan(const char* Mask, const char* Nick, time_t TS);
	virtual void UnsetBan(const char* Mask);

	const ban_t* CBanlist::GetBan(const char* Mask);
	virtual const ban_t* Iterate(int Skip);
private:
	CHashtable<ban_t*, false>* m_Bans;
};

#endif // !defined(AFX_BANLIST_H__BBD3E3D9_7146_452C_BBB3_8762021E7C62__INCLUDED_)
