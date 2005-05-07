/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005 Gunnar Beutner                                           *
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

char* LastArg(char* Args);

class CIRCConnection;
class CBouncerUser;

const char* ArgParseServerLine(const char* Data);
const char* ArgTokenize(const char* Data);
const char** ArgToArray(const char* Args);
void ArgFree(const char* Args);
void ArgFreeArray(const char** Array);
const char* ArgGet(const char* Args, int Arg);
int ArgCount(const char* Args);

SOCKET SocketAndConnect(const char* Host, unsigned short Port, const char* BindIp = NULL);
CIRCConnection* CreateIRCConnection(const char* Host, unsigned short Port, CBouncerUser* Owning, const char* BindIp = NULL);
SOCKET CreateListener(unsigned short Port);

char* NickFromHostmask(const char* Hostmask);
void string_free(char* string);

#define BNCVERSION "0.7"
