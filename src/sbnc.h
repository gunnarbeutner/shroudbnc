/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2011 Gunnar Beutner                                      *
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

bool sbncIsAbsolutePath(const char *Path);
void sbncPathCanonicalize(char *NewPath, const char *Path);
const char *sbncBuildPath(const char *Filename, const char *BasePath);

const char *sbncGetConfigPath(void);
const char *sbncGetModulePath(void);
const char *sbncGetSharedPath(void);
const char *sbncGetExePath(void);

#ifndef SBNC
/**< main function of the shroudBNC shared object */
typedef int (*sbncLoad)(const char *ModulePath, bool Daemonized, int argc, char **argv);
#endif /* SBNC */
