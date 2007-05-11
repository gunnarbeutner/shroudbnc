/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                      *
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

struct CConfigModuleFar {
	virtual void Destroy(void) = 0;
	virtual void Init(CCore *Root) = 0;

	virtual CConfig *CreateConfigObject(const char *Name, CUser *Owner) = 0;
};

/**< used for getting the module's main object */
typedef CConfigModuleFar *(* FNGETCONFIGOBJECT)();

/**< used for getting the module's interface version */
typedef int (* FNGETINTERFACEVERSION)();

/**
 * CConfigModule
 *
 * Loader for user-defined config modules.
 */
class SBNCAPI CConfigModule {
private:
	HMODULE m_Image;
	CConfigModuleFar *m_Far;
	char *m_File;
	char *m_Error;

	bool InternalLoad(const char *Filename);

public:
	CConfigModule(const char *Filename);
	virtual ~CConfigModule(void);

	CConfigModuleFar *GetModule(void);
	const char *GetFilename(void);
	HMODULE GetHandle(void);
	RESULT<bool> GetError(void);

	// proxy implementation
	void Destroy(void);
	void Init(CCore *Root);

	CConfig *CreateConfigObject(const char *Name, CUser *Owner);
};

class SBNCAPI CDefaultConfigModule : public CConfigModuleFar {
public:
	void Destroy(void) {
		// nothing to do here
	}

	void Init(CCore *Root) {
		// nothing to do here
	}

	CConfig *CreateConfigObject(const char *Name, CUser *Owner);
};
