class CAssocArray;

typedef bool (*GetBoxProc)(CAssocArray **BoxPtr);
typedef void (*SigEnableProc)(void);
typedef void (*SetModuleProc)(const char *Module);
typedef void (*SetAutoReloadProc)(bool Reload);

struct loaderparams_s {
	int Version;

	int argc;
	char **argv;

	CAssocArray *Box;

	GetBoxProc GetBox;
	SigEnableProc SigEnable;
	SetModuleProc SetModule;
	SetAutoReloadProc unused;
};

#ifndef SBNC
typedef int (*sbncLoad)(loaderparams_s *Parameters);
typedef bool (*sbncPrepareFreeze)(void);
#endif
