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

internalbind command vhost:command
internalbind usrcreate vhost:newuser

proc vhost:host2ip {host} {
	set vhosts [bncgetglobaltag vhosts]

	if {[lsearch -exact [info procs] "vhost_hack:getadditionalvhosts"] != -1} {
		set vhosts [concat $vhosts [vhost_hack:getadditionalvhosts]]
	}

	foreach vhost $vhosts {
		if {[string equal -nocase [lindex $vhost 2] $host]} {
			return [lindex $vhost 0]
		}
	}

	return $host
}

proc vhost:countvhost {ip} {
	set count 0

	set ip [vhost:host2ip $ip]

	if {$ip == ""} { set ip [vhost:getdefaultip] }

	foreach user [bncuserlist] {
		if {[string equal [getbncuser $user vhost] $ip] || ([getbncuser $user vhost] == "" && [string equal $ip [vhost:getdefaultip]])} {
			incr count
		}
	}

	return $count
}

proc vhost:getlimit {ip} {
	set vhosts [bncgetglobaltag vhosts]
	set ip [vhost:host2ip $ip]

	if {[lsearch -exact [info procs] "vhost_hack:getadditionalvhosts"] != -1} {
		set vhosts [concat $vhosts [vhost_hack:getadditionalvhosts]]
	}

	if {$ip == ""} { set ip [vhost:getdefaultip] }

	set res [lsearch -inline $vhosts "$ip *"]

	if {$res != ""} {
		return [lindex $res 1]
	} else {
		return 0
	}
}

proc vhost:getdefaultip {} {
	foreach user [bncuserlist] {
		if {[getbncuser $user vhost] == ""} {
			set localip [getbncuser $user localip]

			if {$localip != ""} { return $localip }
		}
	}

	return ""
}

proc vhost:isip {ip} {
	return [regexp {(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)} $ip]
}

proc vhost:command {client parameters} {
	if {![getbncuser $client admin] && [string equal -nocase [lindex $parameters 0] "set"] && [string equal -nocase [lindex $parameters 1] "vhost"]} {
		if {[lsearch -exact [info commands] "lock:islocked"] != -1} {
			if {![string equal [lock:islocked [getctx] "vhost"] "0"]} { return }
		}

		if {![vhost:isip [lindex $parameters 2]]} {
			bncreply "You have to specify a valid IP address."

			haltoutput
			return
		}

		set limit [vhost:getlimit [lindex $parameters 2]]

		if {$limit == 0} {
			bncreply "Sorry, you may not use this IP address/hostname."

			haltoutput
			return
		} elseif {[vhost:countvhost [lindex $parameters 2]] >= $limit} {
			bncreply "Sorry, the IP address [lindex $parameters 2] is already being used by [vhost:countvhost [lindex $parameters 2]] users. A maximum number of [vhost:getlimit [lindex $parameters 2]] users may use this IP address."

			haltoutput
			return
		}
	}

	set vhosts [bncgetglobaltag vhosts]

	if {[lsearch -exact [info procs] "vhost_hack:getadditionalvhosts"] != -1} {
		set vhosts [concat $vhosts [vhost_hack:getadditionalvhosts]]
	}

	if {[string equal -nocase [lindex $parameters 0] "vhosts"]} {
		foreach vhost $vhosts {
			if {[lindex $vhost 1] > 0} {
				if {[getbncuser $client admin]} {
					bncreply "[lindex $vhost 0] ([lindex $vhost 2]) [vhost:countvhost [lindex $vhost 0]]/[vhost:getlimit [lindex $vhost 0]] connections"
				} else {
					if {[vhost:countvhost [lindex $vhost 0]] >= [vhost:getlimit [lindex $vhost 0]]} {
						set status "full"
					} else {
						set status "not full"
					}

					bncreply "[lindex $vhost 0] ([lindex $vhost 2]) $status"
				}
			}
		}

		bncreply "-- End of VHOSTS."

		haltoutput
	}

	if {[getbncuser [getctx] admin] && [string equal -nocase [lindex $parameters 0] "addvhost"]} {
		set ip [lindex $parameters 1]
		set limit [lindex $parameters 2]
		set host [lindex $parameters 3]

		if {$host == ""} {
			bncreply "Syntax: ADDVHOST <ip> <limit> <host>"

			haltoutput
			return
		}

		if {![vhost:isip $ip]} {
			bncreply "You did not specify a valid IP address."

			haltoutput
			return
		}

		if {![string is integer $limit]} {
			bncreply "You did not specify a valid limit."

			haltoutput
			return
		}

		if {[catch [list vhost:addvhost $ip $limit $host] error]} {
			bncreply $error
		} else {
			bncreply "Done."
		}

		haltoutput
	}

	if {[getbncuser [getctx] admin] && [string equal -nocase [lindex $parameters 0] "delvhost"]} {
		set ip [vhost:host2ip [lindex $parameters 1]]

		if {$ip == ""} {
			bncreply "Syntax: DELVHOST <ip>"

			haltoutput
			return
		}

		if {[catch [list vhost:delvhost $ip] error]} {
			bncreply $error
		} else {
			bncreply "Done."
		}

		haltoutput
	}

	if {[string equal -nocase [lindex $parameters 0] "help"]} {
		bncaddcommand vhosts User "lists all available vhosts" "Syntax: vhosts\nDisplays a list of all available virtual vhosts."

		if {[getbncuser [getctx] admin]} {
			bncaddcommand addvhost Admin "adds a new vhost" "Syntax: addvhost ip limit host\nAdds a new vhost."
			bncaddcommand delvhost Admin "removes a vhost" "Syntax: delvhost ip\nRemoves a vhost."
		}
	}
}

proc vhost:findip {} {
	set vhosts [bncgetglobaltag vhosts]

	if {[lsearch -exact [info procs] "vhost_hack:getadditionalvhosts"] != -1} {
		set vhosts [concat $vhosts [vhost_hack:getadditionalvhosts]]
	}

	set min 9000
	set minip ""

	foreach vhost $vhosts {
		if {[vhost:countvhost [lindex $vhost 0]] < $min} {
			set min [vhost:countvhost [lindex $vhost 0]]
			set minip [lindex $vhost 0]

			if {$min == 0} { break }
		}
	}

	return $minip
}

proc vhost:newuser {user} {
	setbncuser $user vhost [vhost:findip]
}

proc vhost:addvhost {ip limit host} {
	if {[vhost:getlimit $ip] != 0 || [vhost:getlimit $host] != 0} {
		return -code error "This vhost has already been added."
	} else {
		if {[string length $limit] == 0 || ![string is integer $limit]} {
			return -code error "You need to specify a valid limit."
		}

		if {[string length $ip] == 0} {
			return -code error "You need to specify a valid IP address."
		}

		if {[string length $host] == 0} {
			return -code error "You need to specify a valid hostname."
		}

		set vhosts [bncgetglobaltag vhosts]
		lappend vhosts [list $ip $limit $host]
		bncsetglobaltag vhosts $vhosts
	}
}

proc vhost:delvhost {ip} {
	set vhosts [bncgetglobaltag vhosts]
	set ip [vhost:host2ip $ip]
	set i 0
	set found 0

	while {$i < [llength $vhosts]} {
		set vhost [lindex $vhosts $i]

		if {[string equal -nocase $ip [lindex $vhost 0]]} {
			set vhosts [lreplace $vhosts $i $i]
			set found 1
			break
		}

		incr i
	}

	if {$found} {
		bncsetglobaltag vhosts $vhosts
	} else {
		return -code error "There is no such vhost."
	}
}

# iface commands
# +user
# getfreeip
# setvalue vhost
# getvhosts
# +admin
# addvhost ip limit host
# delvhost ip

proc iface-vhost:getfreeip {} {
	return [itype_string [vhost:findip]]
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "vhost" "getfreeip" "iface-vhost:getfreeip"
}

proc iface-vhost:setvalue {setting value} {
	if {[iface:isoverride]} { return "" }

	if {[lsearch -exact [info commands] "lock:islocked"] != -1} {
		if {![string equal [lock:islocked [getctx] "vhost"] "0"]} { return "" }
	}

	if {![getbncuser [getctx] admin] && [string equal -nocase $setting "vhost"]} {
		set limit [vhost:getlimit $value]
		if {$limit == 0} { return -code error "You may not use this virtual host." }

		set count [vhost:countvhost $value]
		if {$count >= $limit} { return -code error "Sorry, the virtual host $ip is already being used by $count users. Please use another virtual host." }

		setbncuser [getctx] vhost $value
	}

	return ""
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "vhost" "setvalue" "iface-vhost:setvalue"
}

proc iface-vhost:getvhosts {} {
	set result [itype_list_create]

	if {[lsearch -exact [info procs] "vhost_hack:getadditionalvhosts"] != -1} {
		set vhosts [concat $vhosts [vhost_hack:getadditionalvhosts]]

		foreach vhost $vhosts {
			set vhost_itype [itype_list_strings $vhost]
			itype_list_insert result $vhost_itype
		}
	}

	set vhosts [bncgetglobaltag vhosts]

	foreach vhost $vhosts {
		set vhost_itype [itype_list_strings_args [lindex $vhost 0] [vhost:countvhost [lindex $vhost 0]] [lindex $vhost 1] [lindex $vhost 2]]
		itype_list_insert result $vhost_itype
	}

	itype_list_end $result

	return $result
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "vhost" "getvhosts" "iface-vhost:getvhosts"
}

proc iface-vhost:addvhost {ip limit host} {
	vhost:addvhost $ip $limit $host

	return ""
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "vhost" "addvhost" "iface-vhost:addvhost" "access:admin"
}

proc iface-vhost:delvhost {ip} {
	vhost:delvhost $ip

	return ""
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "vhost" "delvhost" "iface-vhost:delvhost" "access:admin"
}
