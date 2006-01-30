typedef enum assoctype_e : int {
	Assoc_String = 1,
	Assoc_Integer = 2,
	Assoc_Box = 3
} assoctype_t;

class CAssocArray;

typedef struct assoc_s {
	char *Name; /**< the name of the item */
	assoctype_t Type; /**< the type of the item, see ASSOC_* constants for details */

	union {
		char *ValueString; /**< the value for (type == Assoc_String) */
		int ValueInt; /**< the value for (type == Assoc_Integer) */
		CAssocArray *ValueBox; /**< the value for (type == Assoc_Box) */
	};
} assoc_t;

/**
 * CAssocArray
 *
 * An associative list of items.
 */
class CAssocArray {
	assoc_t *m_Values; /**< the values for this associative array */
	int m_Count; /**< the count of items */
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
