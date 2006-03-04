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
			if {![getbncuser $client tag lockawaymessage]} {
				setbncuser $client tag awaymessage [lindex $parameters 2]
				bncreply "Done."
				haltoutput
			}
		} elseif {[string equal [lindex $parameters 2] ""]} {
			utimer 0 [list bncreply "awaymessage - [getbncuser $client tag awaymessage]"]
		}
	}
}

proc iface-awaymsg:setvalue {setting value} {
	if {[string equal -nocase $setting "awaymessage"]} {
		if {[lsearch -exact [info commands] "lock:islocked"] != -1} {
			if {[lock:islocked [getctx] "awaymessage"]} { return -code error "Setting cannot be modified." }
		}

		setbncuser [getctx] tag awaymessage $value

		return 1
	}
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "awaymsg" "setvalue" "iface-awaymsg:setvalue"
}

proc iface-awaymsg:getvalue {setting} {
	if {[string equal -nocase $setting "awaymessage"]} {
		return [getbncuser [getctx] tag awaymessage]
	}
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "awaymsg" "getvalue" "iface-awaymsg:getvalue"
}
