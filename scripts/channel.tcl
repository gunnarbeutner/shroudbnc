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

internalbind pulse sbnc:channelpulse
internalbind unload sbnc:channelflush

setudef int inactive

proc sbnc:channelflush {} {
	foreach user [bncuserlist] {
		setctx $user
		savechannels

		foreach channel [channels] {
			if {![botonchan $channel] && ![channel get $channel inactive]} {
				puthelp "JOIN $channel"
			}
		}
	}
}

proc sbnc:channelpulse {time} {
	if {$time % 120 == 0} {
		sbnc:channelflush
	}
}

proc channel {option chan args} {
	namespace eval [getns] {
		if {![info exists channels]} { array set channels {} }
		if {![info exists chanoptions]} { array set chanoptions {} }
	}

	upvar [getns]::channels channels
	upvar [getns]::chanoptions chanoptions

	if {[info exists channels($chan)]} {
		array set channel $channels($chan)
	} else {
		array set channel {}
	}

	switch [string tolower $option] {
		add {
			set channels($chan) [join $args]

			puthelp "JOIN $chan"

			return 1
		}
		set {
			if {[llength $args] < 2} { return -code error "Too few parameters" }

			if {![info exists chanoptions([lindex $args 0])]} {
				return -code error "No such option."
			} elseif {[string equal -nocase $chanoptions([lindex $args 0]) "int"] && ![string is digit [lindex $args 1]]} {
				return -code error "Value is not an integer."
			} elseif {[string equal -nocase $chanoptions([lindex $args 0]) "flag"] && [lindex $args 1] != "1" && [lindex $args 1] != "0"} {
				return -code error "Value is not a flag."
			}

			set channel([lindex $args 0]) [lindex $args 1]

			set channels($chan) [array get channel]

			return [lindex $args 1]
		}
		info {
			return $channels($chan)
		}
		get {
			if {[llength $args] < 1} { return -code error "Too few parameters" }

			if {![info exists chanoptions([lindex $args 0])]} {
				return -code error "No such option."
			} elseif {[info exists channel([lindex $args 0])]} {
				return $channel([lindex $args 0])
			} else {
				if {$chanoptions(inactive) == "int" || $chanoptions(inactive) == "flag"} {
					return 0
				} else {
					return {}
				}
			}
		}
		remove {
			unset channels($chan)

			return 1
		}
		default {
			return -code error "Option should be one of: add set info get remove"
		}
	}
}

proc savechannels {} {
	namespace eval [getns] {
		if {![info exists channels]} { array set channels {} }
	}

	upvar [getns]::channels channels

	set file [open $::chanfile "w"]

	foreach channel [array names channels] {
		puts $file "channel add $channel \{ $channels($channel) \}"
	}

	close $file

	return 1
}

proc loadchannels {} {
	namespace eval [getns] {
		array set channels {}
	}

	catch [list source $::chanfile]
}

proc channels {} {
	namespace eval [getns] {
		if {![info exists channels]} { array set channels {} }
	}

	upvar [getns]::channels channels

	return [array names channels]
}

proc validchan {channel} {
	namespace eval [getns] {
		if {![info exists channels]} { array set channels {} }
	}

	upvar [getns]::channels channels

	if {[info exists channels([string tolower $channel])]} {
		return 1
	} else {
		return 0
	}
}

proc isdynamic {channel} {
	return [validchan $channel]
}

proc setudef {type name} {
	namespace eval [getns] {
		if {![info exists chanoptions]} { array set chanoptions {} }
	}

	upvar [getns]::chanoptions chanoptions

	set chanoptions($name) $type

	return
}

proc renudef {type oldname newname} {
	namespace eval [getns] {
		if {![info exists channels]} { array set channels {} }
		if {![info exists chanoptions]} { array set chanoptions {} }
	}

	upvar [getns]::chanoptions chanoptions
	upvar [getns]::channels channels

	if {[info exists chanoptions($newname)]} {
		return -code error "$newname is already a channel option."
	}

	if {[info exists chanoptions($oldname)]} {
		set chanoptions($newname) $chanoptions($oldname)
		unset chanoptions($oldname)
	} else {
		setudef $type $newname
	}

	foreach channame [array names channels] {
		array set channel $channels($channame)

		if {[info exists channel($oldname)]} {
			set channel($newname) $channel($oldname)
			unset channel($oldname)
		}

		set channels($channame) [array get channel]
	}

	return
}

proc deludef {type name} {
	namespace eval [getns] {
		if {![info exists chanoptions]} { array set chanoptions {} }
	}

	upvar [getns]::chanoptions chanoptions

	if {[info exists chanoptions($name)]} {
		unset chanoptions($name)
	}

	return
}

if {![info exists sbnc:channelinit]} {
	foreach user [bncuserlist] {
		setctx $user
		loadchannels
	}

	set sbnc:channelinit 1
}
