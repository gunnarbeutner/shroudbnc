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

if {![info exists ::varinitonce]} {
	set botnick "fish"
	set botname "fish"
	set server "fish"
	set serveraddress "fish"
	set version "fish"
	set numversion "fish"
	set uptime "fish"
	set server-online "fish"
	set lastbind "fish"
	set isjuped "fish"
	set handlen "fish"
	set config "fish"
	set userfile "fish"
	set chanfile "fish"
	set botnet-nick "fish"

	set ::varinitonce 1
}

trace variable ::botnick rwu sbnc:tracevars-botnick
trace variable ::botname rwu sbnc:tracevars-botname
trace variable ::server rwu sbnc:tracevars-server
trace variable ::serveraddress rwu sbnc:tracevars-serveraddress
trace variable ::version rwu sbnc:tracevars-version
trace variable ::numversion rwu sbnc:tracevars-numversion
trace variable ::uptime rwu sbnc:tracevars-uptime
trace variable ::server-online rwu sbnc:tracevars-server-online
trace variable ::lastbind rwu sbnc:tracevars-lastbind
trace variable ::isjuped rwu sbnc:tracevars-isjuped
trace variable ::handlen rwu sbnc:tracevars-handlen
trace variable ::config rwu sbnc:tracevars-config
trace variable ::userfile rwu sbnc:tracevars-userfile
trace variable ::chanfile rwu sbnc:tracevars-chanfile
trace variable ::botnet-nick rwu sbnc:tracevars-botnet-nick

proc sbnc:tracevars-botnick {name item operation} {
	if {$operation == "r"} {
		set ::botnick [getcurrentnick]
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-botname {name item operation} {
	if {$operation == "r"} {
		set nick [getcurrentnick]

		if {[getchanhost $nick] != ""} {
			set ::botname $nick![getchanhost $nick]
		} else {
			set ::botname "$nick![getctx]@unknown"
		}
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-server {name item operation} {
	if {$operation == "r"} {
		set s [getbncuser [getctx] realserver]:[getbncuser [getctx] port]

		if {[getbncuser [getctx] realserver] != ""} {
			set ::server $s
		} else {
			return "sbnc:6667"
		}
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-serveraddress {name item operation} {
	if {$operation == "r"} {
		set ::serveraddress [getbncuser [getctx] server]:[getbncuser [getctx] port]
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-version {name item operation} {
	if {$operation == "r"} {
		set ::version [bncversion]
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-numversion {name item operation} {
	if {$operation == "r"} {
		set ::numversion [bncnumversion]
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-uptime {name item operation} {
	if {$operation == "r"} {
		set ::uptime [bncuptime]
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-server-online {name item operation} {
	if {$operation == "r"} {
		set ::server-online [expr [clock seconds] - [getbncuser [getctx] uptime]]
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-lastbind {name item operation} {
	if {$operation == "r"} {
		namespace eval [getns] {
			if {![info exists binds]} { set clastbind "" }
		}

		upvar [getns]::clastbind clastbind

		set ::lastbind $clastbind
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-isjuped {name item operation} {
	if {$operation == "r"} {
		set ::isjuped 0
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-handlen {name item operation} {
	if {$operation == "r"} {
		set ::handlen 15
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-config {name item operation} {
	if {$operation == "r"} {
		set ::config "sbnc.tcl"
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-userfile {name item operation} {
	if {$operation == "r"} {
		set ::userfile "users/[getctx].user"
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-chanfile {name item operation} {
	if {$operation == "r"} {
		set ::chanfile "users/[getctx].chan"
	} else {
		return -code error "variable is read-only"
	}
}

proc sbnc:tracevars-botnet-nick {name item operation} {
	if {$operation == "r"} {
		set ::botnet-nick [getctx]
	} else {
		return -code error "variable is read-only"
	}
}
