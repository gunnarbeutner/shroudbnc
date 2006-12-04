#include "../src/StdAfx.h"
#include <mysql.h>

class CMysqlConfigModule;

CMysqlConfigModule *g_MysqlMod;
CCore *g_Bouncer;
time_t g_NextConnect;


MYSQL *MysqlModConnect(void);
MYSQL *MysqlModGetConnection(void);

void FreeSimpleString(char *String) {
	free(String);
}

class CMysqlConfig : public CConfig {
	char *m_Filename;
	const char *m_Table;
	CUser *m_User;
	CHashtable<char *, false, 16> *m_Cache;
	mutable CHashtable<char *, false, 16> *m_WriteQueue;
	CLog *m_Log;
	mutable time_t m_LastFetch;
	unsigned int m_UpdateInterval;

public:
	CMysqlConfig(const char *Filename, CUser *User, MYSQL *MysqlConnection, const char *Table, CLog *Log, unsigned int UpdateInterval) {
		m_Filename = strdup(Filename);
		m_User = User;
		m_Table = Table;
		m_Log = Log;
		m_LastFetch = 0;
		m_UpdateInterval = UpdateInterval;

		m_Cache = new CHashtable<char *, false, 16>();
		m_Cache->RegisterValueDestructor(FreeSimpleString);

		m_WriteQueue = new CHashtable<char *, false, 16>();
		m_WriteQueue->RegisterValueDestructor(FreeSimpleString);
	}

	~CMysqlConfig(void) {
		FlushWriteQueue();

		free(m_Filename);
		delete m_Cache;
		delete m_WriteQueue;
	}

	void FlushWriteQueue(void) const {
		unsigned int i = 0;
		hash_t<char *> *Bucket;
		MYSQL *Mysql;

		if (m_WriteQueue->GetLength() == 0) {
			return;
		}

		Mysql = MysqlModGetConnection();

		if (Mysql == NULL) {
			MysqlModConnect();

			if (Mysql == NULL) {
				return;
			}
		}

		while ((Bucket = m_WriteQueue->Iterate(i++)) != NULL) {
			WriteStringNoQueue(Bucket->Name, Bucket->Value);
		}

		m_WriteQueue->Clear();
	}

	bool WriteStringNoQueue(const char *Setting, const char *Value) const {
		char *Query;
		const utility_t *Utils;
		MYSQL *Mysql;
		char *FileEscaped, *SettingEscaped, *ValueEscaped, *TableEscaped;

		if (Value != NULL) {
			m_Cache->Add(Setting, strdup(Value));
		} else {
			m_Cache->Remove(Setting);
		}

		Mysql = MysqlModGetConnection();

		if (Mysql == NULL) {
			return false;
		}

		Utils = g_Bouncer->GetUtilities();

		FileEscaped = (char *)malloc(strlen(m_Filename) * 2 + 1);
		SettingEscaped = (char *)malloc(strlen(Setting) * 2 + 1);
		ValueEscaped = (char *)malloc(strlen(Value) * 2 + 1);
		TableEscaped = (char *)malloc(strlen(m_Table) * 2 + 1);

		mysql_real_escape_string(Mysql, FileEscaped, m_Filename, strlen(m_Filename));
		mysql_real_escape_string(Mysql, SettingEscaped, Setting, strlen(Setting));
		mysql_real_escape_string(Mysql, ValueEscaped, Value, strlen(Value));
		mysql_real_escape_string(Mysql, TableEscaped, m_Table, strlen(m_Table));

		Utils->asprintf(&Query, "REPLACE INTO `%s`\n"
								"         ( `file`, `setting`, `value` )\n"
								"  VALUES ( '%s', '%s', '%s' )",
			TableEscaped, FileEscaped, SettingEscaped, ValueEscaped);

		free(FileEscaped);
		free(SettingEscaped);
		free(ValueEscaped);
		free(TableEscaped);

		if (mysql_query(Mysql, Query) != 0) {
			m_Log->WriteLine(NULL, "MySQL Error: %s", mysql_error(Mysql));

			Utils->Free(Query);

			MysqlModConnect();

			return false;
		}

		Utils->Free(Query);

		return true;
	}

	virtual void Destroy(void) {
		delete this;
	}

	virtual RESULT<int> ReadInteger(const char *Setting) const {
		const char *Value = ReadString(Setting);

		if (Value != NULL) {
			RETURN(int, atoi(Value));
		} else {
			THROW(int, Generic_Unknown, "There is no such setting");
		}

		RETURN(int, 0);
	}

	virtual RESULT<const char *> ReadString(const char *Setting) const {
		FlushWriteQueue();

		if (m_LastFetch < time(NULL) - m_UpdateInterval) {
			InternalReload();
		}

		RETURN(const char *, m_Cache->Get(Setting));
	}

	virtual RESULT<bool> WriteInteger(const char *Setting, const int Value) {
		char *ValueStr;
		const utility_t *Utils;

		Utils = g_Bouncer->GetUtilities();

		Utils->asprintf(&ValueStr, "%d", Value);
		WriteString(Setting, ValueStr);
		Utils->Free(ValueStr);

		RETURN(bool, true);
	}

	virtual RESULT<bool> WriteString(const char *Setting, const char *Value) {
		if (!WriteStringNoQueue(Setting, Value)) {
			m_WriteQueue->Add(Setting, strdup(Value));

			THROW(bool, Generic_Unknown, "mysql query failed");
		}

		RETURN(bool, true);
	}

	virtual const char *GetFilename(void) const {
		return m_Filename;
	}

	bool InternalReload(void) const {
		const utility_t *Utils;
		char *FileEscaped, *TableEscaped, *Query;
		MYSQL *Mysql;
		MYSQL_RES *Result;
		MYSQL_ROW Row;

		Mysql = MysqlModGetConnection();

		if (Mysql == NULL) {
			Mysql = MysqlModConnect();

			if (Mysql == NULL) {
				return false;
			}
		}

		Utils = g_Bouncer->GetUtilities();

		FileEscaped = (char *)malloc(strlen(m_Filename) * 2 + 1);
		TableEscaped = (char *)malloc(strlen(m_Table) * 2 + 1);

		mysql_real_escape_string(Mysql, FileEscaped, m_Filename, strlen(m_Filename));
		mysql_real_escape_string(Mysql, TableEscaped, m_Table, strlen(m_Table));

		Utils->asprintf(&Query, "SELECT `setting`, `value` FROM `%s` WHERE `file`='%s'",
			 TableEscaped, FileEscaped);

		free(FileEscaped);
		free(TableEscaped);

		if (mysql_query(Mysql, Query) != 0) {
			m_Log->WriteLine(NULL, "MySQL Error: %s", mysql_error(Mysql));

			Utils->Free(Query);

			MysqlModConnect();

			return false;
		}

		Utils->Free(Query);

		Result = mysql_use_result(Mysql);

		if (Result == NULL) {
			m_Log->WriteLine(NULL, "MySQL Error: %s", mysql_error(Mysql));

			MysqlModConnect();

			return false;
		}

		while ((Row = mysql_fetch_row(Result)) != NULL) {
			m_Cache->Add(Row[0], strdup(Row[1]));
		}

		mysql_free_result(Result);

		time(&m_LastFetch);

		return true;
	}

	virtual void Reload(void) {
		InternalReload();
	}

	virtual CHashtable<char *, false, 16> *GetInnerHashtable(void) {
		return m_Cache;
	}

	virtual hash_t<char *> *Iterate(int Index) const {
		return m_Cache->Iterate(Index);
	}

	virtual unsigned int GetLength(void) const {
		return m_Cache->GetLength();
	}

	virtual bool CanUseCache(void) {
		return false;
	}
};

bool ResetCacheTimer(time_t Now, void *Null) {
	if (MysqlModGetConnection() == NULL) {
		MysqlModConnect();
	}

	return true;
}

class CMysqlConfigModule : public CConfigModuleFar {
	CConfig *m_BootstrapConfig;
	MYSQL *m_Mysql;
	const char *m_Table;
	CLog *m_Log;
	CTimer *m_CacheResetTimer;
	unsigned int m_UpdateInterval;

public:
	CMysqlConfigModule(void) {
		m_BootstrapConfig = NULL;
		m_CacheResetTimer = NULL;
	}

	virtual void Destroy(void) {
		delete m_CacheResetTimer;
		delete m_Log;
		mysql_close(m_Mysql);
	}

	void Init(CCore *Root) {
		m_Log = new CLog("mysql.log");

		g_NextConnect = 0;

		m_CacheResetTimer = new CTimer(300, 1, ResetCacheTimer, NULL);

		g_Bouncer = Root;
		m_BootstrapConfig = Root->GetConfig();

		m_Mysql = NULL;

		if (Connect() == NULL) {
			g_Bouncer->Fatal();
		}
	}

	MYSQL *Connect(void) {
		char *TableEscaped, *Query;
		const utility_t *Utils;
		MYSQL *Mysql;

		Mysql = mysql_init(NULL);
		mysql_options(Mysql, MYSQL_READ_DEFAULT_GROUP, "sbnc");

		const char *Host = m_BootstrapConfig->ReadString("mysql.host");
		unsigned int Port = m_BootstrapConfig->ReadInteger("mysql.port");
		const char *User = m_BootstrapConfig->ReadString("mysql.user");
		const char *Password = m_BootstrapConfig->ReadString("mysql.password");
		const char *Database = m_BootstrapConfig->ReadString("mysql.database");
		m_Table = m_BootstrapConfig->ReadString("mysql.table");
		m_UpdateInterval = m_BootstrapConfig->ReadInteger("mysql.updateinterval");

		if (m_Table == NULL) {
			m_Table = "sbnc_config";
		}

		if (m_UpdateInterval == 0) {
			m_UpdateInterval = 5 * 60;
		}

		MYSQL *Result = mysql_real_connect(Mysql, Host, User, Password, Database, Port, NULL, 0);

		if (Result == NULL) {
			m_Log->WriteLine(NULL, "MySQL Error: %s", mysql_error(Mysql));
			mysql_close(Mysql);
			Mysql = Result;
		} else {
			g_Bouncer->Log("Connected to MySQL server at %s:%d", Host, Port);
		}

		if (Mysql != NULL) {
			Utils = g_Bouncer->GetUtilities();

			TableEscaped = (char *)malloc(strlen(m_Table) * 2 + 1);

			mysql_real_escape_string(Mysql, TableEscaped, m_Table, strlen(m_Table));

			Utils->asprintf(&Query,
				"CREATE TABLE IF NOT EXISTS `%s` (\n"
				"  `file` varchar(128) NOT NULL,\n"
				"  `setting` varchar(128) NOT NULL,\n"
				"  `value` text NOT NULL,\n"
				"  UNIQUE KEY `id` (`file`,`setting`)\n"
				")", TableEscaped
			);

			free(TableEscaped);

			mysql_query(Mysql, Query);

			Utils->Free(Query);
		}

		if (m_Mysql != NULL) {
			mysql_close(m_Mysql);
		}

		m_Mysql = Mysql;

		return Mysql;
	}

	CConfig *CreateConfigObject(const char *Name, CUser *Owner) {
		return new CMysqlConfig(Name, Owner, m_Mysql, m_Table, m_Log, m_UpdateInterval);
	}

	MYSQL *GetConnection(void) {
		return m_Mysql;
	}

	void CheckAndReconnect(void) {
		if (m_Mysql == NULL) {
			Connect();
		}
	}
};

MYSQL *MysqlModConnect(void) {
	if (g_NextConnect < time(NULL)) {
		g_NextConnect = time(NULL) + 30;

		return g_MysqlMod->Connect();
	} else {
		return NULL;
	}
}

MYSQL *MysqlModGetConnection(void) {
	return g_MysqlMod->GetConnection();
}

extern "C" EXPORT CConfigModuleFar *bncGetConfigObject(void) {
	g_MysqlMod = new CMysqlConfigModule();

	return (CConfigModuleFar *)g_MysqlMod;
}

extern "C" EXPORT int bncGetInterfaceVersion(void) {
	return INTERFACEVERSION;
}
