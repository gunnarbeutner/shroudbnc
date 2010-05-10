# shroudBNC - an object-oriented framework for IRC
# Copyright (C) 2005-2007,2010 Gunnar Beutner
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# iface commands:
# +user:
# sendmessage text
# +admin
# setcontactusers users
# getcontactusers
proc iface-contact:sendmessage {text} {
	set contactusers [split [string tolower [bncgetglobaltag contactusers]]]

	set wholetext "Message from user [getctx]: $text"

	foreach user [bncuserlist] {
		if {([lsearch -exact $contactusers [string tolower $user]] || [string equal -nocase [lindex $contactusers 0] "all"]) && [getbncuser $user admin]} {
			setctx $user

			if {[getbncuser $user hasclient]} {
				bncnotc $wholetext
			} else {
				putlog $wholetext
			}
		}
	}
}

if {[bncgetglobaltag contactusers] == ""} {
	bncsetglobaltag contactusers "all"
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "contact" "sendmessage" "iface-contact:sendmessage"
}

proc iface-contact:setcontactusers {users} {
	bncsetglobaltag contactusers [split $users]

	return ""
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "contact" "setcontactusers" "iface-contact:setcontactusers" "access:admin"
}

proc iface-contact:getcontactusers {} {
	return [itype_list_strings [split [bncgetglobaltag contactusers]]]
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "contact" "getcontactusers" "iface-contact:getcontactusers" "access:admin"
}
