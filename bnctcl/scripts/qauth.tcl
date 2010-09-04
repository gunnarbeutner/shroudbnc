# shroudBNC - an object-oriented framework for IRC
# Copyright (C) 2005-2007,2010 Gunnar Beutner
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

proc qauth:migrate_users {} {
	foreach user [bncuserlist] {
		set quser [getbncuser $user tag quser]
		set qpass [getbncuser $user tag qpass]
		set qx [getbncuser $user tag qx]

		if {$quser == "" || $qpass == ""} {
			continue
		}

		setbncuser $user tag authuser $quser
		setbncuser $user tag authpass $qpass
		setbncuser $user tag auth on

		setbncuser $user tag quser ""
		setbncuser $user tag qpass ""

		if {[string equal -nocase $qx "on"]} {
			set automodes [getbncuser $user automodes]
			append automodes "+x"
			setbncuser $user automodes $automodes
		}
	}
}

qauth:migrate_users

source scripts/auth.tcl
