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

internalbind command tclient:command
internalbind client tclient:client
internalbind attach tclient:attach
#internalbind server tclient:dccchat "\001DCC CHAT *\001"

proc tclient:command {client params} {
	set command [lindex $params 0]

	if {[string equal -nocase $command "addtelnet"]} {
		set session [lindex $params 1]
		set host [lindex $params 2]
		set port [lindex $params 3]

		if {$port == ""} {
			bncreply "Syntax: addtelnet <Name> <Host> <Port>"

			haltoutput
			return
		}

		if {[tclient:issession $session]} {
			bncreply "A session with that name already exists."

			haltoutput
			return
		}

		tclient:addsession $session $host $port

		bncreply "Done."
		haltoutput
	} elseif {[string equal -nocase $command "deltelnet"]} {
		set session [lindex $params 1]

		if {![tclient:issession $session]} {
			bncreply "There is no such session."

			haltoutput
			return
		}

		tclient:delsession $session

		bncreply "Done."
		haltoutput
	} elseif {[string equal -nocase $command "telnetsessions"]} {
		namespace eval [getns] {
			if {![info exists activesessions]} { set activesessions [list] }
		}

		upvar [getns]::activesessions activesessions

		foreach sessionobj [getbncuser [getctx] tag tsessions] {
			if {[lsearch $activesessions "[lindex $sessionobj 0] *"] == -1} {
				set status "Not active"
			} else {
				set status "Active"
			}

			bncreply "[lindex $sessionobj 0] - [lindex $sessionobj 1]:[lindex $sessionobj 2] - $status"
		}

		bncreply "End of SESSIONS."
		haltoutput
	} elseif {[string equal -nocase $command "help"]} {
		bncaddcommand "addtelnet" "User" "creates a new telnet session" "Syntax: addtelnet <Name> <Host> <Port>\nCreates a new telnet session."
		bncaddcommand "deltelnet" "User" "removes a telnet session" "Syntax: deltelnet <Name>\nRemoves a telnet session."
		bncaddcommand "telnetsessions" "User" "shows a list of telnet sessions" "Syntax: telnetsessions\nLists all telnet sessions."
	}
}

proc tclient:issession {session} {
	set sessions [getbncuser [getctx] tag tsessions]

	if {[lsearch $sessions "$session *"] != -1} {
		return 1
	} else {
		return 0
	}
}

proc tclient:addsession {session host port} {
	set sessionobj [list $session $host $port]
	set sessions [getbncuser [getctx] tag tsessions]

	lappend sessions $sessionobj
	setbncuser [getctx] tag tsessions $sessions

	tclient:checksessions
}

proc tclient:delsession {session} {
	namespace eval [getns] {
		if {![info exists activesessions]} { set activesessions [list] }
	}

	upvar [getns]::activesessions activesessions

	set sessions [getbncuser [getctx] tag tsessions]
	set sessionidx [lsearch $sessions "$session *"]

	if {$sessionidx == -1} { return }

	set sessions [lreplace $sessions $sessionidx $sessionidx]
	setbncuser [getctx] tag tsessions $sessions

	set sessionobj [lsearch -inline $activesessions "$session *"]

	if {$sessionobj != ""} {
		catch [list killdcc [lindex $sessionobj 2]]
	}

	tclient:checksessions
}

proc tclient:checksessions {} {
	namespace eval [getns] {
		if {![info exists activesessions]} { set activesessions [list] }
	}

	upvar [getns]::activesessions activesessions

	foreach sessionobj [getbncuser [getctx] tag tsessions] {
		if {[lsearch $activesessions "[lindex $sessionobj 0] *"] == -1} {
			utimer 5 [list tclient:connect [lindex $sessionobj 0]]
		}
	}

	foreach sessionobj $activesessions {
		if {[lsearch [getbncuser [getctx] tag tsessions] "[lindex $sessionobj 0] *"] == -1} {
			tclient:replyas [lindex $sessionobj 0] "\00303Disconnecting session."
		}
	}
}

proc tclient:replyas {session text} {
	global botnick

	putclient ":-$session!telnet@session PRIVMSG $botnick :$text"
}

proc tclient:connect {session} {
	if {![tclient:issession $session]} { return }

	namespace eval [getns] {
		if {![info exists activesessions]} { set activesessions [list] }
	}

	upvar [getns]::activesessions activesessions

	if {[lsearch $activesessions "$session *"] != -1} { return }

	set sessionobj [lsearch -inline [getbncuser [getctx] tag tsessions] "$session *"]

	if {[lindex $sessionobj 1] == 0} {
		return
	}

	tclient:replyas [lindex $sessionobj 0] "\00303Reconnecting to [lindex $sessionobj 1]:[lindex $sessionobj 2]"

	set connection [connect [lindex $sessionobj 1] [lindex $sessionobj 2]]
	control $connection tclient:line

	lappend activesessions [list $session 0 $connection]

	return
}

proc tclient:disconnect {session} {
	namespace eval [getns] {
		if {![info exists activesessions]} { set activesessions [list] }
	}

	upvar [getns]::activesessions activesessions

	set index [lsearch $activesessions "$session *"]

	if {$index != -1} {
		set sessionobj [lindex $activesessions $index]
		catch [list killdcc [lindex $sessionobj 2]]

		set activesessions [lreplace $activesessions $index $index]

		if {[lindex $sessionobj 1]} {
			tclient:delsession [lindex $sessionobj 0]
		}

		tclient:replyas [lindex $sessionobj 0] "\00303Disconnected from telnet server."
	}

	tclient:checksessions
}

proc tclient:line {idx line} {
	set sessionobj ""

	foreach user [bncuserlist] {
		setctx $user

		namespace eval [getns] {
			if {![info exists activesessions]} { set activesessions [list] }
		}

		upvar [getns]::activesessions activesessions

		set sessionobj [lsearch -inline $activesessions "* $idx"]

		if {$sessionobj != ""} {
			break
		}
	}

	# TODO: remove
	if {$sessionobj == ""} {
		setctx fnords
		bncreply "unknown line: $line"

		return
	}

	if {$line == ""} {
		tclient:disconnect [lindex $sessionobj 0]

		return
	}

	tclient:replyas [lindex $sessionobj 0] [sbnc:telnet2text $line]
}

proc tclient:client {client params} {
	global botnick

	if {[string equal -nocase [lindex $params 0] "PRIVMSG"]} {
		if {[string index [lindex $params 1] 0] != "-"} { return }
		set session [string range [lindex $params 1] 1 end]
		if {![tclient:issession $session]} { return }

		namespace eval [getns] {
			if {![info exists activesessions]} { set activesessions [list] }
		}

		upvar [getns]::activesessions activesessions
		set sessionobj [lsearch -inline $activesessions "$session *"]

		if {$sessionobj == ""} {
			tclient:checksessions
			return
		}

		if {[catch [list putdcc [lindex $sessionobj 2] [lindex $params 2]]]} {
			tclient:disconnect [lindex $sessionobj 0]
		}

		haltoutput
	} elseif {[string equal -nocase [lindex $params 0] "WHOIS"]} {
		if {[string index [lindex $params 1] 0] != "-"} { return }
		set session [string range [lindex $params 1] 1 end]
		if {![tclient:issession $session]} { return }

		set sessionobj [lsearch -inline [getbncuser [getctx] tag tsessions] "$session *"]

		set server [getbncuser [getctx] realserver]

		putclient ":$server 311 $botnick -[lindex $sessionobj 0] [lindex $sessionobj 2] [lindex $sessionobj 1] * :Telnet session"
		putclient ":$server 318 $botnick -[lindex $sessionobj 0] :End of /WHOIS list."

		haltoutput
	}
}

proc tclient:attach {client} {
	namespace eval [getns] {
		if {![info exists activesessions]} { set activesessions [list] }
	}

	upvar [getns]::activesessions activesessions

	foreach sessionobj $activesessions {
		tclient:replyas [lindex $sessionobj 0] "\00303Telnet session is active."
	}

	tclient:checksessions
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

proc tclient:createtempsession {session idx} {
	if {[tclient:issession $session]} { return }

	tclient:addsession $session 0 0

	namespace eval [getns] {
		if {![info exists activesessions]} { set activesessions [list] }
	}

	upvar [getns]::activesessions activesessions

	set index [lsearch $activesessions "$session *"]

	if {$index != -1} { return }

	lappend activesessions [list $session 1 $idx]

	control $idx tclient:line

	tclient:replyas $session "\00303DCC session is active."
}

proc tclient:dccchat {client params} {
	namespace eval [getns] {
		if {![info exists activesessions]} { set activesessions [list] }
	}

	upvar [getns]::activesessions activesessions

	if {![string equal -nocase [lindex $params 1] "PRIVMSG"]} { return }

	set ctcp [lindex $params 3]
	if {[catch [list int2ip [lindex [split $ctcp] 3]] ip]} { return }
	set port [string range [lindex [split $ctcp] 4] 0 end-1]

	tclient:createtempsession [lindex [split [lindex $params 0] "!"] 0]-[llength $activesessions] [connect $ip $port]

	haltoutput
}
