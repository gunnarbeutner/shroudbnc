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

	foreach user [bncuserlist] {
		if {![string equal -nocase $user $ctx]} {
			setctx $user
			sbnc:callbinds "bot" - {} [lindex [split $message] 0] $ctx [lindex [split $message] 0] [join [lrange [split $message] 1 end]]
		}
	}

	setctx $ctx
}

proc bots {} {
	set i [lsearch -exact [bncuserlist] [getctx]]

	return [lreplace [bncuserlist] $i $i]
}

proc botlist {} {
	set bl [list]

	foreach user [bncuserlist] {
		if {![string equal -nocase $user [getctx]]} {
			lappend bl [list $user [getctx] [bncversion] "-"]
		}
	}

	return $bl
}

proc islinked {bot} {
	if {[lsearch -exact [bncuserlist] $bot] != -1} {
		return 1
	} else {
		return 0
	}
}
