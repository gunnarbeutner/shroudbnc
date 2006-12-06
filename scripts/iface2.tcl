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

# Configuration options are availble through iface2 comamnds. The RPC interface
# (Version 2) uses the same port(s) like shroudBNC.

source "scripts/itype.tcl"

internalbind client iface:hijackclient RPC_IFACE
internaltimer 600 1 iface:expireblocks

proc iface:hijackclient {client params} {
	if {$client == "" && [string equal -nocase [lindex $params 0] "RPC_IFACE"]} {
		set idx [hijacksocket]
		control $idx iface:line
	}
}

set ::ifacecmds [list]
set ::ifaceblocks [list]

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
	set error [catch [list eval $script] result]
	setctx $oldcontext

	if {$error} {
		return [itype_exception "RPC_ERROR $result"]
	} elseif {$result == ""} {
		return [itype_string ""]
	} else {
		return $result
	}
}

proc reflect:call {command user arguments} {
	global ifacecmds

	if {![reflect:cancall $command $user]} {
		return [itype_exception "RPC_UNKNOWN_FUNCTION"]
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

proc access:admin {username} {
	return [getbncuser $username admin]
}

proc access:anyone {username} {
	return 1
}

proc iface:evalline {line disconnectVar blockVar} {
	global ifaceoverride

	upvar $disconnectVar disconnect
	upvar $blockVar block

	if {[string equal -nocase $line "RPC_IFACE"]} {
		return "RPC_IFACE_OK"
	}

	set parsedtoks [itype_parse $line]

	set validparameters 0

	if {[lindex $parsedtoks 0] == "list"} {
		set listitems [lindex $parsedtoks 1]

		if {[llength $listitems] >= 4} {
			if {[lindex [lindex $listitems 0] 0] == "string" && [lindex [lindex $listitems 1] 0] == "string"
			  && [lindex [lindex $listitems 2] 0] == "string" && [lindex [lindex $listitems 3] 0] == "list"} {
				set validparameters 1
			}
		}
	}

	if {!$validparameters} {
		set disconnect 1

		return [itype_exception "RPC_INVALIDLINE"]
	}

	set toks [itype_flat $parsedtoks]

	set adm [split [lindex $toks 1] ":"]

	if {[llength $adm] < 2 || ![bncvaliduser [lindex $adm 0]] == -1} {
		set ifaceoverride 0
	} elseif {![getbncuser [lindex $adm 0] admin]} {
		set ifaceoverride 0
	} elseif {[catch [list getbncuser [lindex $adm 0] server]] || ![bnccheckpassword [lindex $adm 0] [lindex $adm 1]]} {
		set ifaceoverride 0
	} else {
		set ifaceoverride 1
	}

	if {[catch [list getbncuser [lindex $toks 0] server]] || (![bnccheckpassword [lindex $toks 0] [lindex $toks 1]] && !$ifaceoverride)} {
		set disconnect 1
		set block 1

		return [itype_exception "RPC_INVALIDUSERPASS"]
	}

	return [reflect:call [lindex $toks 2] [lindex $toks 0] [lindex $toks 3]]
}

proc iface:isoverride {} {
	global ifaceoverride

	return $ifaceoverride
}

proc iface:isipblocked {ip} {
	global ifaceblocks
	set count 0

	foreach block $ifaceblocks {
		if {[string equal -nocase $ip $block]} {
			incr count

			if {$count >= 3} {
				return 1
			}
		}
	}

	return 0
}

proc iface:logbadlogin {ip} {
	global ifaceblocks

	lappend ifaceblocks $ip
}

proc iface:expireblocks {} {
	global ifaceblocks

	set ifaceblocks [list]
}

proc iface:line {idx line} {
	if {$line == ""} {
		return
	}

	set disconnect 0
	set block 0

	set ip [internalgetipforsocket $idx]

	if {[iface:isipblocked $ip] && ![iface:istrustedip $ip]} {
		putdcc $idx [itype_exception "RPC_BLOCK"]
		set disconnect 1
	} else {
		putdcc $idx [iface:evalline $line disconnect block]
	}

	if {$block && ![iface:istrustedip $ip]} {
		iface:logbadlogin $ip
	}

	if {$disconnect} {
		internaltimer 1 0 "killdcc" $idx
	}
}

# core interface commands

proc iface-reflect:commands {} {
	set commands [reflect:commands]
	set result [itype_list_create]

	foreach command $commands {
		if {[reflect:cancall $command [getctx]]} {
			itype_list_insert result [itype_string $command]
		}
	}

	itype_list_end result

	return $result
}

registerifacecmd "reflect" "commands" "iface-reflect:commands"

proc iface-reflect:params {command} {
	if {[reflect:cancall $command [getctx]]} {
		return [itype_list_strings [reflect:params $command]]
	}

	return -code error "Unknown command."
}

registerifacecmd "reflect" "params" "iface-reflect:params"

proc iface-reflect:overrides {command} {
	if {[reflect:cancall $command [getctx]]} {
		set result [itype_list_create]
		set overrides [reflect:overrides $command]

		foreach override $overrides {
			itype_list_insert result [itype_list_strings $override]
		}

		itype_list_end result

		return $result
	}

	return -code error "Unknown command."
}

registerifacecmd "reflect" "overrides" "iface-reflect:overrides"

proc iface-reflect:modules {} {
	global ifacecmds

	set modules [itype_list_create]

	foreach cmd $ifacecmds {
		if {[lsearch -exact $modules [lindex $cmd 0]] == -1} {
			itype_list_insert modules [itype_string [lindex $cmd 0]]
		}
	}

	itype_list_end modules

	return $modules
}

registerifacecmd "reflect" "modules" "iface-reflect:modules"

proc iface-reflect:multicall {calls} {
	set results [itype_list_create]

	set ctx [getctx]

	foreach call $calls {
		setctx $ctx
		itype_list_insert results [reflect:call [lindex $call 0] [getctx] [lindex $call 1]]
	}

	itype_list_end results

	return $results
}

registerifacecmd "reflect" "multicall" "iface-reflect:multicall"

proc iface-trust:addtrustedip {ip} {
	set ips [bncgetglobaltag iface2_trustedips]

	catch [list iface-trust:removetrustedip $ip]

	lappend ips $ip

	bncsetglobaltag iface2_trustedips $ips

	return ""
}

registerifacecmd "trust" "addtrustedip" "iface-trust:addtrustedip" "access:admin"

proc iface-trust:removetrustedip {ip} {
	set ips [bncgetglobaltag iface2_trustedips]

	set index [lsearch -exact [string tolower $ips] $ip]

	if {$index == -1} {
		return -code error "IP address not found."
	}

	set ips [lreplace $ips $index $index]

	bncsetglobaltag iface2_trustedips $ips

	return ""
}

registerifacecmd "trust" "removetrustedip" "iface-trust:removetrustedip" "access:admin"

proc iface-trust:gettrustedips {} {
	return [itype_list_strings [bncgetglobaltag iface2_trustedips]]
}

registerifacecmd "trust" "gettrustedips" "iface-trust:gettrustedips"  "access:admin"

proc iface:istrustedip {ip} {
	set ips [bncgetglobaltag iface2_trustedips]

	set index [lsearch -exact [string tolower $ips] $ip]

	if {$index == -1} {
		return 0
	} else {
		return 1
	}
}

proc iface-trust:istrustedip {ip} {
	return [itype_string [iface:istrustedip $ip]]
}

registerifacecmd "trust" "istrustedip" "iface-trust:istrustedip" "access:admin"
