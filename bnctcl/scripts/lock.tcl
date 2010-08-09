# shroudBNC - an object-oriented framework for IRC
# Copyright (C) 2010 Christoph Wiese
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

set ::lock_lockable_settings [list password vhost server port serverpass realname awaynick away awaymessage usequitasaway ssl ipv6 automodes dropmodes]

internalbind command lock:command

proc lock:command {client params} {
	set command [string tolower [lindex $params 0]]

	if {$command == "set" && [llength $params] > 2 && [lock:islocked $client [lindex $params 1]]} {
		bncreply "Error: You cannot modify this setting."
		haltoutput
		return
	}

	if {$command == "globalset" && [getbncuser $client admin]} {
		if {[llength $params] < 3} {
			if {[bncgetglobaltag globallocks] == ""} {
				set reply "Not set"
			} else {
				regsub -all "," [bncgetglobaltag globallocks] " " reply
			}

			internaltimer 0 0 bncreply "globallocks - $reply"
		} else {
			set reply [lock:setgloballocks [lrange $params 2 end]]

			if {[lindex $reply 0] != ""} {
				bncreply "The following settings were locked: [lindex $reply 0]"
			} elseif {[lindex $reply 0] == "" && [lindex $reply 1] == ""} {
				bncreply "Done."
			} else {
				bncreply "None of the default settings were entered, setting has not been changed."
			}

			if {[lindex $reply 1] != ""} {
				bncreply "The following settings are not part of the default settings and are therefore not lockable: [lindex $reply 1]"
			}

			haltoutput
		}

		return
	}

	if {($command == "lock" || $command == "unlock") && [getbncuser $client admin]} {
		if {($command == "lock" && [llength $params] < 2) || ($command == "unlock" && [llength $params] < 3)} {
			if {$command == "lock"} {
				set reply "Syntax: /sbnc lock <user> \[setting\]"
			} else {
				set reply "Syntax: /sbnc unlock <user> <setting>"
			}

			bncreply $reply
			haltoutput
			return
		}

		set targetuser [lindex $params 1]

		if {![bncvaliduser $targetuser]} {
			bncreply "No such user \"$targetuser\"."
			haltoutput
			return
		}

		set lockedsettings [split [getbncuser $targetuser tag locksetting] ","]

		if {[llength $params] < 3} {
			if {$lockedsettings != ""} {
				set reply $lockedsettings
			} else {
				set reply "none"
			}

			bncreply "Settings specifically locked for user \"$targetuser\": $reply."
			haltoutput
			return
		}

		set targetsetting [string tolower [lindex $params 2]]

		if {$command == "lock"} {
			if {[lock:setuserlock $targetuser $targetsetting] == 1} {
				bncreply "Done."
			} else {
				bncreply "Setting \"$targetsetting\" is not part of the defaultsettings and is therefore not lockable."
			}
		} else {
			if {[lock:unsetuserlock $targetuser $targetsetting] == 1} {
				bncreply "Done."
			} else {
				bncreply "Setting \"$targetsetting\" was not locked for user \"$targetuser\", no changes applied."
			}

		}

		haltoutput
		return
	}

	if {$command == "help" && [getbncuser $client admin]} {
		bncaddcommand lock Admin "locks a user's settings" "/sbnc lock <user> \[setting\]\nShows locked settings for a user or locks a setting"
		bncaddcommand unlock Admin "unlocks a user's settings" "/sbnc unlock <user> <setting>\nUnlocks a users setting"
	}
}

# Parameters:
# account: user account on which to perform the check
# setting: setting which is checked on whether or not it is locked
# Returns:
# 0 - if setting is not locked, or account is an admin
# 1 - if setting is locked

proc lock:islocked {account setting} {
	set lockedsettings [concat [split [getbncuser $account tag locksetting] ","] [split [bncgetglobaltag globallocks] ","]]

	if {[lsearch -exact $lockedsettings [string tolower $setting]] > -1 && ![getbncuser $account admin]} {
		return "1"
	} else {
		return "0"
	}
}

# Parameters:
# settings - settings to be globally locked
# Returns:
# A list with two elements, the first containing the settings successfully locked, the second containg those on which the lock failed

proc lock:setgloballocks {settings} {
	if {$settings == "{}"} {
		bncsetglobaltag globallocks ""
		return [list "" ""]
	}

	foreach setting $settings {
		set setting [string tolower $setting]

		if {[lsearch -exact $::lock_lockable_settings $setting] > -1} {
			lappend lockable $setting
		} else {
			lappend unlockable $setting
		}
	}



	if {[info exists lockable]} {
		bncsetglobaltag globallocks [join $lockable ","]
		set reply [list $lockable]
	} else {
		set reply [list ""]
	}

	if {[info exists unlockable]} {
		lappend reply $unlockable
	} else {
		lappend reply ""
	}

	return $reply
}

# Parameters:
# account - user account for which to lock
# setting - setting to be locked
# Returns:
# 0 - if lock failed
# 1 - if lock succeeded

proc lock:setuserlock {account setting} {
	set setting [string tolower $setting]
	set lockedsettings [split [getbncuser $account tag locksetting] ","]

	if {[lsearch -exact $lockedsettings $setting] > -1} {
		return "1"
	} elseif {[lsearch -exact $::lock_lockable_settings $setting] > -1 } {
		setbncuser $account tag locksetting [join [lappend lockedsettings $setting] ","]
		return "1"
	} else {
		return "0"
	}
}

# Parameters:
# account - user account for which to unlock
# setting - setting to be unlocked
# Returns:
# 0 - if unlock failed
# 1 - if unlock succeeded

proc lock:unsetuserlock {account setting} {
	set setting [string tolower $setting]
	set lockedsettings [split [getbncuser $account tag locksetting] ","]

	if {[lsearch -exact $lockedsettings $setting] > -1} {
		set idx [lsearch -exact $lockedsettings $setting]
		setbncuser $account tag locksetting [join [lreplace $lockedsettings $idx $idx] ","]
		return "1"
	} else {
		return "0"
	}
}

# Override for ifacecmds.tcl's setvalue to respect locks

proc lock:iface_setvalue {setting value} {
	if {[lock:islocked [getctx] $setting] == 1} {
		return -code error "You may not modify this setting."
	} else {
		iface:setvalue $setting $value
		return ""
	}
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "lock" "setvalue" "lock:iface_setvalue"
}

# The following procs are iface2 wrappers for their respective counterparts above

proc lock:iface_setgloballocks {settings} {
	set result [lindex [lock:setgloballocks $settings]]

	if {[lindex $result 1] == ""} {
		return ""
	} else {
		return -code error "You may not lock these settings: [lindex $result 1]."
	}
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "lock" "setgloballocks" "lock:iface_setgloballocks" access:admin
}

proc lock:iface_setuserlock {account setting} {
	if {![bncvaliduser $account]} {
		return -code error "Invalid user \"$account\"."
	}

	if {[lock:setuserlock $account $setting]} {
		return ""
	} else {
		return -code error "You cannot lock this setting."
	}
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "lock" "setuserlock" "lock:iface_setuserlock" access:admin
}

proc lock:iface_unsetuserlock {account setting} {
	if {![bncvaliduser $account]} {
		return -code error "Invalid user \"$account\"."
	}

	if {[lock:unsetuserlock $account $setting]} {
		return ""
	} else {
		return -code error "You cannot unlock this setting."
	}
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "lock" "unsetuserlock" "lock:iface_unsetuserlock" access:admin
}