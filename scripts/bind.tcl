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

internalbind modec sbnc:modechange
internalbind svrconnect sbnc:svrconnect
internalbind svrdisconnect sbnc:svrdisconnect
internalbind svrlogon sbnc:svrlogon
internalbind prerehash sbnc:prerehash
internalbind postrehash sbnc:postrehash
internalbind channelsort sbnc:channelsort

if {![info exists ::channelsorthandler]} {
	set ::channelsorthandler "stricmp"
}

proc sbnc:nickfromhost {host} {
	return [lindex [split $host "!"] 0]
}

proc sbnc:sitefromhost {host} {
	return [lindex [split $host "!"] 1]
}

proc sbnc:svrconnect {client} {
	callevent "connect-server"
}

proc sbnc:svrdisconnect {client} {
	callevent "disconnect-server"
}

proc sbnc:svrlogon {client} {
	callevent "init-server"
}

proc sbnc:prerehash {} {
	foreach user [bncuserlist] {
		setctx $user
		callevent "prerehash"
	}
}

proc sbnc:postrehash {} {
	foreach user [bncuserlist] {
		setctx $user
		callevent "postrehash"
	}
}

proc sbnc:bindpulse {} {
	foreach user [bncuserlist] {
		setctx $user

		foreach chan [channels] {
			if {[botonchan $chan] && ![botisop $chan]} {
				sbnc:callbinds "need" - $chan "$chan op" $chan "op"
			}
		}

		set time [unixtime]

		set minute [clock format $time -format "%M"]
		set hour [clock format $time -format "%H"]
		set day [clock format $time -format "%d"]
		set month [clock format $time -format "%m"]
		set year [clock format $time -format "%Y"]

		sbnc:callbinds "time" - {} "$minute $hour $day $month $year" $minute $hour $day $month $year
	}
}

proc sbnc:modechange {client parameters} {
	set source [lindex $parameters 0]
	set channel [lindex $parameters 1]
	set mode [lindex $parameters 2]
	set targ [lindex $parameters 3]

	set hand [finduser $source]
	set flags $hand

	set nick [sbnc:nickfromhost $source]
	set host [sbnc:sitefromhost $source]

	if {![onchan $nick $channel]} {
		set nick ""
		set host $source
		set hand "*"
		set flags $hand
	}

	if {[llength $parameters] < 4} {
		sbnc:callbinds "mode" $flags $channel "$channel $mode" $nick $host $hand $channel $mode
	} else {
		sbnc:callbinds "mode" $flags $channel "$channel $mode" $nick $host $hand $channel $mode $targ
	}
}

proc sbnc:rawserver {client parameters} {
	if {[llength [binds]] == 0} { return }

	set source [lindex $parameters 0]
	set nick [sbnc:nickfromhost $source]
	set site [sbnc:sitefromhost $source]
	set verb [string tolower [lindex $parameters 1]]
	set targ [lindex $parameters 2]
	set opt [lindex $parameters 3]

	# todo: implement
	set hand [finduser $source]
	set flags $hand

	if {[llength [split [lindex $parameters end]]] > 1} {
		set last ":[lindex $parameters end]"
	} else {
		set last [lindex $parameters end]
	}

	sbnc:callbinds "raw" - "" $verb $source $verb "[join [lrange $parameters 2 end-1]] $last"

	switch $verb {
		"privmsg" {
			if {[regexp "\001(\[^ \]+)(| (.*?))\001" $opt foo verb text]} {
				sbnc:callbinds "ctcp" $flags $targ $verb $nick $site $hand $targ $verb $text
			} else {
				if {[validchan $targ] || [botonchan $targ]} {
					sbnc:callbinds "pub" $flags $targ [lindex [split $opt] 0] $nick $site $hand $targ [join [lrange [split $opt] 1 end]]
					sbnc:callbinds "pubm" $flags $targ "$targ $opt" $nick $site $hand $targ $opt
				} else {
					sbnc:callbinds "msg" $flags "" [lindex [split $opt] 0] $nick $site $hand [join [lrange [split $opt] 1 end]]
					sbnc:callbinds "msgm" $flags "" $opt $nick $site $hand $opt
				}
			}
		}
		"notice" {
			if {[regexp "\001(\[^ \]+)(| (.*?))\001" $opt foo verb text]} {
				sbnc:callbinds "ctcr" $flags $targ $verb $nick $site $hand $targ $verb $text
			} else {
				sbnc:callbinds "notc" $flags $targ $opt $nick $site $hand $opt $targ
			}
		}
		"join" {
			sbnc:callbinds "join" $flags $targ "$targ $source" $nick $site $hand $targ
		}
		"part" {
			sbnc:callbinds "part" $flags $targ "$targ $source" $nick $site $hand $targ $opt
		}
		"quit" {
			if {![regexp {^\S+\.\S+ \S+\.\S+$} $targ]} {
				foreach c [internalchannels] {
					if {[onchan $nick $c]} {
						sbnc:callbinds "sign" $flags $c "$c $source" $nick $site $hand $c $targ
					}
				}
			}
		}
		"topic" {
			sbnc:callbinds "topc" $flags $targ "$targ $opt" $nick $site $hand $targ $opt
		}
		"kick" {
			sbnc:callbinds "kick" $flags $targ "$targ $opt" $nick $site $hand $targ $opt [lindex $parameters 4]
		}
		"nick" {
			foreach c [internalchannels] {
				if {[onchan $targ $c]} {
					sbnc:callbinds "nick" $flags $c "$c $targ" $nick $site $hand $c $targ
				}
			}
		}
		"wallops" {
			sbnc:callbinds "wall" - "" $targ $source [join [lrange $parameters 2 end]]
		}
		"471" {
			sbnc:callbinds "need" - $opt "$opt limit" $opt "limit"
		}
		"473" {
			sbnc:callbinds "need" - $opt "$opt invite" $opt "invite"
		}
		"474" {
			sbnc:callbinds "need" - $opt "$opt unban" $opt "unban"
		}
		"475" {
			sbnc:callbinds "need" - $opt "$opt key" $opt "key"
		}
		"477" {
			sbnc:callbinds "need" - $opt "$opt register" $opt "register"
		}
		"invite" {
			if {[lsearch -exact [string tolower [channels]] [string tolower $opt]] != -1} {
				puthelp "JOIN $opt"
			}
		}
	}
}

proc sbnc:callbinds {type flags chan mask args} {
	namespace eval [getns] {
		if {![info exists binds]} { set binds "" }
		if {![info exists binds]} { set clastbind "" }
	}

	upvar [getns]::binds binds
	upvar [getns]::clastbind clastbind

	set allbinds $binds

	set old [getctx]
	setctx ":any"
	namespace eval [getns] {
		if {![info exists binds]} { set binds "" }
	}

	upvar [getns]::binds binds
	setctx $old

	foreach bind $binds {
		lappend allbinds $bind
	}

	set count 0

	foreach bind $allbinds {
		set t [lindex $bind 0]
		set f [lindex $bind 1]
		set m [lindex $bind 2]
		set u [lindex $bind 3]
		set p [lindex $bind 4]

		if {[string equal -nocase $t $type] && [string match -nocase $m $mask] && [matchattr $flags $f $chan]} {
			set clastbind $m

			set errcode [catch [list eval $p $args] error]

			if {$errcode} {
				bncnotc "Error in tcl bind $t $f $m called: $p ($args)"

				foreach line [split $error \n] {
					bncnotc $line
				}
			}

			incr count
		}
	}

	return $count
}

proc bind {type flags mask {procname ""}} {
	set nonstackable [list msg dcc fil pub bot note]
	set result [list]

	namespace eval [getns] {
		if {![info exists binds]} { set binds "" }
	}

	upvar [getns]::binds binds

	foreach bind $binds {
		if {$procname != ""} {
			if {[lsearch -exact $nonstackable [string tolower $type]] != -1 && [string equal -nocase $mask [lindex $bind 2]]} {
				unbind [lindex $bind 0] [lindex $bind 1] [lindex $bind 2] [lindex $bind 4]
			}
			if {[string equal -nocase $type [lindex $bind 0]] && [string equal -nocase $mask [lindex $bind 2]] && [string equal -nocase $procname [lindex $bind 4]]} {
				unbind [lindex $bind 0] [lindex $bind 1] [lindex $bind 2] [lindex $bind 4]
			}
		} elseif {$procname == "" && [string equal -nocase $type [lindex $bind 0]] && [string equal -nocase $mask [lindex $bind 2]]} {
			lappend result $bind
		}
	}

	if {$procname == ""} {
		return $result
	}

	unbind $type $flags $mask $procname

	lappend binds [list $type $flags $mask 0 $procname]

	internalbind server sbnc:rawserver * [getctx]

	if {[string equal -nocase $type "need"] || [string equal -nocase $type "time"]} {
		internaltimer 60 1 sbnc:bindpulse
	}

	return $mask
}

proc bindall {type flags mask {procname ""}} {
	set old [getctx]
	setctx ":any"
	set result [bind $type $flags $mask $procname]
	setctx $old

	return $result
}

proc unbind {type flags mask procname} {
	namespace eval [getns] {
		if {![info exists binds]} { set binds "" }
	}

	upvar [getns]::binds binds

	if {$procname == ""} {
		set list ""

		foreach bind $binds {
			if {[lindex $bind 0]} {
				lappend list $bind
			}
		}

		return $list
	}

	set newbinds ""

	foreach bind $binds {
		set t [lindex $bind 0]
		set f [lindex $bind 1]
		set m [lindex $bind 2]
		set u [lindex $bind 3]
		set p [lindex $bind 4]

		if {![string equal -nocase $t $type] || ![string equal -nocase $f $flags] || ![string equal -nocase $m $mask] || ![string equal -nocase $p $procname]} {
			lappend newbinds $bind
		}
	}

	set binds $newbinds

	if {[llength $binds] == 0} {
		internalunbind server sbnc:rawserver * [getctx]
	}

	return $mask
}

proc unbindall {type flags mask procname} {
	set old [getctx]
	setctx ":any"
	set result [unbind $type $flags $mask $procname]
	setctx $old

	return $result
}

proc binds {{pattern ""}} {
	namespace eval [getns] {
		if {![info exists binds]} { set binds "" }
	}

	upvar [getns]::binds binds

	if {$pattern == ""} {
		return $binds
	} else {
		set out ""

		foreach bind $binds {
			set type [lindex $bind 0]
			set mask [lindex $bind 2]

			if {[string match -nocase $pattern $mask] || [string match -nocase $pattern $type]} {
				lappend out $bind
			}
		}

		return $out
	}
}

proc bindsall {{pattern ""}} {
	set old [getctx]
	setctx ":any"
	set result [binds $pattern]
	setctx $old

	return $result
}

proc callevent {event} {
	sbnc:callbinds "evnt" - {} $event $event
}

proc sbnc:channelsort {client params} {
	setchannelsortvalue [$::channelsorthandler [lindex $params 0] [lindex $params 1]]
}

proc setchannelsorthandler {handler} {
	set ::channelsorthandler $handler
}

proc getchannelsorthandler {} {
	return $::channelsorthandler
}
