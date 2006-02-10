class CAssocArray;

typedef bool (*GetBoxProc)(CAssocArray **BoxPtr);
typedef void (*SigEnableProc)(void);
typedef void (*SetModuleProc)(const char *Module);
typedef void (*SetAutoReloadProc)(bool Reload);
typedef const char *(*BuildPath)(const char *Filename);

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

	BuildPath BuildPath;
} loaderparams_t;

#ifndef SBNC
typedef int (*sbncLoad)(loaderparams_s *Parameters);
typedef bool (*sbncPrepareFreeze)(void);
#endif
