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

catch {listen 8090 script sbnc:iface}

proc sbnc:iface {socket} {
	control $socket sbnc:ifacemsg
}

proc sbnc:ifacemsg {socket line} {
	set toks [split $line]

	set code [lindex $toks 0]
	set account [lindex $toks 1]
	set pass [lindex $toks 2]
	set command [lindex $toks 3]
	set params [lrange $toks 4 end]

#	setctx fnords
#	puthelp "PRIVMSG #shroudtest2 :$socket $line"

	if {![bnccheckpassword $account $pass] || ![getbncuser $account admin]} {
		putdcc $socket "$code 105 Access denied."

		return
	}

	setctx $account

	set result ""

	switch -- $command {
		"tcl" {
			if {[catch [join $params] result] != 0} {
				set result [lindex [split $result \n] 0]
			}
		}
		"checkpw" {
			set result [bnccheckpassword [lindex $params 0] [lindex $params 1]]
		}
		"raw" {
			setctx [lindex $params 0]
			puthelp [join [lrange $params 1 end]]
		}
	}

	putdcc $socket "$code $result"
}
