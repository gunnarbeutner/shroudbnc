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

/**
 * loaderparams_t
 *
 * Parameters which are passed to the shroudBNC module
 * by the loader.
 */
typedef struct loaderparams_s {
	int Version; /**< the version of this structure */

	int argc; /**< the number of arguments for the shroudBNC module */
	char **argv; /**< the arguments for the shroudBNC module */
	const char *basepath; /**< the path of shroudBNC */

	CAssocArray *Box; /**< the box which is used for storing persisted data */

	GetBoxProc GetBox; /**< used for getting the root box from the loader */
	SigEnableProc SigEnable; /**< used for re-enabling signals */

	SetModuleProc SetModulePath; /**< sets the module path */
	GetModuleProc GetModulePath; /**< gets the module path */

	BuildPathProc BuildPath;
} loaderparams_t;

#ifndef SBNC
/**< main function of the shroudBNC shared object */
typedef int (*sbncLoad)(loaderparams_s *Parameters);

/**< modifies the status code */
typedef bool (*sbncSetStatus)(int Status);

extern sbncSetStatus g_SetStatusFunc;
#endif
