/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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
 * CResult<Type>
 *
 * A wrapper class for results and error codes.
 */
template<typename Type>
class CResult {
private:
	Type m_Result; /**< the actual result */
	unsigned int m_Code; /**< an error code, or 0 if no error occured */
	const char *m_Description; /**< a human-readable description of the error */
public:
	/**
	 * CResult
	 *
	 * Constructs a result object which represents a default result.
	 */
	CResult(void) {
		memset(&m_Result, 0, sizeof(m_Result));
		m_Code = 0;
		m_Description = NULL;
	}

	/**
	 * CResult
	 *
	 * The copy-constructor for result objects.
	 *
	 * @param Source the source object.
	 */
	CResult(const CResult<Type> &Source) {
		m_Code = Source.m_Code;
		m_Result = Source.m_Result;
		m_Description = Source.m_Description;
	}

	/**
	 * CResult
	 *
	 * Wraps a result value in a CResult object.
	 *
	 * @param Result the result value
	 */
	explicit CResult(const Type Result) {
		m_Code = 0;
		m_Description = NULL;
		m_Result = Result;
	}

	/**
	 * CResult
	 *
	 * Constructs a new result object which represents an error.
	 *
	 * @param Code an error code
	 * @param Description a description for the error
	 */
	CResult(unsigned int Code, const char *Description) {
		if (Code != 0) {
			m_Code = Code;
		} else {
			m_Code = 1;
		}

		m_Description = Description;

		memset(&m_Result, 0, sizeof(Type));
	}

	/**
	 * GetCode
	 *
	 * Returns the error code of a result object.
	 */
	unsigned int GetCode(void) const {
		return m_Code;
	}

	/**
	 * GetDescription
	 *
	 * Returns the error description of the result object.
	 */
	const char *GetDescription(void) const {
		return m_Description;
	}

	/**
	 * GetResult
	 *
	 * Returns a reference to the underlying result.
	 */
	Type &GetResult(void) {
		return m_Result;
	}

	/**
	 * operator Type &
	 *
	 * Returns a reference to the underlying result.
	 */
	operator Type &(void) {
		return m_Result;
	}
};

/**
 * IsError<Type>
 *
 * Checks whether a result object represents an error.
 *
 * @param Result the result object
 */
template<typename Type>
bool IsError(const CResult<Type> &Result) {
	unsigned int Code;

	Code = Result.GetCode();

	return (Code != 0);
}

// some macros for using CResult objects
#define RESULT CResult
#define RETURN(Type, Result) do { CResult<Type> cResult(Result); return cResult; } while (0)
#define THROW(Type, Code, Description) do { CResult<Type> cResult(Code, Description); return cResult; } while (0)
#define THROWRESULT(Type, Result) do { assert(IsError(Result)); CResult<Type> cResult(Result.GetCode(), Result.GetDescription()); if (IsError(Result)) { return cResult; } } while (0)
#define THROWIFERROR(Type, Result) do { CResult<Type> cResult(Result.GetCode(), Result.GetDescription()); if (IsError(Result)) { return cResult; } } while (0)
#define ASSERTRESULT(Result) assert(!IsError(Result))
#define GETCODE(Result) Result.GetCode()
#define GETDESCRIPTION(Result) Result.GetDescription()
#define NEWERROR(Type, Code, Description) CResult<Type>(Code, Description)

/**
 * generic_error_t
 *
 * Commonly used error codes.
 */
typedef enum generic_error_e {
	Generic_OutOfMemory = 5000,
	Generic_InvalidArgument,
	Generic_QuotaExceeded,
	Generic_Unknown
} generic_error_t;

#ifdef SWIGINTERFACE
%template(CResultCharP) CResult<char *>;
%template(CResultConstCharP) CResult<const char *>;
%template(CResultBool) CResult<bool>;
#endif
