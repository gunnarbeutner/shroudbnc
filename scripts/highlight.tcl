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

# This script demonstrates how to add own settings (/sbnc set) and modify
# the ircd's replies. Once enabled it will replace your own nick with
# \2YourNick\2 in any PRIVMSGs you receive from the server, thus effectively
# highlighting your nick in the other users' messages.

internalbind server highlight:server
internalbind command highlight:command

proc highlight:server {client parameters} {
	if {[string equal -nocase [lindex $parameters 1] "PRIVMSG"] && [getbncuser $client tag "highlight"] == "1"} {
		if {[string first [string tolower $::botnick] [string tolower [lindex $parameters 3]]] != -1} {
			haltoutput
			putclient ":[lindex $parameters 0] PRIVMSG [lindex $parameters 2] :[string map -nocase "$::botnick \002${::botnick}\002" [lindex $parameters 3]]"
		}
	}
}

proc highlight:command {client parameters} {
	if {[string equal -nocase [lindex $parameters 0] "set"]} {
		if {[llength $parameters] < 3} {
			if {[getbncuser $client tag highlight] == "1"} {
				set highlight "On"
			} else {
				set highlight "Off"
			}

			utimer 0 [list bncreply "highlight - $highlight"]

			return
		}

		if {[string equal -nocase [lindex $parameters 1] "highlight"]} {
			if {[string equal -nocase [lindex $parameters 2] "on"]} {
				set highlight 1
			} elseif {[string equal -nocase [lindex $parameters 2] "off"]} {
				set highlight 0
			} else {
				bncreply "Value must be either 'on' or 'off'."
				haltoutput

				return
			}

			setbncuser $client tag highlight $highlight
			bncreply "Done."
			haltoutput
		}
	}
}
