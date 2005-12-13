#define ASSOC_STRING 1
#define ASSOC_INT 2
#define ASSOC_POINTER 3

class CAssocArray;

typedef struct assoc_s {
	char *Name;
	int Type;

	union {
		char *ValueString;
		int ValueInt;
		CAssocArray *ValueBox;
	};
} assoc_t;

class CAssocArray {
	assoc_t *m_Values;
	int m_Count;
public:
	CAssocArray(void);
	virtual ~CAssocArray(void);

	virtual void AddString(const char *Name, const char *Value);
	virtual void AddInteger(const char *Name, int Value);
	virtual void AddBox(const char *Name, CAssocArray *Value);

	virtual const char *ReadString(const char *Name);
	virtual int ReadInteger(const char *Name);
	virtual CAssocArray *ReadBox(const char *Name);

	virtual CAssocArray *Create(void);
	virtual void Destroy(void);
};
