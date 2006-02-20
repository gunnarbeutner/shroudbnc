#define NOADNSLIB
#include "../StdAfx.h"

CAssocArray::CAssocArray(void) {
	m_Values = NULL;
	m_Count = 0;
}

CAssocArray::~CAssocArray(void) {
	for (unsigned int i = 0; i < m_Count; i++) {
		free(m_Values[i].Name);

		if (m_Values[i].Type == Assoc_String) {
			free(m_Values[i].ValueString);
		} else if (m_Values[i].Type == Assoc_Box) {
			delete m_Values[i].ValueBox;
		}
	}

	free(m_Values);
}

void CAssocArray::AddString(const char *Name, const char *Value) {
	if (Value == NULL) {
		return;
	}

	m_Values = (assoc_t *)realloc(m_Values, sizeof(assoc_t) * ++m_Count);

	m_Values[m_Count - 1].Name = strdup(Name);
	m_Values[m_Count - 1].Type = Assoc_String;
	m_Values[m_Count - 1].ValueString = strdup(Value);
}

void CAssocArray::AddInteger(const char *Name, int Value) {
	m_Values = (assoc_t *)realloc(m_Values, sizeof(assoc_t) * ++m_Count);

	m_Values[m_Count - 1].Name = strdup(Name);
	m_Values[m_Count - 1].Type = Assoc_Integer;
	m_Values[m_Count - 1].ValueInt = Value;
}

void CAssocArray::AddBox(const char *Name, CAssocArray *Value) {
	m_Values = (assoc_t *)realloc(m_Values, sizeof(assoc_t) * ++m_Count);

	m_Values[m_Count - 1].Name = strdup(Name);
	m_Values[m_Count - 1].Type = Assoc_Box;
	m_Values[m_Count - 1].ValueBox = Value;
}

const char *CAssocArray::ReadString(const char *Name) {
	for (unsigned int i = 0; i < m_Count; i++) {
		if (strcasecmp(m_Values[i].Name, Name) == 0) {
			if (m_Values[i].Type == Assoc_String) {
				return m_Values[i].ValueString;
			} else {
				return "";
			}
		}
	}

	return NULL;
}

int CAssocArray::ReadInteger(const char *Name) {
	for (unsigned int i = 0; i < m_Count; i++) {
		if (strcasecmp(m_Values[i].Name, Name) == 0) {
			if (m_Values[i].Type == Assoc_Integer) {
				return m_Values[i].ValueInt;
			} else {
				return 0;
			}
		}
	}

	return 0;
}

CAssocArray *CAssocArray::ReadBox(const char *Name) {
	for (unsigned int i = 0; i < m_Count; i++) {
		if (strcasecmp(m_Values[i].Name, Name) == 0) {
			if (m_Values[i].Type == Assoc_Box) {
				return m_Values[i].ValueBox;
			} else {
				return NULL;
			}
		}
	}

	return NULL;
}

CAssocArray *CAssocArray::Create(void) {
	return new CAssocArray();
}

void CAssocArray::Destroy(void) {
	delete this;
}
