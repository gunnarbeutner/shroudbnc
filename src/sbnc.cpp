/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
 *                                                                             *
 * This program is free software; you can redistribute it and/or               *
 * modify it under the terms of the GNU General Public License                 *
 * as published by the Free Software Foundation; either version 2              *
 * of the License, or (at your option) any later version.                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *******************************************************************************/

#ifndef SBNC
#define SBNC
#endif

#include "StdAfx.h"

CCore *g_Bouncer = NULL;

static int g_ArgC;
static char **g_ArgV;

#if defined(IPV6) && defined(__MINGW32__)
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
#endif /* defined(IPV6) && defined(__MINGW32__) */

const char *sbncGetConfigPath(void) {
	static char *ConfigPath;

	if (ConfigPath != NULL) {
		return ConfigPath;
	}

	ConfigPath = (char *)malloc(MAXPATHLEN);

	if (ConfigPath == NULL) {
		fprintf(stderr, "Out of memory.\n");
	}

	if (getcwd(ConfigPath, MAXPATHLEN) == NULL) {
		fprintf(stderr, "Could not determine current working directory.\n");
		exit(EXIT_FAILURE);
	}

	return ConfigPath;
}

const char *sbncGetExePath(void) {
	static char *ExePath;

	if (ExePath != NULL) {
		return ExePath;
	}

#ifndef _WIN32
	char Buf[MAXPATHLEN], Cwd[MAXPATHLEN];
	char *PathEnv, *Directory, PathTest[MAXPATHLEN], FullExePath[MAXPATHLEN];
	bool FoundPath;

	if (getcwd(Cwd, sizeof(Cwd)) == NULL) {
		perror("getcwd() failed");
		exit(EXIT_FAILURE);
	}

	if (g_ArgV[0][0] != '/') {
		snprintf(FullExePath, sizeof(FullExePath), "%s/%s", Cwd, g_ArgV[0]);
	} else {
		strmcpy(FullExePath, g_ArgV[0], sizeof(FullExePath));
	}

	if (strchr(g_ArgV[0], '/') == NULL) {
		PathEnv = getenv("PATH");

		if (PathEnv != NULL) {
			PathEnv = strdup(PathEnv);

			if (PathEnv == NULL) {
				perror("strdup() failed");
				exit(EXIT_FAILURE);
			}

			FoundPath = false;

			for (Directory = strtok(PathEnv, ":"); Directory != NULL; Directory = strtok(NULL, ":")) {
				if (snprintf(PathTest, sizeof(PathTest), "%s/%s", Directory, g_ArgV[0]) < 0) {
					perror("snprintf() failed");
					exit(EXIT_FAILURE);
				}

				if (access(PathTest, X_OK) == 0) {
					strmcpy(FullExePath, PathTest, sizeof(FullExePath));

					FoundPath = true;

					break;
				}
			}

			free(PathEnv);

			if (!FoundPath) {
				printf("Could not determine executable path.\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	if (realpath(FullExePath, Buf) == NULL) {
		perror("realpath() failed");
		exit(EXIT_FAILURE);
	}

	// remove filename
	char *LastSlash = strrchr(Buf, '/');
	if (LastSlash != NULL) {
		*LastSlash = '\0';
	}

	ExePath = strdup(Buf);

	if (ExePath == NULL) {
		perror("strdup() failed");
		exit(EXIT_FAILURE);
	}
#else /* _WIN32 */
	ExePath = (char *)malloc(MAXPATHLEN);

	if (ExePath == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(EXIT_FAILURE);
	}

	GetModuleFileName(NULL, ExePath, MAXPATHLEN);

	PathRemoveFileSpec(ExePath);
#endif /* _WIN32 */

	return ExePath;
}

const char *sbncGetModulePath(void) {
	static const char *ModulePath;

	if (ModulePath != NULL) {
		return ModulePath;
	}

#ifdef _WIN32
	const char *RelativeModulePath = ".";
#else /* _WIN32 */
	const char *RelativeModulePath = "../lib/sbnc";
#endif /* _WIN32 */

	ModulePath = strdup(sbncBuildPath(RelativeModulePath, sbncGetExePath()));

	if (ModulePath == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(EXIT_FAILURE);
	}

	return ModulePath;
}

const char *sbncGetSharedPath(void) {
	static const char *SharedPath;

	if (SharedPath != NULL) {
		return SharedPath;
	}

#ifdef _WIN32
	const char *RelativeSharedPath = ".";
#else /* _WIN32 */
	const char *RelativeSharedPath = "../share/sbnc";
#endif /* _WIN32 */

	SharedPath = strdup(sbncBuildPath(RelativeSharedPath, sbncGetExePath()));

	if (SharedPath == NULL) {
		fprintf(stderr, "Out of memory.\n");
		exit(EXIT_FAILURE);
	}

	return SharedPath;
}

bool sbncIsAbsolutePath(const char *Path) {
#ifdef _WIN32
	return !PathIsRelative(Path);
#else
	if (Path[0] == '/') {
		return true;
	}

	return false;
#endif
}

void sbncPathCanonicalize(char *NewPath, const char *Path) {
	int i = 0, o = 0;

	while (true) {
		if ((Path[i] == '\\' || Path[i] == '/') && Path[i + 1] == '.' && Path[i + 1] != '\0' && Path[i + 2] != '.') {
			i += 2;
		}

		if (o >= MAXPATHLEN - 1) {
			NewPath[o] = '\0';

			return;
		} else {
			NewPath[o] = Path[i];
		}

		if (Path[i] == '\0') {
			return;
		}

		i++;
		o++;
	}
}

const char *sbncBuildPath(const char *Filename, const char *ExePath) {
	char NewPath[MAXPATHLEN];
	static char *Path = NULL;
	size_t Len;

	if (ExePath == NULL) {
		return Filename;
	}

	if (sbncIsAbsolutePath(Filename)) {
		return Filename;
	}

	free(Path);

	Len = strlen(ExePath) + 1 + strlen(Filename) + 1;
	Path = (char *)malloc(Len);
	strncpy(Path, ExePath, Len);
#ifdef _WIN32
	strncat(Path, "\\", Len);
#else
	strncat(Path, "/", Len);
#endif
	strncat(Path, Filename, Len);

#ifdef _WIN32
	for (unsigned int i = 0; i < strlen(Path); i++) {
		if (Path[i] == '/') {
			Path[i] = '\\';
		}
	}
#endif

	sbncPathCanonicalize(NewPath, Path);
	
	strncpy(Path, NewPath, Len);
	Path[Len - 1] = '\0';

	return Path;
}

#ifndef _WIN32

void Socket_Init(void) {}
void Socket_Final(void) {}

#else

void Socket_Init(void) {
	WSADATA wsaData;

	WSAStartup(MAKEWORD(1,1), &wsaData);
}

void Socket_Final(void) {
	WSACleanup();
}

#endif

/**
 * main
 *
 * Entry point for shroudBNC.
 */
int main(int argc, char **argv) {
#ifdef _WIN32
	char TclLibrary[512];
#endif
	CConfig *Config;
	bool Daemonize, Usage, ExplicitConfigDirectory;

	g_ArgC = argc;
	g_ArgV = argv;

#if defined(_WIN32) && defined(_DEBUG)
	Sleep(10000);
#endif

	Daemonize = true;
	Usage = false;
	ExplicitConfigDirectory = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--config") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Missing parameter for --config: You need to specify a config directory.");
				return EXIT_FAILURE;
			}

			ExplicitConfigDirectory = true;

			if (chdir(argv[i + 1]) < 0) {
				fprintf(stderr, "Could not chdir() into config directory '%s': %s\n", argv[i + 1], strerror(errno));
				return EXIT_FAILURE;
			}

			i++;

			continue;
		}

		if (strcmp(argv[i], "--lpc") == 0) {
			fprintf(stderr, "Warning: Ignoring --lpc (which is now the default behaviour.\n");

			continue;
		}

		if (strcmp(argv[i], "--foreground") == 0) {
			Daemonize = false;

			continue;
		}

		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "/?") == 0) {
			Usage = true;

			continue;
		}

		fprintf(stderr, "Unknown command-line option: '%s'\n", argv[i]);
		return EXIT_FAILURE;
	}

	if (!ExplicitConfigDirectory) {
		/* TODO: figure out config directory, in this order:
			* $HOME/.sbnc
			* $HOME/sbnc
			* cwd
		   fall back to using
			* $HOME/.sbnc
		   if no config dir was found.
		*/
	}

	sbncGetConfigPath(); // first call sets config path to cwd

	fprintf(stderr, "shroudBNC (version: " BNCVERSION ") - an object-oriented IRC bouncer\n");
	fprintf(stderr, "Configuration directory: %s\n", sbncGetConfigPath());

	if (Usage) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Syntax: %s [OPTION]", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "\t--help\t\tdisplay this help and exit\n");
		fprintf(stderr, "\t--foreground\trun in the foreground\n");

		return 3;
	}

#ifdef _WIN32
	if (!GetEnvironmentVariable("TCL_LIBRARY", TclLibrary, sizeof(TclLibrary)) || strlen(TclLibrary) == 0) {
		SetEnvironmentVariable("TCL_LIBRARY", "./tcl8.4");
	}
#endif

	srand((unsigned int)time(NULL));

#if defined(_DEBUG) && defined(_WIN32)
	SetUnhandledExceptionFilter(GuardPageHandler);

	SymInitialize(GetCurrentProcess(), NULL, TRUE);
#endif

#ifndef _WIN32
	if (getuid() == 0 || geteuid() == 0 || getgid() == 0 || getegid() == 0) {
		printf("You cannot run shroudBNC as 'root' or using an account which has 'wheel' privileges. Use an ordinary user account and remove the suid bit if it is set.\n");
		return EXIT_FAILURE;
	}

	rlimit core_limit = { INT_MAX, INT_MAX };
	setrlimit(RLIMIT_CORE, &core_limit);
#endif

#if !defined(_WIN32 ) || defined(__MINGW32__)
	lt_dlinit();
#endif

	time(&g_CurrentTime);

	Config = new CConfig(sbncBuildPath("sbnc.conf", NULL), NULL);

	if (Config == NULL) {
		printf("Fatal: could not create config object.");

#if !defined(_WIN32 ) || defined(__MINGW32__)
		lt_dlexit();
#endif

		return EXIT_FAILURE;
	}

	// constructor sets g_Bouncer
	new CCore(Config, argc, argv);

#if !defined(_WIN32)
	signal(SIGPIPE, SIG_IGN);
#endif

	g_Bouncer->StartMainLoop(Daemonize);

	delete g_Bouncer;
	delete Config;

#if !defined(_WIN32 ) || defined(__MINGW32__)
	lt_dlexit();
#endif

	exit(EXIT_SUCCESS);
}
