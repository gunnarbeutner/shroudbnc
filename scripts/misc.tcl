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
	return [getbncuser [getctx] nick]
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
