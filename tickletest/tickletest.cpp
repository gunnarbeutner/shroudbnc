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

#include <stdlib.h>
#include <assert.h>
#include <tcl.h>

int Tcl_MyProc(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]) {
	const char* s = Tcl_GetStringFromObj(objv[1], NULL);

	Tcl_DString string;

	Tcl_DStringInit(&string);

	Tcl_UtfToExternalDString(NULL, s, -1, &string);

	printf("%s\n", Tcl_DStringValue(&string));

	// string sollte eigentlich "\x95" sein, aber es ist "\xC2\x95"
	// string should actually be "\x95", but it's "\xC2\x95"
	assert(*Tcl_DStringValue(&string) == '\x95');

	Tcl_DStringFree(&string);

	return 0;
}

int main(int argc, char* argv[]) {
	Tcl_DString script;

	Tcl_FindExecutable(argv[0]);

	Tcl_SetSystemEncoding(NULL, "ISO8859-1");

	Tcl_Interp* p = Tcl_CreateInterp();

	Tcl_DStringInit(&script);

	Tcl_CreateObjCommand(p, "moo", Tcl_MyProc, NULL, NULL);

	char* buf = "moo \"hello world\\x95\"";

	Tcl_ExternalToUtfDString(NULL, buf, -1, &script);
	Tcl_Eval(p, Tcl_DStringValue(&script));

	Tcl_DeleteInterp(p);

	return 0;
}
