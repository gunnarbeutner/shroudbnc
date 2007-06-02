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

proc sbncnick {} {
	return [getcurrentnick]
}

proc isbotnick {nick} {
	return [string equal -nocase $nick [sbncnick]]
}

proc botisop {{channel ""}} {
	return [isop [sbncnick] $channel]
}

proc botishalfop {{channel ""}} {
	return [ishalfop [sbncnick] $channel]
}

proc botisvoice {{channel ""}} {
	return [isvoice [sbncnick] $channel]
}

proc botonchan {{channel ""}} {
	return [onchan [sbncnick] $channel]
}

proc unixtime {} {
	return [clock seconds]
}

proc ctime {timeval} {
	return [clock format $timeval]
}

proc strftime {formatstring {time ""}} {
	if {$time == ""} {
		return [clock format [clock seconds] -format $formatstring]
	} else {
		return [clock format $time -format $formatstring]
	}
}

proc putkick {channel nicks {reason ""}} {
	foreach nick [split $nicks ","] {
		putquick "KICK $channel $nick :$reason"
	}
}

proc bncnotc {text} {
	putclient ":-sBNC!core@shroudbnc.info PRIVMSG $::botnick :$text"
}

proc bncrnotc {text} {
	putclient ":-sBNC!core@shroudbnc.info NOTICE $::botnick :$text"
}

proc getchanidle {nick chan} {
	return [expr [internalgetchanidle $nick $chan] / 60]
}

proc duration {seconds} {
	set result ""

	set dur [expr 365*24*60*60]
	if {[format %.0f [expr $seconds/$dur]] != 0} {
		set years [format %.0f [expr $seconds/$dur]]
		if {$years == "1"} {
			set result "$result$years year "
		} {set result "$result$years years "}
		set seconds [expr $seconds - $years*$dur]
	}

	set dur [expr 7*24*60*60]
	if {[format %.0f [expr $seconds/$dur]] != 0} {
		set weeks [format %.0f [expr $seconds/$dur]]
		if {$weeks == "1"} {
			set result "$result$weeks week "
		} {set result "$result$weeks weeks "}
		set seconds [expr $seconds - $weeks*$dur]
	}

	set dur [expr 24*60*60]
	if {[format %.0f [expr $seconds/$dur]] != 0} {
		set days [format %.0f [expr $seconds/$dur]]
		if {$days == "1"} {
			set result "$result$days day "
		} {set result "$result$days days "}
		set seconds [expr $seconds - $days*$dur]
	}

	set dur [expr 60*60]
	if {[format %.0f [expr $seconds/$dur]] != 0} {
		set hours [format %.0f [expr $seconds/$dur]]
		if {$hours == "1"} {
			set result "$result$hours hour "
		} {set result "$result$hours hours "}
		set seconds [expr $seconds - $hours*$dur]
	}

	set dur 60
	if {[format %.0f [expr $seconds/$dur]] != 0} {
		set minutes [format %.0f [expr $seconds/$dur]]
		if {$minutes == "1"} {
			set result "$result$minutes minute "
		} {set result "$result$minutes minutes "}
		set seconds [expr $seconds - $minutes*$dur]
	}

	if {$seconds != 0} {
		if {$seconds == "1"} {
			set result "$result$seconds second "
		} {set result "$result$seconds seconds "}
	}

	if {$result == ""} {
		set result "0 seconds"
	}

	return $result
}

proc chanlist {channel {flags ""}} {
	set res [list]

	foreach nick [internalchanlist $channel] {
		if {$flags == "" || [matchattr [nick2hand $nick] $flags $channel]} {
			lappend res $nick
		}
	}

	return $res
}

proc nick2hand {nick {channel ""}} {
	if {($channel != "" && [onchan $nick $channel]) || [onchan $nick]} {
		return [finduser $nick![getchanhost $nick]]
	} else {
		return ""
	}
}

proc hand2nick {handle {channel ""}} {
	if {$channel != ""} {
		set channels [list $channel]
	} else {
		set channels [channels]
	}

	foreach chan $channels {
		foreach user [internalchanlist $chan] {
			set u [finduser $user![getchanhost $user]]

			if {[string equal $u $handle]} {
				return $user
			}
		}
	}

	return ""
}

proc handonchan {handle {channel ""}} {
	if {[hand2nick $handle $channel] != ""} {
		return 1
	} else {
		return 0
	}
}

proc putcmdlog {text} {
	putlog "(command) $text"
}

proc putxferlog {text} {
	putlog "(xfer) $text"
}

proc putloglev {level channel text} {
	putlog "(level: $level, channel: $channel) $text"
}

proc int2ip {num} {
	binary scan [binary format I $num] cccc a b c d

	return [format %d.%d.%d.%d [expr $a&255] [expr $b&255] [expr $c&255] [expr $d&255]]
}

proc ip2int {ip} {
	foreach {a b c d} [split $ip .] {
		break
	}

	return [expr ($a<<8+$b<<8+$c<<8)+$d]
}

proc stripcodes {flags text} {
	variable result

	regsub -all "\[\002\017\]" $text "" result
	regsub -all "\003\[0-9\]\[0-9\]?\(,\[0-9\]\[0-9\]?\)?" $result "" result	
	regsub -all "\[\003\017\026\037\]" $result "" result

	return $result
}

proc dnslookup {args} {
	if {[llength $args] < 2} {
		return -code error "wrong # args: shrould be dnslookup ip-address/hostname proc ?arg1? ... ?argN?"
	}

	set host [lindex $args 0]
	set proc [lindex $args 1]
	set arg [lrange $args 2 end]

	if {[regexp {\d*\.\d*\.\d*\.\d*} $host]} {
		set reverse 1
		set ipv6 0
	} elseif {[string first ":" $host] != -1} {
		set reverse 1
		set ipv6 1
	} else {
		set reverse 0
		set ipv6 0
	}

	internaldnslookup $host dnslookup:callback $reverse $ipv6 [list $proc $arg]
}

proc dnslookup:callback {addr host status cookie} {
	set proc [lindex $cookie 0]

	set cmd [list $proc $addr $host $status]

	foreach arg [lindex $cookie 1] {
		lappend cmd $arg
	}

	eval $cmd
}
