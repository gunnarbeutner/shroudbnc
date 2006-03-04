internalbind command lock:command

set ::globallocks [list awaymessage]

proc lock:command {client parameters} { 
	set command [lindex $parameters 0]
	
	if {[string equal -nocase $command "help"]} {
		bncaddcommand lock Admin "locks a user's settings" "/sbnc lock <user> \[setting\]\nShows locked settings for a user or locks a setting"
		bncaddcommand unlock Admin "unlocks a user's settings" "/sbnc unlock <user> <setting>\nUnlocks a users setting"
	}
	
	if {[string equal -nocase $command "lock"]} {
		if {[getbncuser $client admin]} {
			if {[llength $parameters] <= 1} {
				bncreply "Syntax: /sbnc lock <user> \[setting\]"
				haltoutput
			} else {
				if {[string equal -nocase [lindex $parameters 2] ""]} {
					set tmp [split [bncuserlist] " "]
					set res [lsearch $tmp [lindex $parameters 1]]
					if {[string equal -nocase $res "-1"]} {
						bncreply "No such user [lindex $parameters 1]."
						haltoutput
					} else {			 
						set tmp [split [getbncuser [lindex $parameters 1] tag locksetting] ","]
						bncreply "Locked settings for user [lindex $parameters 1]:"
						if {[string equal -nocase $tmp ""]} {
							bncreply "None."
							haltoutput	
						} else {
							bncreply $tmp
							haltoutput
						}
					}
				} else {
					if {[lsearch -exact [list server port realname awaynick away awaymessage vhost quitasaway] [lindex $parameters 2]] == -1} {
						bncreply "Error: Cannot lock this setting."
						haltoutput
					} else {
						set tmp [split [getbncuser [lindex $parameters 1] tag locksetting] ","]
						lappend tmp [lindex $parameters 2]
						setbncuser [lindex $parameters 1] tag locksetting [join $tmp ","]
						bncreply "Done."
						haltoutput
					}
				}
			}
		}
	}

	if {[string equal -nocase $command "unlock"]} {
		if {[getbncuser $client admin]} {
			if {[llength $parameters] < 3} {
				bncreply "Syntax: /sbnc unlock <user> <setting>"
				haltoutput
			} else {
				set tmp [split [bncuserlist] " "]
				set res [lsearch $tmp [lindex $parameters 1]]
				if {[string equal -nocase $res "-1"]} {
					bncreply "No such user [lindex $parameters 1]."
					haltoutput
				} else {
					set tmp [split [getbncuser [lindex $parameters 1] tag locksetting] ","];
					set idx [lsearch -exact $tmp [lindex $parameters 2]]
					set tmp [lreplace $tmp $idx $idx]
					setbncuser [lindex $parameters 1] tag locksetting [join $tmp ","]
					bncreply "Done."
					haltoutput
				}
			}
		}
	}

    if {[string equal -nocase $command "set"] && [llength $parameters] > 2} {
		set setting [lindex $parameters 1]
		set notlocked [lock:set $client $setting [join [lrange $parameters 2 end]]]

		if {[string equal -nocase $setting "server"] && $notlocked} {
			set notlocked [lock:set $client "port" [join [lrange $parameters 2 end]]]
		}

		if {$notlocked} {
			return
		} else {
			bncreply "Error: You cannot modify this setting."
			haltoutput
		}
	}
	
}

#------------------ HALT SET WHEN LOCKED --------------------
proc lock:ifacecmd {command params account override} {
	switch -- $command {
		"lockedsetting" {
			return [lock:islocked $account [lindex $params 0]]
		}
		"set" {
			if {$override || [lock:set $account [lindex $params 0] [lrange $params 1 end]]} {
				return
			} else {
				return "denied."
			}
		}
	}
	
	if {[getbncuser $account admin]} {
		switch -- $command {
			"locksetting" {
				set tmp [split [getbncuser [lindex $params 0] tag locksetting] ","]
				set res [lsearch -exact $tmp [lindex $params 1]]

				if {[string equal -nocase $res "-1"]} {
					lappend tmp [lindex $params 1]
				}
				setbncuser [lindex $params 0] tag locksetting [join $tmp ","]
			}	
			"unlocksetting" {
				set tmp [split [getbncuser [lindex $params 0] tag locksetting] ","]; 
				set idx [lsearch -exact $tmp [lindex $params 1]]
				set tmp [lreplace $tmp $idx $idx]
				setbncuser [lindex $params 0] tag locksetting [join $tmp ","] 
			}
		}
	}
}

proc lock:islocked {account setting} {
	global globallocks

	set tmp [split [getbncuser $account tag locksetting] ","]

	if {[lsearch -exact $tmp $setting] != -1} {
		return 1
	} else {
		if {[lsearch -exact $globallocks $setting] != -1} {
			return 2
		} else {
			return 0
		}
	}
}

# account = who wants to set
# parameters
#	0 = what user
#	1 = what setting
#	2 - end = value
# returns:
#	1 if user can change the setting
#	0 otherwise
proc lock:set {account setting value} {
	if {[getbncuser $account admin]} {
		return 1
	} else {
		if {![string equal [lock:islocked $account $setting] "0"]} {
			return 0
		} else {
			return 1
		}
	}
}

if {[lsearch -exact [info commands] "registerifacehandler"] != -1} {
	registerifacehandler lock lock:ifacecmd
}
