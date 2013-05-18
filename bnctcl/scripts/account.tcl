# shroudBNC - an object-oriented framework for IRC
# Copyright (C) 2005-2011 Gunnar Beutner
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

# configuration variables: ::account354

foreach user [split $::account354] {
	setctx $user
	bind join - * auth:join
	bind raw - 315 auth:raw315
	bind raw - 354 auth:raw354
	bind evnt - init-server auth:connect
}

internaltimer 180 1 auth:pulse 180
internaltimer 240 1 auth:pulse 240
internaltimer 5 1 auth:processqueue

proc auth:connect {type} {
	namespace eval [getns] {
		if {![info exists inwho]} { set inwho 0 }
		if {![info exists authqueue]} { set authqueue {} }
	}

	upvar [getns]::inwho inwho
	upvar [getns]::authqueue authqueue

	set inwho 0
	set authqueue [list]

	internalkilltimer auth:jointimer [getctx]
}

proc auth:join {nick host hand chan} {
	namespace eval [getns] {
		if {![info exists whonicks]} { set whonicks {} }
	}

	if {[isbotnick $nick]} {
		auth:enqueue $chan
	} else {
		auth:enqueue $nick
	}

	internalkilltimer auth:jointimer [getctx]
	internaltimer 3 0 auth:jointimer [getctx]
}

proc auth:enqueue {names {now 0}} {
	namespace eval [getns] {
		if {![info exists authqueue]} { set authqueue [list] }
	}

	upvar [getns]::authqueue authqueue

	foreach name $names {
		set name [string tolower $name]

		if {[lsearch -exact $authqueue $name] == -1} {
			if {$now} {
				set authqueue [linsert $authqueue 0 $name]
			} else {
				lappend authqueue $name
			}
		}
	}

	if {$now} {
		auth:processqueue
	}
}

proc auth:processqueue {} {
	global account354

	foreach user [split $account354] {
		if {![bncvaliduser $user]} {
			continue
		}

		setctx $user

		namespace eval [getns] {
			if {![info exists authqueue]} { set authqueue [list] }
			if {![info exists inwho]} { set inwho 0 }
		}

		upvar [getns]::authqueue authqueue
		upvar [getns]::inwho inwho

		if {$inwho} {
			continue
		}

		set names [lrange $authqueue 0 9]
		set authqueue [lrange $authqueue 10 end]

		if {[llength $names] > 0} {
			set inwho 1
			puthelp "WHO [join $names ","] n%nat,23"
		}
	}
}

proc auth:raw315 {source raw text} {
	namespace eval [getns] {
		if {![info exists inwho]} { set inwho 0 }
	}

	upvar [getns]::inwho inwho

	if {$inwho} {
		haltoutput
		set inwho 0
	}
}

proc auth:raw354 {source raw text} {
	set pieces [split $text]

	if {[lindex $pieces 1] != 23} { return }

	set nick [lindex $pieces 2]
	set site [getchanhost $nick]
	set host [lindex $pieces 2]!$site
	set hand [finduser $host]

	haltoutput

	foreach chan [internalchannels] {
		if {[onchan $nick $chan]} {
			set old [bncgettag $chan $nick account]
			bncsettag $chan $nick account [lindex $pieces 3]

			if {$old == "" || ($old == 0 && [lindex $pieces 3] != 0)} {
				sbnc:callbinds "account" - $chan "$chan $host" $nick $site $hand $chan
			}
		}
	}
}

proc auth:jointimer {context} {
	setctx $context
	auth:processqueue
}

proc auth:pulse {reason} {
	global account354

	foreach user [split $account354] {
		if {![bncvaliduser $user]} {
			continue
		}

		setctx $user


		foreach chan [internalchannels] {
			set count 0
			set nicks [list]

			foreach nick [internalchanlist $chan] {
				set account [bncgettag $chan $nick account]

				if {$account == 0 || $account == ""} {
					incr count
				}
			}

			if {$count > 100 || ($count > 20 && $count > [llength [internalchanlist $chan]] * 3 / 4)} {
				auth:enqueue $chan
				continue
			}

			foreach nick [internalchanlist $chan] {
				set account [bncgettag $chan $nick account]

				if {($reason == 180 && $account == "") || ($account == 0 && $reason == 240)} {
					lappend nicks $nick
				}
			}

			auth:enqueue $nicks
		}
	}
}

proc getchanlogin {nick {channel ""}} {
	foreach chan [internalchannels] {
		set acc [bncgettag $chan $nick account]

		if {$acc != ""} {
			return $acc
		}
	}

	return ""
}
