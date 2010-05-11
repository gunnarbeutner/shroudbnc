#define STATUS_RUN 0
#define STATUS_SHUTDOWN 1
#define STATUS_PAUSE 2

bool sbncIsAbsolutePath(const char *Path);
void sbncPathCanonicalize(char *NewPath, const char *Path);
const char *sbncBuildPath(const char *Filename, const char *BasePath);

const char *sbncGetConfigPath(void);
const char *sbncGetModulePath(void);
const char *sbncGetExePath(void);

#ifndef SBNC
/**< main function of the shroudBNC shared object */
typedef int (*sbncLoad)(const char *ModulePath, bool Daemonized, int argc, char **argv);
#endif /* SBNC */
