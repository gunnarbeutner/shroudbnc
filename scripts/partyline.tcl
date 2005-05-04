internalbind client sbnc:partyline
internalbind attach sbnc:partyattach
internalbind detach sbnc:partydetach
internalbind usrdelete sbnc:partysync

set ::partyline "&partyline"

if {![info exists ::partytopic]} {
	set ::partytopic "shroudBNC Partyline"
	set ::partywho "-sBNC"
}

proc sbnc:partyline {client parameters} {
	global botnick botname server partyline partytopic partywho

	setctx $client

	if {![string equal -nocase $partyline [lindex $parameters 1]]} { return }

	haltoutput

	set cmd [lindex $parameters 0]
	set serv [lindex [split $server ":"] 0]

	if {[string equal -nocase "join" $cmd]} {
		if {[getbncuser $client tag partyline] != 1} {
			setbncuser $client tag partyline 1
			putclient ":$botname JOIN $partyline"
			sbnc:partyline $client "NAMES $partyline"
			sbnc:partyline $client "TOPIC $partyline"

			sbnc:bcpartybutone $client ":\$$client!$client@sbnc JOIN $partyline"
		}
	}

	if {[string equal -nocase "part" $cmd]} {
		if {[getbncuser $client tag partyline] == 1} {
			setbncuser $client tag partyline 0

			putclient ":$botname PART $partyline"
			sbnc:bcpartybutone $client ":\$$client!$client@sbnc PART $partyline"
		}
	}

	if {[string equal -nocase "names" $cmd]} {
		set idents ""
		foreach user [bncuserlist] {
			if {[getbncuser $user tag partyline] == 1 && [getbncuser $user hasclient]} {
				lappend idents "\$$user"
			}
		}

		putclient ":$serv 353 $botnick @ $partyline :[join $idents]"
		putclient ":$serv 366 $botnick $partyline :End of /NAMES list."
	}

	if {[string equal -nocase "mode" $cmd]} {
		if {[llength $parameters] < 3} {
			putclient ":$serv 324 $botnick $partyline +nt"
			putclient ":$serv 329 $botnick $partyline 0"
		} elseif {[lindex $parameters 2] == "+b"} {
			putclient ":$serv 368 $botnick $partyline :End of Channel Ban List"
		} else {
			putclient ":$serv 482 $botnick $partyline :You can't change modes on $partyline"
		}
	}

	if {[string equal -nocase "topic" $cmd]} {
		if {[llength $parameters] < 3} {
			putclient ":$serv 332 $botnick $partyline :$partytopic"
			putclient ":$serv 333 $botnick $partyline $partywho 0"
		} else {
			set partytopic [lindex $parameters 2]
			set partywho $client
			sbnc:bcparty ":\$$client!$client@sbnc TOPIC $partyline :$partytopic"
		}
	}

	if {[string equal -nocase "kick" $cmd]} {
		if {[llength $parameters] < 3} {
			putclient ":$serv 461 $botnick KICK :Not enough parameters"
		} else {
			putclient ":$serv 482 $botnick $partyline :You can't kick users from $partyline"
		}
	}

	if {[string equal -nocase "privmsg" $cmd]} {
		if {[llength $parameters] < 3} {
			putclient ":$serv 461 $botnick PRIVMSG :Not enough parameters"
		} else {
			sbnc:bcpartybutone $client ":\$$client!$client@sbnc PRIVMSG $partyline :[lindex $parameters 2]"
		}
	}
}

proc sbnc:bcparty {text} {
	foreach user [bncuserlist] {
		if {[getbncuser $user tag partyline] == 1} {
			setctx $user
			putclient "$text"
		}
	}
}

proc sbnc:bcpartybutone {client text} {
	foreach user [bncuserlist] {
		if {[getbncuser $user tag partyline] == 1 && ![string equal -nocase $client $user]} {
			setctx $user
			putclient "$text"
		}
	}
}

proc sbnc:partyattach {client} {
	global botnick botname partyline

	setctx $client

	if {[getbncuser $client tag partyline] == 1} {
		utimer 1 [list putclient ":$botname JOIN $partyline"]
		utimer 1 [list sbnc:partyline $client "NAMES $partyline"]
		utimer 1 [list sbnc:partyline $client "TOPIC $partyline"]

		sbnc:bcpartybutone $client ":\$$client!$client@sbnc JOIN $partyline"
	}	
}

proc sbnc:partydetach {client} {
	global partyline

	setctx $client

	if {[getbncuser $client tag partyline] == 1} {
		sbnc:bcpartybutone $client ":\$$client!$client@sbnc PART $partyline :Leaving"
	}
}

proc sbnc:partysync {client} {
	if {[getbncuser $client tag partyline] == 1} {
		sbnc:bcpartybutone $client ":\$$client!$client@sbnc PART $partyline :Removing user"
	}
}
