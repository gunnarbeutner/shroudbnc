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

internalbind server sbnc:bold
internalbind client sbnc:boldcmds

proc sbnc:bold {client parameters} {
	if {[string equal -nocase [lindex $parameters 1] "PRIVMSG"] && [string equal -nocase [getbncuser $client tag "highlight"] "on"]} {
		if {[string first [string tolower $::botnick] [string tolower [lindex $parameters 3]]] != -1} {
			haltoutput
			putclient ":[lindex $parameters 0] PRIVMSG [lindex $parameters 2] :[string map -nocase "$::botnick \002${::botnick}\002" [lindex $parameters 3]]"
		}
	}
}

proc sbnc:boldcmds {client parameters} {
	if {[string equal -nocase "highlight" [lindex $parameters 0]]} {
		if {[llength $parameters] < 2} {
			bncnotc "Current highlight mode: [getbncuser $client tag "highlight"]"
		} else {
			setbncuser $client tag "highlight" [lindex $parameters 1]
			bncnotc "Set highlight mode: [lindex $parameters 1]"
		}

		haltoutput
	}
}
