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

set ::userport 7080

catch [list listen $::userport script sbnc:telnet] error

proc sbnc:telnet {socket} {
	sbnc:telnetbanner $socket
	control $socket sbnc:telnetmsg
}

proc sbnc:telnetbanner {socket} {
	global telnetusers

	putdcc $socket "Welcome to the shroudBNC partyline"
	putdcc $socket "Bot:"
	set telnetusers($socket) [list]
}

proc sbnc:telnet2text {input} {
	set out [list]

	for {set i 0} {$i < [string length $input]} {incr i} {
		set chr [string index $input $i]

		if {[scan $chr "%c"] == 255} {
			incr i 2
		} else {
			lappend out $chr
		}
	}

	return [join $out ""]
}

proc sbnc:telnetmsg {socket line} {
	global telnetusers

	if {$line == ""} { return }

	set line [sbnc:telnet2text $line]
	set word [lindex [split $line] 0]

	set user $telnetusers($socket)

	if {$user == ""} {
		set telnetusers($socket) [list -1 $word]

		putdcc $socket "Username:"
	} elseif {[lindex $user 0] == -1} {
		set telnetusers($socket) [list 0 [lindex $user 1] $word]

		putdcc $socket "Password:"
	} elseif {[lindex $user 0] == 0} {
		setctx [lindex $user 1]

		if {[passwdok [lindex $user 2] $word]} {
			set telnetusers($socket) [list 1 [lindex $user 1] [lindex $user 2]]

			putdcc $socket "You are now logged in as [lindex $user 2]."
		} else {
			putdcc $socket "Wrong username or password. You are being disconnected."
			utimer 1 [list killdcc $socket]
		}
	} else {
		setctx [lindex $user 1]
		set result [sbnc:telnetcmd $socket [lindex $user 1] [lindex $user 2] $line]

		if {$result != ""} {
			putdcc $socket $result
		}
	}
}

proc sbnc:telnetcmd {socket bot user line} {
	set toks [split $line]

	if {[string index $line 0] == "."} {
		set command [string range [lindex [split $line] 0] 1 end]
	} else {
		return ""
	}

	set count [sbnc:callbinds "dcc" $user "" $command $user $socket [join [lrange [split $line] 1 end]]]

	if {$count < 1} {
		return "Not yet implemented."
	} else {
		return ""
	}
}

# dcc commands
bindall dcc n tcl dcc:tcl
bindall dcc - quit dcc:quit

proc dcc:quit {handle idx text} {
	putdcc $idx "Bye."

	utimer 2 [list killdcc $idx]
}

proc dcc:tcl {handle idx text} {
	set ctx [getctx]
	catch {eval $text} result
	setctx $ctx

	if {$result == ""} { set result "<null>" }
	foreach sline [split $result \n] {
		putdcc $idx "( $text ) = $sline"
	}
}
