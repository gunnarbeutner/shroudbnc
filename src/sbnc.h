class CAssocArray;

typedef bool (*GetBoxProc)(CAssocArray **BoxPtr);
typedef void (*SigEnableProc)(void);
typedef void (*SetModuleProc)(const char *Module);
typedef void (*SetAutoReloadProc)(bool Reload);
typedef const char *(*BuildPathProc)(const char *Filename, const char *BasePath);
typedef const char *(*GetModuleProc)(void);

typedef struct loaderparams_s {
	int Version;

	int argc;
	char **argv;

	CAssocArray *Box;

	GetBoxProc GetBox;
	SigEnableProc SigEnable;
	SetModuleProc SetModule;
	SetAutoReloadProc unused;

	const char *basepath;

	BuildPathProc BuildPath;
	GetModuleProc GetModule;
} loaderparams_t;

#ifndef SBNC
typedef int (*sbncLoad)(loaderparams_s *Parameters);
typedef bool (*sbncPrepareFreeze)(void);
#endif
