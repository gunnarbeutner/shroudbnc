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

template<typename Type>
class CResult {
private:
	Type m_Result;
	unsigned int m_Code;
	char *m_Description;
public:
	CResult(void) {
		memset(&m_Result, 0, sizeof(m_Result));
		m_Code = 0;
		m_Description = NULL;
	}

	CResult(const CResult<Type> &Source) {
		m_Code = Source.m_Code;
		m_Result = Source.m_Result;

		if (Source.m_Description != NULL) {
			m_Description = strdup(Source.m_Description);
		} else {
			m_Description = NULL;
		}
	}

	explicit CResult(const Type Result) {
		m_Code = 0;
		m_Description = NULL;
		m_Result = Result;
	}

	CResult(unsigned int Code, const char *Description) {
		if (Code != 0) {
			m_Code = Code;
		} else {
			m_Code = 1;
		}

		if (Description != NULL) {
			m_Description = strdup(Description);
		} else {
			m_Description = NULL;
		}

		memset(&m_Result, 0, sizeof(Type));
	}

	~CResult(void) {
		free(m_Description);
	}

	unsigned int GetCode(void) const {
		return m_Code;
	}

	const char *GetDescription(void) const {
		return m_Description;
	}

	Type &GetResult(void) {
		return m_Result;
	}

	operator Type &(void) {
		return m_Result;
	}
};

template<typename Type>
bool IsError(const CResult<Type> &Result) {
	unsigned int Code;

	Code = Result.GetCode();

#ifdef _DEBUG
	if (Code != 0) {
		printf("CResult<%s>: Code %d (%s)\n", typeid(Type).name(), Code, Result.GetDescription());
	}
#endif

	return (Code != 0);
}

#define RESULT(Type) CResult<Type>
#define RETURN(Type, Result) do { CResult<Type> cResult(Result); return cResult; } while (0)
#define THROW(Type, Code, Description) do { CResult<Type> cResult(Code, Description); return cResult; } while (0)
#define THROWRESULT(Type, Result) do { assert(IsError(Result)); CResult<Type> cResult(Result.GetCode(), Result.GetDescription()); if (IsError(Result)) { return cResult; } } while (0)
#define THROWIFERROR(Type, Result) do { CResult<Type> cResult(Result.GetCode(), Result.GetDescription()); if (IsError(Result)) { return cResult; } } while (0)
#define ASSERTRESULT(Result) assert(!IsError(Result))
#define GETCODE(Result) Result.GetCode()
#define GETDESCRIPTION(Result) Result.GetDescription()
#define NEWERROR(Type, Code, Description) CResult<Type>(Code, Description)

typedef enum generic_error_e {
	Generic_OutOfMemory = 5000,
	Generic_InvalidArgument,
	Generic_Unknown
} generic_error_t;
