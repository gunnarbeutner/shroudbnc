class CAssocArray;

#define STATUS_RUN 0
#define STATUS_FREEZE 1
#define STATUS_SHUTDOWN 2
#define STATUS_PAUSE 3

typedef bool (*GetBoxProc)(CAssocArray **BoxPtr);
typedef void (*SigEnableProc)(void);
typedef void (*SetModuleProc)(const char *Module);
typedef const char *(*BuildPathProc)(const char *Filename, const char *BasePath);
typedef const char *(*GetModuleProc)(void);

typedef struct loaderparams_s {
	int Version;

	int argc;
	char **argv;
	const char *basepath;

	CAssocArray *Box;

	GetBoxProc GetBox;
	SigEnableProc SigEnable;

	SetModuleProc SetModulePath;
	GetModuleProc GetModulePath;

	BuildPathProc BuildPath;
} loaderparams_t;

#ifndef SBNC
typedef int (*sbncLoad)(loaderparams_s *Parameters);
typedef bool (*sbncSetStatus)(int Status);

extern sbncSetStatus g_SetStatusFunc;
#endif
