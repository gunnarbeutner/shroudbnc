class CSimpleIRCConnection {
	SOCKET m_Socket;

	char *m_SendQ;
	int m_SendQSize;

	char *m_RecvQ;
	int m_RecvQSize;
public:
	CSimpleIRCConnection(SOCKET Client);
	virtual ~CSimpleIRCConnection(void);

	void Read(void);
	void Write(void);
};
