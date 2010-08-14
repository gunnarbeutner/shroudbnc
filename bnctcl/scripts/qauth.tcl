# shroudBNC - an object-oriented framework for IRC
# Copyright (C) 2005-2007,2010 Gunnar Beutner
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

set ::qauth_algorithms "md5 sha1 sha256"

foreach algorithm $::qauth_algorithms {
	if {![catch [list package require $algorithm]]} {
		lappend ::qauth_supported_algorithms $algorithm
	}
}

if {![info exists ::qauth_supported_algorithms]} {
	set ::qauth_supported_algorithms "none"
}

internalbind command qauth:commands
internalbind svrlogon qauth:logon

proc qauth:replyset {params} {
	# params: [list ctx quser qpass qx]
	setctx [lindex $params 0]

	bncreply "quser - [lindex $params 1]"
	bncreply "qpass - [lindex $params 2]"
	bncreply "qx - [lindex $params 3]"
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

			internaltimer 0 0 qauth:replyset [list [getctx 1] $quser $qpass $qx]

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
			if {![string equal -nocase [lindex $params 2] "on"] && ![string equal -nocase [lindex $params 2] "off"]} {
				bncreply "Value should be either on or off."
				haltoutput

				return
			}

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

	if {$::qauth_supported_algorithms == "none"} {
		putquick "PRIVMSG Q@CServe.QuakeNet.Org :AUTH $quser $qpass"
	} else {
		putquick "PRIVMSG Q@CServe.QuakeNet.Org :CHALLENGE"
	}

	internalbind server qauth:server * $client
	timer 30 [list internalunbind server qauth:server * $client]
}

proc qauth:server {client params} {
	if {[string equal -nocase [lindex $params 0] "Q!TheQBot@CServe.quakenet.org"]} {

		if {[string match -nocase "You are now logged in as*" [lindex $params 3]]} {
			bncjoinchans $client

			if {![getbncuser $client hasclient]} {
				putlog "Authenticated with network service"
			}

			internalunbind server qauth:server * $client
		}

		if {[string match -nocase "*CHALLENGE*" [lindex $params 3]]} {
			set challenge [lindex [split [lindex $params 3] " "] 1]
			set quser [getbncuser $client tag quser]
			set quserlower [string tolower [string map -nocase {"[" "{" "]" "}" "|" "\\"} [getbncuser $client tag quser]]]
			set qpass [string range [getbncuser $client tag qpass] 0 9]

			if {[string match -nocase "*HMAC-SHA-256*" [lindex $params 3]] && [lsearch $::qauth_supported_algorithms sha256] > "-1"} {
				set qpass [string tolower [::sha2::sha256 -hex $qpass]]
				set response [string tolower [::sha2::sha256 -hex "${quserlower}:${qpass}"]]
				set response [string tolower [::sha2::hmac -hex -key $response $challenge]]
				putquick "PRIVMSG Q@CServe.QuakeNet.Org :CHALLENGEAUTH $quser $response HMAC-SHA-256"
			} elseif {[string match -nocase "*HMAC-SHA-1*" [lindex $params 3]] && [lsearch $::qauth_supported_algorithms sha1] > "-1"} {
				set qpass [string tolower [::sha1::sha1 -hex $qpass]]
				set response [string tolower [::sha1::sha1 -hex "${quserlower}:${qpass}"]]
				set response [string tolower [::sha1::hmac -hex -key $response $challenge]]
				putquick "PRIVMSG Q@CServe.QuakeNet.Org :CHALLENGEAUTH $quser $response HMAC-SHA-1"
			} elseif {[string match -nocase "*HMAC-MD5*" [lindex $params 3]] && [lsearch $::qauth_supported_algorithms md5] > "-1"} {
				set qpass [string tolower [::md5::md5 -hex $qpass]]
				set response [string tolower [::md5::md5 -hex "${quserlower}:${qpass}"]]
				set response [string tolower [::md5::hmac -hex -key $response $challenge]]
				putquick "PRIVMSG Q@CServe.QuakeNet.Org :CHALLENGEAUTH $quser $response HMAC-MD5"
			} else {
				putquick "PRIVMSG Q@CServe.QuakeNet.Org :AUTH $quser $qpass"
			}
		}
	}
}

proc iface-qauth:qsetuser {username} {
	setbncuser [getctx] tag quser $username

	return ""
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "qauth" "qsetuser" "iface-qauth:qsetuser"
}

proc iface-qauth:qsetpass {password} {
	setbncuser [getctx] tag qpass $password

	return ""
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "qauth" "qsetpass" "iface-qauth:qsetpass"
}

proc iface-qauth:qsetx {value} {
	if {[string is integer $value] && $value} {
		set qx on
	} else {
		set qx off
	}

	setbncuser [getctx] tag qx $qx

	return ""
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "qauth" "qsetx" "iface-qauth:qsetx"
}

proc iface-qauth:qgetuser {} {
	return [itype_string [getbncuser [getctx] tag quser]]
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "qauth" "qgetuser" "iface-qauth:qgetuser"
}

proc iface-qauth:qhaspass {} {
	if {[getbncuser [getctx] tag qpass] == ""} {
		return [itype_string "0"]
	} else {
		return [itype_string "1"]
	}
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "qauth" "qhaspass" "iface-qauth:qhaspass"
}

proc iface-qauth:qgetx {} {
	if {[string equal -nocase [getbncuser [getctx] tag qx] "on"]} {
		return [itype_string "1"]
	} else {
		return [itype_string "0"]
	}
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "qauth" "qgetx" "iface-qauth:qgetx"
}
