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

internalbind pulse sbnc:tcltimers

if {![info exists sbnc:timerinit]} {
	set sbnc:timers {}
	set sbnc:utimers {}

	set sbnc:timeridx 0
	set sbnc:utimeridx 0

	set sbnc:timerinit 1
}

# entry point to sbnc's timer system
proc sbnc:tcltimers {time} {
	global sbnc:utimers sbnc:timers

	foreach timer ${sbnc:utimers} {
		if {$time >= [lindex $timer 0]} {
			catch {eval [lindex $timer 1]}
			killutimer [lindex $timer 2]
		}
	}

	foreach timer ${sbnc:timers} {
		if {$time >= [lindex $timer 0]} {
			catch {eval [lindex $timer 1]}
			killtimer [lindex $timer 2]
		}
	}
}

proc timer {minutes tclcommand} {
	global sbnc:timers sbnc:timeridx

	set time [expr [clock seconds] + $minutes * 60]
	incr sbnc:timeridx
	set id "timer${sbnc:timeridx}"

	set timer [list $time $tclcommand $id]

	lappend sbnc:utimers $timer

	return $id
}

proc utimer {seconds tclcommand} {
	global sbnc:utimers sbnc:utimeridx

	set time [expr [clock seconds] + $seconds]
	incr sbnc:utimeridx
	set id "timer${sbnc:utimeridx}"

	set timer [list $time $tclcommand $id]

	lappend sbnc:utimers $timer

	return $id
}

proc timers {} {
	global sbnc:timers
	set temptimers ""

	foreach ut ${sbnc:timers} {
		lappend temptimers [list [expr [lindex $ut 0] - [clock seconds]] [lindex $ut 1] [lindex $ut 2]]
	}

	return $temptimers

}

proc utimers {} {
	global sbnc:utimers
	set temptimers ""

	foreach ut ${sbnc:utimers} {
		lappend temptimers [list [expr [lindex $ut 0] - [clock seconds]] [lindex $ut 1] [lindex $ut 2]]
	}

	return $temptimers
}

proc killtimer {timerID} {
	global sbnc:timers

	set idx [lsearch ${sbnc:timers} "* $timerID"]

	if {$idx != -1} {
		set sbnc:timers [lreplace ${sbnc:timers} $idx $idx]
	}
}

proc killutimer {timerID} {
	global sbnc:utimers

	set idx [lsearch ${sbnc:utimers} "* $timerID"]

	if {$idx != -1} {
		set sbnc:utimers [lreplace ${sbnc:utimers} $idx $idx]
	}
}