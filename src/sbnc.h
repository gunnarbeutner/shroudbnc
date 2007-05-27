#define STATUS_RUN 0
#define STATUS_FREEZE 1
#define STATUS_SHUTDOWN 2
#define STATUS_PAUSE 3

const char *sbncGetBaseName(void);
bool sbncIsAbsolutePath(const char *Path);
void sbncPathCanonicalize(char *NewPath, const char *Path);
const char *sbncBuildPath(const char *Filename, const char *BasePath = NULL);
const char *sbncGetModulePath(void);

#ifndef SBNC
/**< main function of the shroudBNC shared object */
typedef int (*sbncLoad)(const char *ModulePath, bool LPC, int argc, char **argv);
#endif
