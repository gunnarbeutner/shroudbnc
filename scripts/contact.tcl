# shroudBNC - an object-oriented framework for IRC
# Copyright (C) 2005 Gunnar Beutner
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

# options
set ::contactuser "all"

if {[lsearch -exact [info commands] "registerifacehandler"] != -1} {
	registerifacehandler contact contact:ifacecmd
}

# iface commands:
# +user:
# sendmessage text
proc contact:ifacecmd {command params account} {
	global contactuser

	if {[string equal -nocase $command "sendmessage"]} {
		set user [lindex $params 0]

		set text "Message from user $account: [join $params]"

		foreach u [bncuserlist] {
			if {[lsearch -exact $contactuser $u] != -1 || ([string equal -nocase $contactuser "all"]) && [getbncuser $u admin]} {
				setctx $u

				foreach line [split $text \5] {
					if {[getbncuser $u hasclient]} {
						bncnotc $line
					} else {
						putlog $line
					}
				}
			}
		}
	}
}
