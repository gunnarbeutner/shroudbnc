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

# Interface 2

# There are no configurable options. The RPC interface (Version 2) uses
# the same port like shroudBNC.

internalbind client iface:hijackclient RPC_IFACE

proc iface:hijackclient {client params} {
	if {$client == "" && [string equal -nocase [lindex $params 0] "RPC_IFACE"]} {
		set idx [hijacksocket]
		control $idx iface:line
		putdcc $idx RPC_IFACE_OK
	}
}

set ::ifacecmds [list]

proc registerifacecmd {module command proc {accessproc "access:anyone"} {paramcount -1}} {
	global ifacecmds

	if {[catch [list info args $proc]] && $paramcount < 0} {
		return -code error "You need to specify a parameter count for native functions."
	}

	foreach cmd $ifacecmds {
		if {[string equal [lindex $cmd 0] $module] && [string equal [lindex $cmd 1] $command]} {
			unregisterifacecmd $module $command
		}
	}

	set index [lsearch -exact [reflect:commands] $command]

	if {$index != -1} {
		set cmd [lindex $ifacecmds $index]

		if {[catch [list info args $proc] arguments]} {
			set count $paramcount
		} else {
			set count [llength $arguments]
		}

		if {[lindex $cmd 4] != $paramcount} {
			set out "Invalid parameter count. Another function called \"$command\" which has "
			append out "[lindex $cmd 4] parameters already exists in module \"[lindex $cmd 0]\". "
			append out "Cannot create an override command which has $paramcount parameters."
			return -code error $out
		}
	}

	lappend ifacecmds [list $module $command $proc $accessproc $paramcount]

	return
}

proc unregisterifacecmd {module command} {
	global ifacecmds

	set i [expr [llength $ifacecmds] - 1]
	while {$i >= 0} {
		set cmd [lindex $ifacecmds $i]

		if {[string equal [lindex $cmd 0] $module] && [string equal [lindex $cmd 1] $command]} {
			set ifacecmds [lreplace $ifacecmds $i $i]
		}

		incr i -1
	}

	return
}

proc reflect:cancall {command user} {
	global ifacecmds

	foreach cmd $ifacecmds {
		if {[string equal [lindex $cmd 0] "core"] && [string equal [lindex $cmd 1] $command]} {
			if {[[lindex $cmd 3] $user]} {
				return 1
			} else {
				return 0
			}
		}
	}

	foreach cmd $ifacecmds {
		if {![string equal [lindex $cmd 0] "core"] && [string equal [lindex $cmd 1] $command] && [[lindex $cmd 3] $user]} {
			return 1
		}
	}

	return 0
}

proc reflect:commands {} {
	global ifacecmds

	set commands [list]

	foreach cmd $ifacecmds {
		if {[lsearch -exact $commands [lindex $cmd 1]] == -1} {
			lappend commands [lindex $cmd 1]
		}
	}

	return $commands
}

proc reflect:params {command} {
	global ifacecmds

	foreach cmd $ifacecmds {
		if {[string equal [lindex $cmd 1] $command]} {
			if {[catch [list info args [lindex $cmd 2]] arguments]} {
				set arguments [list]

				set i 0
				while {$i < [lindex $cmd 4]} {
					lappend arguments _$i
					incr i
				}
			}

			return $arguments
		}
	}

	return [list]
}

proc reflect:overrides {command} {
	global ifacecmds

	set results [list]

	foreach cmd $ifacecmds {
		if {[string equal [lindex $cmd 1] $command]} {
			lappend results [list [lindex $cmd 0] [lindex $cmd 3]]
		}
	}

	return $results
}

proc reflect:call2 {cmditem user arguments} {
	set oldcontext [getctx]
	set script [concat [list [lindex $cmditem 2]] $arguments]

	setctx $user
#	puthelp "PRIVMSG #shroudtest2 :[join $cmditem] -  $user - [join $arguments]"
	set error [catch [list eval $script] result]
	setctx $oldcontext

	if {$error} {
		return "RPC_ERROR $result"
	} elseif {$result == ""} {
		return "RPC_NORESULT"
	} else {
		return "RPC_OK $result"
	}
}

proc reflect:call {command user arguments} {
	global ifacecmds

	if {![reflect:cancall $command $user]} {
		return "RPC_UNKNOWN_FUNCTION"
	}

	if {[llength [reflect:params $command]] != [llength $arguments]} {
		return "RPC_PARAMCOUNT"
	}

	foreach cmd $ifacecmds {
		if {![string equal -nocase [lindex $cmd 0] "core"] && [string equal [lindex $cmd 1] $command]} {
			if {[[lindex $cmd 3] $user]} {
				set result [reflect:call2 $cmd $user $arguments]

				if {$result != "RPC_NORESULT"} {
					return $result
				}
			}
		}
	}

	foreach cmd $ifacecmds {
		if {[string equal -nocase [lindex $cmd 0] "core"] && [string equal [lindex $cmd 1] $command]} {
			if {[[lindex $cmd 3] $user]} {
				set result [reflect:call2 $cmd $user $arguments]

				if {$result != "RPC_NORESULT"} {
					return $result
				}
			}
		}
	}

	return "RPC_OK"
}

proc access:admin {user} {
	return [getbncuser $user admin]
}

proc access:anyone {user} {
	return 1
}

proc iface:list {line} {
	return "\n[join $line \n]"
}

proc iface:splitline {line} {
	set toks [list]
	set tok ""
	# 0 = at beginning of new parameter, 1 = reading "enclosed" parameter, 2 reading word
	set state 0

	set i 0
	while {$i < [string length $line]} {
		if {$i > 0} {
			set prev [string index $line [expr $i - 1]]
		} else {
			set prev ""
		}

		if {[expr $i + 1] < [string length $line]} {
			set next [string index $line [expr $i + 1]]
		} else {
			set next ""
		}

		set char [string index $line $i]

		if {$state == 0} {
			if {$char == {"}} {
				set state 1
				incr i
				continue
			}

			set state 2
		}

		if {$state == 1 && $char == {"} && $prev != "\\"} {
			set state 0
			incr i 2

			lappend toks [string map [list "\\\"" "\"" "\6" "\n"] $tok]
			set tok ""

			if {$next != " " && $next != ""} {
				return -code error "Invalid input."
			}

			continue
		}

		if {$state == 2 && $char == " "} {
			set state 0
			incr i

			lappend toks [string map [list "\\\"" "\"" "\6" "\n"] $tok]
			set tok ""

			continue
		}

		append tok $char
		incr i
	}

	if {$state == 1} {
		return -code error "Invalid input."
	}

	if {$tok != ""} {
		lappend toks [string map [list "\\\"" "\"" "\6" "\n"] $tok]
	}

	return $toks
}

proc iface:evalline {line} {
	global ifaceoverride

	if {[catch [list iface:splitline $line] error]} {
		return "RPC_PARSEERROR $error"
	} else {
		set toks $error
	}

	if {[llength $toks] < 3} {
		return "RPC_INVALIDLINE"
	}

	set adm [split [lindex $toks 1] ":"]

	if {[llength $adm] < 2 || [lsearch -exact [bncuserlist] [lindex $adm 0]] == -1} {
		set ifaceoverride 0
	} elseif {![getbncuser [lindex $adm 0] admin]} {
		set ifaceoverride 0
	} elseif {[catch [list getbncuser [lindex $adm 0] server]] || ![bnccheckpassword [lindex $adm 0] [lindex $adm 1]]} {
		set ifaceoverride 0
	} else {
		set ifaceoverride 1
	}

	if {[catch [list getbncuser [lindex $toks 0] server]] || (![bnccheckpassword [lindex $toks 0] [lindex $toks 1]] && !$ifaceoverride)} {
		return "RPC_INVALIDUSERPASS"
	}

	return [string map [list "\n" "\6"] [reflect:call [lindex $toks 2] [lindex $toks 0] [lrange $toks 3 end]]]
}

proc iface:isoverride {} {
	global ifaceoverride

	return $ifaceoverride
}

proc iface:line {idx line} {
	putdcc $idx [iface:evalline $line]
}

# core interface commands

proc iface-reflect:commands {} {
	set commands [reflect:commands]
	set result [list]

	foreach command $commands {
		if {[reflect:cancall $command [getctx]]} {
			lappend result $command
		}
	}

	return [iface:list $result]
}

registerifacecmd "reflect" "commands" "iface-reflect:commands"

proc iface-reflect:params {command} {
	if {[reflect:cancall $command [getctx]]} {
		return [iface:list [reflect:params $command]]
	}

	return -code error "Unknown command."
}

registerifacecmd "reflect" "params" "iface-reflect:params"

proc iface-reflect:overrides {command} {
	if {[reflect:cancall $command [getctx]]} {
		return [iface:list [reflect:overrides $command]]
	}

	return -code error "Unknown command."
}

registerifacecmd "reflect" "overrides" "iface-reflect:overrides"

proc iface-reflect:modules {} {
	global ifacecmds

	set modules [list]

	foreach cmd $ifacecmds {
		if {[lsearch -exact $modules [lindex $cmd 0]] == -1} {
			lappend modules [lindex $cmd 0]
		}
	}

	return [iface:list $modules]
}

registerifacecmd "reflect" "modules" "iface-reflect:modules"
