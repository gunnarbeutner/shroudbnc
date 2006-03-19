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

package require bnc 0.2

proc sbnc:runthistimer {cookie} {
	set user [lindex $cookie 0]
	set timerID [lindex $cookie 1]

	setctx $user

	namespace eval [getns] {
		if {![info exists timers]} { set timers "" }
	}

	upvar [getns]::timers timers

	set idx [lsearch $timers "* $timerID"]
	if {$idx == -1} { return }

	set timer [lindex $timers $idx]

	if {[catch [lindex $timer 1] error]} {
		foreach user [bncuserlist] {
			if {[getbncuser $user admin]} {
				bncnotc "Error in timer $timerID: $error"
			}
		}
	}

	set idx [lsearch $timers "* $timerID"]
	if {$idx == -1} { return }

	set timers [lreplace $timers $idx $idx]
}

proc sbnc:runthisutimer {cookie} {
	set user [lindex $cookie 0]
	set timerID [lindex $cookie 1]

	setctx $user

	namespace eval [getns] {
		if {![info exists utimers]} { set utimers "" }
	}

	upvar [getns]::utimers utimers

	set idx [lsearch $utimers "* $timerID"]
	if {$idx == -1} { return }

	set timer [lindex $utimers $idx]

	if {[catch [lindex $timer 1] error]} {
		foreach user [bncuserlist] {
			if {[getbncuser $user admin]} {
				bncnotc "Error in utimer $timerID: $error"
			}
		}
	}

	set idx [lsearch $utimers "* $timerID"]
	if {$idx == -1} { return }

	set utimers [lreplace $utimers $idx $idx]
}

proc timer {minutes tclcommand} {
	namespace eval [getns] {
		if {![info exists timers]} { set timers "" }
		if {![info exists timeridx]} { set timeridx 0 }
	}

	upvar [getns]::timers timers
	upvar [getns]::timeridx timeridx

	set time [expr [clock seconds] + $minutes * 60]
	incr timeridx
	set id "timer$timeridx"

	set timer [list $time $tclcommand $id]

	lappend timers $timer

	internaltimer [expr $minutes * 60] 0 sbnc:runthistimer [list [getctx] $id]

	return $id
}

proc utimer {seconds tclcommand} {
	namespace eval [getns] {
		if {![info exists utimers]} { set utimers "" }
		if {![info exists utimeridx]} { set utimeridx 0 }
	}

	upvar [getns]::utimers utimers
	upvar [getns]::utimeridx utimeridx

	set time [expr [clock seconds] + $seconds]
	incr utimeridx
	set id "timer$utimeridx"

	set timer [list $time $tclcommand $id]

	lappend utimers $timer

	internaltimer $seconds 0 sbnc:runthisutimer [list [getctx] $id]

	return $id
}

proc timers {} {
	namespace eval [getns] {
		if {![info exists timers]} { set timers "" }
	}

	upvar [getns]::timers timers
	set temptimers ""

	foreach ut $timers {
		lappend temptimers [list [expr [lindex $ut 0] - [clock seconds]] [lindex $ut 1] [lindex $ut 2]]
	}

	return $temptimers
}

proc utimers {} {
	namespace eval [getns] {
		if {![info exists utimers]} { set utimers "" }
	}

	upvar [getns]::utimers utimers
	set temptimers ""

	foreach ut $utimers {
		lappend temptimers [list [expr [lindex $ut 0] - [clock seconds]] [lindex $ut 1] [lindex $ut 2]]
	}

	return $temptimers
}

proc killtimer {timerID} {
	namespace eval [getns] {
		if {![info exists timers]} { set timers "" }
	}

	upvar [getns]::timers timers

	set idx [lsearch $timers "* $timerID"]

	if {$idx != -1} {
		set timers [lreplace $timers $idx $idx]

		internalkilltimer sbnc:runthistimer [list [getctx] $timerID]]
	}

	return
}

proc killutimer {timerID} {
	namespace eval [getns] {
		if {![info exists utimers]} { set utimers "" }
	}

	upvar [getns]::utimers utimers

	set idx [lsearch $utimers "* $timerID"]

	if {$idx != -1} {
		set utimers [lreplace $utimers $idx $idx]

		internalkilltimer sbnc:runthisutimer [list [getctx] $timerID]]
	}

	return
}
