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

internalbind detach awaymsg:detach
internalbind command awaymsg:command

proc awaymsg:detach {client} {
	set result [getbncuser $client tag awaymessage]

	if {$result != ""} {
		foreach chan [internalchannels] {
			putquick "PRIVMSG $chan :\001ACTION $result \001"
		}
	}
}

proc awaymsg:command {client parameters} {
	set command [lindex $parameters 0]

	if {[string equal -nocase $command "set"]} {
		if {[string equal -nocase [lindex $parameters 1] "awaymessage"] && [llength $parameters] >= 3} {
		    if {[lsearch -exact [info commands] "lock:islocked"] != -1} {
		        set locked [lock:islocked $client awaymessage]
			if {![string equal $locked "0"]} { 
			    return
			}
		    }
		    setbncuser $client tag awaymessage [lindex $parameters 2]
		    bncreply "Done."
		    haltoutput
		} elseif {[string equal [lindex $parameters 2] ""]} {
			utimer 0 [list bncreply "awaymessage - [getbncuser $client tag awaymessage]"]
		}
	}
}


proc awaymsg:ifacecmd {command params account} {
	switch -- $command {
		"set" {
			if {[lsearch -exact [info commands] "lock:islocked"] != -1} {
				if {![string equal [lock:islocked $account "awaymessage"] "0"]} { return }
			}

			if {[string equal -nocase [lindex $params 0] "awaymessage"]} {
				setbncuser $account tag awaymessage [join [lrange $params 1 end]]
			}
		}
		"value" {
			if {[string equal -nocase [lindex $params 0] "awaymessage"]} {
				return [getbncuser $account tag awaymessage]
			}
		}
	}
}

if {[lsearch -exact [info commands] "registerifacehandler"] != -1} {
	registerifacehandler awaymsg awaymsg:ifacecmd
}
