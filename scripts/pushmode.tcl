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

internalbind post sbnc:pminternalflush

proc sbnc:pminternalflush {} {
	global sbnc:pmbuf

	foreach chan [array names sbnc:pmbuf] {
		flushmode $chan
	}

	if {[string length [array get sbnc:pmbuf]] > 0} {
		array unset sbnc:pmbuf
	}
}

proc sbnc:uniq {list} {
	set out ""

	foreach item $list {
		if {[lsearch -exact $out $item] == -1} {
			lappend out $item
		}
	}

	return $out
}

proc flushmode {channel} {
	upvar ::sbnc:pmbuf pmbuf
	set mc ""
	set params ""

	set pmbuf($channel) [sbnc:uniq $pmbuf($channel)]

	if {![info exists pmbuf($channel)] || ![botisop $channel]} {
		return 0
	}

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
			putquick "MODE $channel $mc $params"
			set mc ""
			set params ""
		}
	}

	putquick "MODE $channel $mc $params"

	unset pmbuf($channel)

	return
}

proc sbnc:requiresparam {chanmode} {
	set modes [split [getisupport "chanmodes"] ","]
	set modesA [lindex $modes 0]
	set modesB [lindex $modes 1]
	set modesC [lindex $modes 2]
	set modesD [lindex $modes 3]

	set prx [sbnc:isprefixmode $chanmode]

	if {$prx != 0} {
		return $prx
	}

	if {[string first $chanmode $modesA] != -1} {
		return 3
	} elseif {[string first $chanmode $modesB] != -1} {
		return 2
	} elseif {[string first $chanmode $modesC] != -1} {
		return 1
	} else {
		return 0
	}
}

proc sbnc:isprefixmode {chanmode} {
	set prefix [getisupport "prefix"]
	set bracket [string first ")" $prefix]

	if {$bracket == -1} {
		return 0
	}

	set modes [string range $prefix 1 [expr $bracket - 1]]

	if {[string first $chanmode $modes] != -1} {
		return 2
	} else {
		return 0
	}
}

proc pushmode {channel mode {arg ""}} {
	upvar ::sbnc:pmbuf pmbuf

	set mtype [sbnc:requiresparam [string map {"+" "" "-" ""} $mode]]

	if {$mtype != 0 && $arg == "" && ($mtype != 1 && [string first "-" $mode] != -1)} {
		return -code error "Chanmode $mode requires a parameter."
	}

	if {[info exists pmbuf($channel)]} {
		lappend pmbuf($channel) [list $mode $arg]
	} else {
		set pmbuf($channel) [list [list $mode $arg]]
	}

	return
}
