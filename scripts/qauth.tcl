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

internalbind command qauth:commands
internalbind svrlogon qauth:logon

proc qauth:replyset {quser qpass qx} {
	bncreply "quser - $quser"
	bncreply "qpass - $qpass"
	bncreply "qx - $qx"
}

proc qauth:commands {client params} {
	if {[string equal -nocase [lindex $params 0] "set"]} {
		if {[llength $params] < 3} {
			if {[getbncuser $client tag quser] != ""} {
				set quser [getbncuser $client tag quser]
			} else {
				set quser "Not set"
			}

			if {[getbncuser $client tag qpass] != ""} {
				set qpass "Set"
			} else {
				set qpass "Not set"
			}

			if {[string equal -nocase [getbncuser $client tag qx] "on"]} {
				set qx "On"
			} else {
				set qx "Off"
			}

			utimer 0 [list qauth:replyset $quser $qpass $qx]

			return
		}

		if {[string equal -nocase [lindex $params 1] "quser"]} {
			setbncuser $client tag quser [lindex $params 2]
			bncreply "Done."
			haltoutput
		}

		if {[string equal -nocase [lindex $params 1] "qpass"]} {
			setbncuser $client tag qpass [lindex $params 2]
			bncreply "Done."
			haltoutput
		}

		if {[string equal -nocase [lindex $params 1] "qx"]} {
			setbncuser $client tag qx [lindex $params 2]
			bncreply "Done."
			haltoutput
		}

		if {[getbncuser $client tag quser] != "" && [getbncuser $client tag qpass] != ""} {
			setbncuser $client delayjoin 1
		} else {
			setbncuser $client delayjoin 0
		}
	}
}

proc qauth:logon {client} {
	global botnick

	set quser [getbncuser $client tag quser]
	set qpass [getbncuser $client tag qpass]
	set qx [getbncuser $client tag qx]

	if {$quser == "" || $qpass == ""} { return }

	if {[string equal -nocase $qx "on"]} {
		putquick "MODE $botnick +x"
	}

	putquick "PRIVMSG Q@CServe.QuakeNet.Org :AUTH $quser $qpass"

	internalbind server qauth:server PRIVMSG $client
	timer 5 [list internalunbind server qauth:server PRIVMSG $client]
}

proc qauth:server {client params} {
	if {![string equal -nocase [lindex $params 1] "PRIVMSG"]} { return }

	if {[string equal -nocase [lindex [split [lindex $params 0] "!"] 0] "Q"]} {
		if {[string match -nocase "*AUTH'd successfully"] [lindex $params 3]} {
			joinchannels

			if {![getbncuser $client hasclient]} {
				putlog "Authenticated with network service"
			}

			internalunbind server qauth:server PRIVMSG $client
		}
	}
}

proc qauth:ifacecmd {command params account} {
	switch -- $command {
		"qsetuser" {
			setbncuser $account tag quser [lindex $params 0]
			return 0
		}
		"qsetpass" {
			setbncuser $account tag qpass [lindex $params 0]
			return 0
		}
		"qsetx" {
			if {[string is integer [lindex $params 0]] && [lindex $params 0]} {
				set qx on
			} else {
				set qx off
			}

			setbncuser $account tag qx $qx
			return 0
		}
		"qgetuser" {
			return [getbncuser $account tag quser]
		}
		"qhaspass" {
			set pass [getbncuser $account tag qpass]

			if {$pass == ""} {
				return 0
			} else {
				return 1
			}
		}
		"qgetx" {
			if {[string equal -nocase [getbncuser $account tag qx] "on"]} {
				return 1
			} else {
				return 0
			}
		}
	}
}

if {[lsearch -exact [info commands] "registerifacehandler"] != -1} {
	registerifacehandler qauth qauth:ifacecmd
}
