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

proc putbot {bot message} {
	if {[islinked $bot]} {
		set ctx [getctx]

		setctx $bot
		sbnc:callbinds "bot" - {} [lindex [split $message] 0] $ctx [lindex [split $message] 0] [join [lrange [split $message] 1 end]]
		setctx $ctx
	}
}

proc putallbots {message} {
	set ctx [getctx]

	foreach user [bots] {
		setctx $user
		sbnc:callbinds "bot" - {} [lindex [split $message] 0] $ctx [lindex [split $message] 0] [join [lrange [split $message] 1 end]]
	}

	setctx $ctx
}

proc bots {} {
	set users [bncuserlist]

	set i [lsearch -exact [string tolower $users] [string tolower [getctx]]]

	return [lreplace $users $i $i]
}

proc botlist {} {
	set bots [list]

	foreach user [bncuserlist] {
		if {![string equal -nocase $user [getctx]]} {
			lappend bots [list $user [getctx] [bncversion] "-"]
		}
	}

	return $bots
}

proc islinked {bot} {
	if {[bncvaliduser $bot]} {
		return 1
	} else {
		return 0
	}
}
