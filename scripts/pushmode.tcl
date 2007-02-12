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

proc sbnc:pminternalflush {} {
	internalunbind post sbnc:pminternalflush

	foreach user [bncuserlist] {
		setctx $user

		namespace eval [getns] {
			foreach chan [array names pmbuf] {
				flushmode $chan
			}

			array unset pmbuf
		}
	}
}

proc sbnc:uniq {list} {
	set out [list]

	foreach item $list {
		if {[lsearch -exact $out $item] == -1} {
			lappend out $item
		}
	}

	return $out
}

proc flushmode {channel} {
	namespace eval [getns] {
		if {![info exists pmbuf]} { array set pmbuf {} }
	}

	upvar [getns]::pmbuf pmbuf

	set mc ""
	set params ""

	if {![info exists pmbuf($channel)] || ![botisop $channel]} {
		return 0
	}

	set pmbuf($channel) [sbnc:uniq $pmbuf($channel)]

	foreach mode $pmbuf($channel) {
		if {[string length [lindex $mode 1]] == 0} {
			set mc "${mc}[lindex $mode 0]"
		}
	}

	foreach mode $pmbuf($channel) {
		if {[string length [lindex $mode 1]] != 0} {
			set mc "${mc}[lindex $mode 0]"
			lappend params [lindex $mode 1]
		}

		if {[llength $params] >= [getisupport modes]} {
			putquick "MODE $channel $mc [join $params]"
			set mc ""
			set params ""
		}
	}

	putquick "MODE $channel $mc [join $params]"

	unset pmbuf($channel)

	return
}

proc pushmode {channel mode {arg ""}} {
	namespace eval [getns] {
		if {![info exists pmbuf]} { array set pmbuf {} }
	}

	upvar [getns]::pmbuf pmbuf

	set mtype [requiresparam [string map {"+" "" "-" ""} $mode]]

	if {$arg == {} && ($mtype == 2 || ($mtype == 1 && [string first "-" $mode] == -1))} {
			return -code error "Chanmode $mode requires a parameter."
	}

	internalbind post sbnc:pminternalflush

	if {[info exists pmbuf($channel)]} {
			lappend pmbuf($channel) [list $mode $arg]
	} else {
			set pmbuf($channel) [list [list $mode $arg]]
	}

	return
}
