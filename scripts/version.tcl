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

# configuration variables: ::versionreply

internalbind pulse sbnc:versionflood

foreach user [split $::versionreply] {
	bind ctcp - version sbnc:ctcpversion
}

proc sbnc:versionflood {time} {
	global versionreply

	if {$time % 10 == 0} {
		foreach user [split $versionreply] {
			set flood [getbncuser $user tag flood]

			if {$flood != "" && $flood > 0} {
				incr flood -1

				setbncuser $user tag flood $flood
			}
		}
	}
}

proc sbnc:ctcpversion {nick host hand dest key text} {
	set flood [getbncuser [getctx] tag flood]

	if {$flood == ""} {
		set flood 0
	}

	if {$flood < 2} {
		puthelp "NOTICE $nick :\001VERSION shroudBNC [lindex [split [bncversion]] 0] with TCL support"

		incr flood
		setbncuser [getctx] tag flood $flood
	}

	haltoutput
}
