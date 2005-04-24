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
