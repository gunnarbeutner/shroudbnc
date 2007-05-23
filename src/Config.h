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

/**
 * CConfig
 *
 * Base class for configuration classes
 */
struct CConfig {
public:
	virtual void Destroy(void) = 0;

	virtual RESULT<int> ReadInteger(const char *Setting) const = 0;
	virtual RESULT<const char *> ReadString(const char *Setting) const = 0;

	virtual RESULT<bool> WriteInteger(const char *Setting, const int Value) = 0;
	virtual RESULT<bool> WriteString(const char *Setting, const char *Value) = 0;

	virtual const char *GetFilename(void) const = 0;

	virtual void Reload(void) = 0;

	virtual CHashtable<char *, false, 16> *GetInnerHashtable(void) = 0;
	virtual hash_t<char *> *Iterate(int Index) const = 0;
	virtual unsigned int GetLength(void) const = 0;

	virtual bool CanUseCache(void) = 0;
};

/**
 * CConfig
 *
 * Represents a shroudBNC configuration file
 */
class SBNCAPI CConfigFile : public CConfig, public CZoneObject<CConfigFile, 128>, public CObject<CConfigFile, CUser> {
private:
	CHashtable<char *, false, 16> m_Settings; /**< the settings */

	char *m_Filename; /**< the filename of the config */
	bool m_WriteLock; /**< marks whether the configuration file should be
						   updated when settings are added/removed */

	bool ParseConfig(void);
	RESULT<bool> Persist(void) const;

public:
#ifndef SWIG
	CConfigFile(const char *Filename, CUser *Owner);
	virtual ~CConfigFile(void);

	static RESULT<CConfigFile *> Thaw(box_t Box, CUser *Owner);
#endif

	virtual void Destroy(void);

	virtual RESULT<int> ReadInteger(const char *Setting) const;
	virtual RESULT<const char *> ReadString(const char *Setting) const;

	virtual RESULT<bool> WriteInteger(const char *Setting, const int Value);
	virtual RESULT<bool> WriteString(const char *Setting, const char *Value);

	virtual const char *GetFilename(void) const;

	virtual void Reload(void);

	virtual CHashtable<char *, false, 16> *GetInnerHashtable(void);
	virtual hash_t<char *> *Iterate(int Index) const;
	virtual unsigned int GetLength(void) const;

	virtual bool CanUseCache(void);
};
