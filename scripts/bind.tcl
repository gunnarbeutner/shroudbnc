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

internalbind server sbnc:rawserver

proc sbnc:nickfromhost {host} {
	return [lindex [split $host "!"] 0]
}

proc sbnc:sitefromhost {host} {
	return [lindex [split $host "!"] 1]
}

proc sbnc:rawserver {client parameters} {
	set source [lindex $parameters 0]
	set nick [sbnc:nickfromhost $source]
	set site [sbnc:sitefromhost $source]
	set verb [string tolower [lindex $parameters 1]]
	set targ [lindex $parameters 2]
	set opt [lindex $parameters 3]

	# todo: implement
	#set hand [finduser $source]
	#set flags [chattr $hand]
	set hand "shroud"
	set flags "n"

	switch $verb {
		"privmsg" {
			if {[validchan $targ] || [lsearch -exact [string tolower [internalchannels]] [string tolower $targ]] != -1} {
				sbnc:callbinds "pub" $flags [lindex [split $opt] 0] $nick $site $hand $targ [join [lrange [split $opt] 1 end]]
				sbnc:callbinds "pubm" $flags "$targ $opt" $nick $site $hand $targ $opt
			} else {
				sbnc:callbinds "msg" $flags [lindex [split $opt] 0] $nick $site $hand [join [lrange [split $opt] 1 end]]
				sbnc:callbinds "pubm" $flags $opt $nick $site $hand $opt
			}
		}
		"notice" {

		}
		"join" {
			sbnc:callbinds "join" $flags "$targ $source" $nick $site $hand $targ
		}
		"part" {
			sbnc:callbinds "part" $flags "$targ $source" $nick $site $hand $targ $opt
		}
		"quit" {

		}
		"topic" {
			sbnc:callbinds "topc" $flags "$targ $opt" $nick $site $hand $targ $opt
		}
		"kick" {

		}
		"nick" {

		}
		"mode" {

		}
	}

	puts [join $parameters]
}

proc sbnc:callbinds {type flags mask args} {
	if {![info exists [getns]::binds]} {
		set [getns]::binds ""
	}

	upvar [getns]::binds binds

	foreach bind $binds {
		set t [lindex $bind 0]
		set f [lindex $bind 1]
		set m [lindex $bind 2]
		set u [lindex $bind 3]
		set p [lindex $bind 4]

		# todo: match flags!
		if {[string equal -nocase $t $type] && [string match -nocase $m $mask]} {
			catch [list eval $p $args]
		}
	}
}

proc bind {type flags mask {procname ""}} {
	if {![info exists [getns]::binds]} {
		set [getns]::binds ""
	}

	upvar [getns]::binds binds

	unbind $type $flags $mask $procname

	lappend binds [list $type $flags $mask 0 $procname]

	return $mask
}

proc unbind {type flags mask procname} {
	if {![info exists [getns]::binds]} {
		set [getns]::binds ""
	}

	upvar [getns]::binds binds

	set newbinds ""

	if {$procname == ""} {
		set list ""

		foreach bind $binds {
			if {[lindex $bind 0]} {
				lappend list $bind
			}
		}

		return $list
	}


	foreach bind $binds {
		set t [lindex $bind 0]
		set f [lindex $bind 1]
		set m [lindex $bind 2]
		set u [lindex $bind 3]
		set p [lindex $bind 4]

		if {![string equal -nocase $t $type] && ![string equal -nocase $f $flags] && ![string equal -nocase $m $mask] && ![string equal -nocase $p $procname]} {
			lappend newbinds $bind
		}
	}

	set binds $newbinds

	return $mask
}

proc binds {{mask ""}} {
	if {![info exists [getns]::binds]} {
		set [getns]::binds ""
	}

	upvar [getns]::binds binds

	if {$mask == ""} {
		return $binds
	} else {
		set out ""

		foreach bind $binds {
			set m [lindex $bind 2]

			if {[string match -nocase $mask $m]} {
				lappend out $bind
			}
		}

		return $out
	}
}

proc callevent {event} {
	sbnc:callbinds "evnt" - {} $event
}
