/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2014 Gunnar Beutner                                      *
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

#include "StdAfx.h"

CCore *g_Bouncer = NULL;

static int g_ArgC;
static char **g_ArgV;
static char *g_ConfigPath, *g_LogPath, *g_DataPath, *g_PidPath;

void sbncSetConfigPath(const char *path) {
	if (g_ConfigPath != NULL)
		free(g_ConfigPath);

	path = sbncBuildPath(path);

	g_ConfigPath = strdup(path);

	if (g_ConfigPath == NULL) {
		perror("strdup failed");

		exit(EXIT_FAILURE);
	}
}

const char *sbncGetConfigPath(void) {
	assert(g_ConfigPath);
	return g_ConfigPath;
}

void sbncSetLogPath(const char *path) {
	if (g_LogPath != NULL)
		free(g_LogPath);

	path = sbncBuildPath(path);

	g_LogPath = strdup(path);

	if (g_LogPath == NULL) {
		perror("strdup failed");

		exit(EXIT_FAILURE);
	}
}

const char *sbncGetLogPath(void) {
	assert(g_LogPath);
	return g_LogPath;
}

void sbncSetDataPath(const char *path) {
	if (g_DataPath != NULL)
		free(g_DataPath);

	path = sbncBuildPath(path);

	g_DataPath = strdup(path);

	if (g_DataPath == NULL) {
		perror("strdup failed");

		exit(EXIT_FAILURE);
	}
}

const char *sbncGetDataPath(void) {
	assert(g_DataPath);
	return g_DataPath;
}

void sbncSetPidPath(const char *path) {
	if (g_PidPath != NULL)
		free(g_PidPath);

	path = sbncBuildPath(path);

	g_PidPath = strdup(path);

	if (g_PidPath == NULL) {
		perror("strdup failed");

		exit(EXIT_FAILURE);
	}
}

const char *sbncGetPidPath(void) {
	assert(g_PidPath);
	return g_PidPath;
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
		perror("strdup failed");

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

	const char *RelativeModulePath = "../lib/sbnc";

	ModulePath = strdup(sbncBuildPath(RelativeModulePath, sbncGetExePath()));

	if (ModulePath == NULL) {
		perror("strdup failed");

		exit(EXIT_FAILURE);
	}

	return ModulePath;
}

const char *sbncGetSharedPath(void) {
	static const char *SharedPath;

	if (SharedPath != NULL) {
		return SharedPath;
	}

	const char *RelativeSharedPath = "../share/sbnc";

	SharedPath = strdup(sbncBuildPath(RelativeSharedPath, sbncGetExePath()));

	if (SharedPath == NULL) {
		perror("strdup failed");

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

const char *sbncBuildPath(const char *Filename, const char *RelativeTo) {
	static char *Path = NULL;
	size_t Len;

	if (sbncIsAbsolutePath(Filename)) {
		return Filename;
	}

	if (RelativeTo == NULL) {
		char Cwd[MAXPATHLEN];

		if (getcwd(Cwd, sizeof(Cwd)) == NULL) {
			perror("getcwd() failed");
			exit(EXIT_FAILURE);
		}

		RelativeTo = Cwd;
	}

	free(Path);

	asprintf(&Path, "%s/%s", RelativeTo, Filename);

	if (AllocFailed(Path)) {
		return NULL;
	}

#ifdef _WIN32
	for (unsigned int i = 0; Path[i] != '\0'; i++) {
		if (Path[i] == '/') {
			Path[i] = '\\';
		}
	}
#endif

	return Path;
}

char *sbncFindConfigDir(void) {
	char *ConfigPath;
#ifndef _WIN32
	char *HomeDir;
	struct stat StatBuf;

	// Try the current working dir first...
	if (lstat("sbnc.conf", &StatBuf) == 0) {
		return strdup(".");
	}

	HomeDir = getenv("HOME");

	if (HomeDir == NULL) {
		return NULL;
	}

	// legacy config location
	if (asprintf(&ConfigPath, "%s/sbnc/sbnc.conf", HomeDir) < 0) {
		perror("asprintf failed");

		exit(EXIT_FAILURE);
	}

	if (lstat(ConfigPath, &StatBuf) == 0) {
		char *ConfigPath2;

		ConfigPath2 = strdup(dirname(ConfigPath));

		free(ConfigPath);

		return ConfigPath2;
	}

	if (asprintf(&ConfigPath, "%s/.sbnc", HomeDir) < 0) {
		perror("asprintf failed");

		exit(EXIT_FAILURE);
	}
#else
	TCHAR AppDataLocation[MAX_PATH];
	SHGetSpecialFolderPath(NULL, AppDataLocation, CSIDL_APPDATA, FALSE);

	asprintf(&ConfigPath, "%s\\shroudBNC", AppDataLocation);
#endif

	return ConfigPath;
}

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
	bool Daemonize, Usage;
	char *ConfigDir = NULL, *LogDir = NULL, *DataDir = NULL, *PidPath = NULL;

	g_ArgC = argc;
	g_ArgV = argv;

	sbncGetExePath(); // first call sets exe path in static var

	Daemonize = true;
	Usage = false;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--config") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Missing parameter for --config: You need to specify the config directory.");

				return EXIT_FAILURE;
			}

			ConfigDir = argv[i + 1];

			i++;

			continue;
		}

		if (strcmp(argv[i], "--log") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Missing parameter for --log: You need to specify the log directory.");

				return EXIT_FAILURE;
			}

			LogDir = argv[i + 1];

			i++;

			continue;
		}

		if (strcmp(argv[i], "--data") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Missing parameter for --log: You need to specify the log directory.");

				return EXIT_FAILURE;
			}

			DataDir = argv[i + 1];

			i++;

			continue;
		}

		if (strcmp(argv[i], "--pid") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr, "Missing parameter for --pid: You need to specify the PID path.");

				return EXIT_FAILURE;
			}

			PidPath = argv[i + 1];

			i++;

			continue;
		}

		if (strcmp(argv[i], "--lpc") == 0) {
			fprintf(stderr, "Warning: Ignoring --lpc (which is now the default behaviour.)\n");

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

	bool WasNull = false;

	if (ConfigDir == NULL) {
		WasNull = true;
		ConfigDir = sbncFindConfigDir();
	}

	if (mkdir(ConfigDir) < 0 && errno != EEXIST) {
		fprintf(stderr, "Config directory (%s) could not be created: %s\n", ConfigDir, strerror(errno));
		free(ConfigDir);

		return EXIT_FAILURE;
	}

	sbncSetConfigPath(ConfigDir);

	if (LogDir)
		sbncSetLogPath(LogDir);
	else if (DataDir)
		sbncSetLogPath(DataDir);
	else
		sbncSetLogPath(ConfigDir);

	if (DataDir)
		sbncSetDataPath(DataDir);
	else
		sbncSetDataPath(ConfigDir);

	if (PidPath)
		sbncSetPidPath(PidPath);
	else
		sbncSetPidPath(sbncBuildPath("sbnc.pid", sbncGetDataPath()));

	if (WasNull)
		free(ConfigDir);

	if (chdir(sbncGetDataPath()) < 0) {
		fprintf(stderr, "Could not chdir() into data directory (%s): %s\n", sbncGetDataPath(), strerror(errno));
		free(ConfigDir);
 
		return EXIT_FAILURE;
	}

	fprintf(stderr, "shroudBNC (version: " BNCVERSION ") - an object-oriented IRC bouncer\n");
	fprintf(stderr, "Configuration directory: %s\n", sbncGetConfigPath());
	fprintf(stderr, "Log directory: %s\n", sbncGetLogPath());
	fprintf(stderr, "Data directory: %s\n", sbncGetDataPath());
	fprintf(stderr, "PID path: %s\n", sbncGetPidPath());

	if (Usage) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Syntax: %s [OPTION]", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "\t--help\t\t\tdisplay this help and exit\n");
		fprintf(stderr, "\t--foreground\t\trun in the foreground\n");
		fprintf(stderr, "\t--config <config dir>\tspecifies the location of the configuration directory.\n");
		fprintf(stderr, "\t--data <data dir>\tspecifies the location of the data directory (defaults to the config directory).\n");
		fprintf(stderr, "\t--log <log dir>\tspecifies the location of the log directory (defaults to the data directory if given, else the config directory).\n");
		fprintf(stderr, "\t--pid <pid path>\tspecifies the location of the PID file (defaults to <data dir>/sbnc.pid).\n");

		return 3;
	}

#ifdef _WIN32
	if (!GetEnvironmentVariable("TCL_LIBRARY", TclLibrary, sizeof(TclLibrary)) || strlen(TclLibrary) == 0) {
		SetEnvironmentVariable("TCL_LIBRARY", "./tcl8.4");
	}
#endif

	srand((unsigned int)time(NULL));

#ifndef _WIN32
	if (getuid() == 0 || geteuid() == 0 || getgid() == 0 || getegid() == 0) {
		printf("You cannot run shroudBNC as 'root' or using an account which has 'wheel' "
			"privileges. Use an ordinary user account and remove the suid bit if it is set.\n");
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
