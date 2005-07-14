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

# configuration variables: ::account354

foreach user [split $::account354] {
	setctx $user
	bind join - * auth:join
	bind raw - 315 auth:raw315
	bind raw - 354 auth:raw354
}

internaltimer 180 1 auth:pulse 180
internaltimer 240 1 auth:pulse 240

proc auth:join {nick host hand chan} {
	namespace eval [getns] {
		if {![info exists inwho]} { set inwho 0 }
		if {![info exists whonicks]} { set whonicks {} }
		if {![info exists accounttimer]} { set accounttimer 0 }
		if {![info exists accountwait]} { set accountwait 0 }
	}

	upvar [getns]::inwho inwho
	upvar [getns]::accounttimer accounttimer
	upvar [getns]::accountwait accountwait

	if {[isbotnick $nick]} {
		if {$inwho} {
			auth:enqueuechan $chan
		} else {
			utimer 1 [list puthelp "WHO $chan n%nat,23"]
			set inwho 1
		}
	} else {
		if {[getchanlogin $nick] == ""} {
			bncsettag $chan $nick accunknown 1

			if {$accounttimer != 0} {
				incr accountwait

				if {$accountwait > 2} {
					return
				}
			}

			internalkilltimer auth:jointimer [getctx]
			internaltimer 3 0 auth:jointimer [getctx]

			set accounttimer 1
		} else {
			sbnc:callbinds "account" - $chan "$chan $host" $nick $host $hand $chan
		}
	}
}

proc auth:enqueuechan {chan} {
	namespace eval [getns] {
		if {![info exists authchans]} { set authchans {} }
	}

	upvar [getns]::authchans authchans

	lappend authchans $chan
}

proc auth:dequeuechan {} {
	namespace eval [getns] {
		if {![info exists authchans]} { set authchans {} }
	}

	upvar [getns]::authchans authchans

	set chan [lindex $authchans 0]
	set authchans [lrange $authchans 1 end]

	return $chan
}

proc auth:raw315 {source raw text} {
	namespace eval [getns] {
		if {![info exists inwho]} { set inwho 0 }
	}

	upvar [getns]::inwho inwho

	set chan [auth:dequeuechan]

	if {$chan != ""} {
		puthelp "WHO $chan n%nat,23"
		set inwho 1
	} else {
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

# it's possible that the same nick gets /who'd twice (during the same call of auth:jointimer). fix?
proc auth:jointimer {context} {
	setctx $context

	namespace eval [getns] {
		if {![info exists accounttimer]} { set accounttimer 0 }
		if {![info exists accountwait]} { set accountwait 0 }
	}

	upvar [getns]::accounttimer accounttimer
	upvar [getns]::accountwait accountwait

	set accounttimer 0
	set accountwait 0

	set nicks ""
	foreach chan [internalchannels] {
		foreach nick [internalchanlist $chan] {
			if {[bncgettag $chan $nick account] == "" && [bncgettag $chan $nick accunknown] == 1} {
				lappend nicks $nick
				bncsettag $chan $nick accunknown 0
			}

			set l [join [sbnc:uniq $nicks] ","]

			if {[string length $l] > 300} {
				puthelp "WHO $l n%nat,23"
				set nicks ""
			}
		}
	}

	if {$nicks != ""} {
		puthelp "WHO [join [sbnc:uniq $nicks] ","] n%nat,23"
	}
}

proc auth:pulse {reason} {
	global account354

	foreach user [split $account354] {
		setctx $user

		set nicks ""
		foreach chan [internalchannels] {
			foreach nick [internalchanlist $chan] {
				set acc [bncgettag $chan $nick account]
				set unk [bncgettag $chan $nick accunknown]

				if {$unk == 1 || ($reason == 180 && $acc == "") || ($acc == 0 && $reason == 240)} {
					lappend nicks $nick
					bncsettag $chan $nick accunknown 0
				}

				if {[string length [join [sbnc:uniq $nicks] ","]] > 300} {
					putserv "WHO [join [sbnc:uniq $nicks] ","] n%nat,23"
					set nicks ""
				}
			}
		}

		if {$nicks != ""} {
			putserv "WHO [join [sbnc:uniq $nicks] ","] n%nat,23"
		}

		namespace eval [getns] {
			set authchans {}
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
