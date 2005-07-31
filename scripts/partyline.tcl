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

internalbind client sbnc:partyline
internalbind attach sbnc:partyattach
internalbind detach sbnc:partydetach
internalbind usrdelete sbnc:partysync

set ::partyline [list &partyline &test]

if {![info exists ::partytopic]} {
	foreach chan $::partyline {
		set ::partytopic($chan) "shroudBNC Partyline"
		set ::partyts($chan) [unixtime]
		set ::partywho($chan) "-sBNC"
	}
}

proc sbnc:partyline {client parameters} {
	setctx $client

	global botnick botname server partyline partytopic partyts partywho

	if {[lsearch [string tolower $partyline] [string tolower [lindex $parameters 1]]] == -1} { return }

	haltoutput

	set cmd [lindex $parameters 0]
	set chan [string tolower [lindex $parameters 1]]
	set serv [lindex [split $server ":"] 0]
	set chans [split [string tolower [getbncuser $client tag partyline]] ","]

	if {[string equal -nocase "join" $cmd]} {
		if {[lsearch $chans $chan] == -1} {
			lappend chans $chan
			setbncuser $client tag partyline [join $chans ","]

			putclient ":$botname JOIN $chan"
			sbnc:partyline $client "NAMES $chan"
			sbnc:partyline $client "TOPIC $chan"

			sbnc:bcpartybutone $client ":\$$client!$client@sbnc JOIN $chan"
		}
	}

	if {[string equal -nocase "part" $cmd]} {
		set idx [lsearch $chans $chan]

		if {$idx != -1} {
			setbncuser $client tag partyline [join [lreplace $chans $idx $idx] ","]

			putclient ":$botname PART $chan"
			sbnc:bcpartybutone $client ":\$$client!$client@sbnc PART $chan"
		}
	}

	if {[string equal -nocase "names" $cmd]} {
		set idents ""
		foreach user [bncuserlist] {
			if {[lsearch [split [string tolower [getbncuser $user tag partyline]] ","] $chan] != -1 && [getbncuser $user hasclient]} {
				lappend idents "\$$user"
			}
		}

		putclient ":$serv 353 $botnick @ $chan :[join $idents]"
		putclient ":$serv 366 $botnick $chan :End of /NAMES list."
	}

	if {[string equal -nocase "mode" $cmd]} {
		if {[llength $parameters] < 3} {
			putclient ":$serv 324 $botnick $chan +nt"
			putclient ":$serv 329 $botnick $chan 0"
		} elseif {[lindex $parameters 2] == "+b"} {
			putclient ":$serv 368 $botnick $chan :End of Channel Ban List"
		} else {
			putclient ":$serv 482 $botnick $chan :You can't change modes on $chan"
		}
	}

	if {[string equal -nocase "topic" $cmd]} {
		if {[llength $parameters] < 3} {
			putclient ":$serv 332 $botnick $chan :$partytopic($chan)"
			putclient ":$serv 333 $botnick $chan $partywho($chan) $partyts($chan)"
		} else {
			set partytopic($chan) [lindex $parameters 2]
			set partyts($chan) [unixtime]
			set partywho($chan) "\$$client"
			sbnc:bcparty ":\$$client!$client@sbnc TOPIC $chan :$partytopic($chan)"
		}
	}

	if {[string equal -nocase "kick" $cmd]} {
		if {[llength $parameters] < 3} {
			putclient ":$serv 461 $botnick KICK :Not enough parameters"
		} else {
			putclient ":$serv 482 $botnick $chan :You can't kick users from $chan"
		}
	}

	if {[string equal -nocase "privmsg" $cmd]} {
		if {[llength $parameters] < 3} {
			putclient ":$serv 461 $botnick PRIVMSG :Not enough parameters"
		} else {
			sbnc:bcpartybutone $client ":\$$client!$client@sbnc PRIVMSG $chan :[lindex $parameters 2]"
		}
	}
}

proc sbnc:bcparty {text} {
	set chan [lindex [split $text] 2]

	foreach user [bncuserlist] {
		set chans [split [string tolower [getbncuser $user tag partyline]] ","]

		if {[lsearch $chans $chan] != -1} {
			setctx $user
			putclient "$text"
		}
	}
}

proc sbnc:bcpartybutone {client text} {
	set chan [string tolower [lindex [split $text] 2]]

	foreach user [bncuserlist] {
		set chans [split [string tolower [getbncuser $user tag partyline]] ","]

		if {[lsearch $chans $chan] != -1 && ![string equal -nocase $client $user]} {
			setctx $user
			putclient "$text"
		}
	}
}

proc sbnc:partyattach {client} {
	setctx $client

	global botnick botname partyline

	set chans [split [string tolower [getbncuser $client tag partyline]] ","]

	foreach chan $partyline {
		if {[lsearch $chans [string tolower $chan]] != -1} {
			putclient ":$botname JOIN $chan"
			sbnc:partyline $client "NAMES $chan"
			sbnc:partyline $client "TOPIC $chan"

			sbnc:bcpartybutone $client ":\$$client!$client@sbnc JOIN $chan"
		}
	}	
}

proc sbnc:partydetach {client} {
	setctx $client

	global partyline

	set chans [split [string tolower [getbncuser $client tag partyline]] ","]

	foreach chan $partyline {
		if {[lsearch $chans [string tolower $chan]] != -1} {
			sbnc:bcpartybutone $client ":\$$client!$client@sbnc PART $chan :Leaving"
		}
	}
}

proc sbnc:partysync {client} {
	set chans [split [string tolower [getbncuser $client tag partyline]] ","]

	foreach chan $partyline {
		if {[lsearch $chans [string tolower $chan]] != -1} {
			sbnc:bcpartybutone $client ":\$$client!$client@sbnc PART $chan :Removing user"
		}
	}
}
