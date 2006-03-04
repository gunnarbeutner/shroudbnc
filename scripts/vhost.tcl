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

# format: ip limit hostname
set ::trust {
{192.168.4.1 5 temple.prco23.org}
{192.168.4.2 5 test.prco23.org}
{192.168.4.3 5 test2.prco23.org}
}

set ::defaulthost "192.168.4.1"

internalbind command vhost:command

proc vhost:host2ip {host} {
	global trust

	foreach vh $trust {
		if {[string equal -nocase [lindex $vh 2] $host]} {
			return [lindex $vh 0]
		}
	}

	return $host
}

proc vhost:countvhost {ip} {
	global defaulthost

	set count 0

	set ip [vhost:host2ip $ip]

	if {$ip == ""} { set ip $defaulthost }

	foreach user [bncuserlist] {
		if {[string equal [getbncuser $user vhost] $ip] || ([getbncuser $user vhost] == "" && [string equal $ip $defaulthost])} {
			incr count
		}
	}

	return $count
}

proc vhost:getlimit {ip} {
	global trust defaulthost

	set ip [vhost:host2ip $ip]

	if {$ip == ""} { set ip $defaulthost }

	set res [lsearch -inline $trust "$ip *"]

	if {$res != ""} {
		return [lindex $res 1]
	} else {
		return 0
	}
}

proc vhost:command {client parameters} {
	global trust

	if {![getbncuser $client admin] && [string equal -nocase [lindex $parameters 0] "set"] && [string equal -nocase [lindex $parameters 1] "vhost"]} {
		if {[lsearch -exact [info commands] "lock:islocked"] != -1} {
			if {![string equal [lock:islocked [getctx] "vhost"] "0"]} { return }
		}

		if {![regexp {(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)} [lindex $parameters 2]]} {
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

	if {[string equal -nocase [lindex $parameters 0] "vhosts"]} {
		foreach ip $trust {
			if {[lindex $ip 1] > 0} {
				bncreply "[lindex $ip 0] ([lindex $ip 2]) [vhost:countvhost [lindex $ip 0]]/[vhost:getlimit [lindex $ip 0]] connections"
			}
		}

		bncreply "-- End of VHOSTS."

		haltoutput
	}

	if {[string equal -nocase [lindex $parameters 0] "help"]} {
		bncaddcommand vhosts User "lists all available vhosts" "Syntax: vhosts\nDisplays a list of all available virtual vhosts."
	}
}

proc vhost:findip {} {
	global trust

	set min 9000
	set minip ""

	foreach ip $trust {
		if {[vhost:countvhost [lindex $ip 0]] < $min} {
			set min [vhost:countvhost [lindex $ip 0]]
			set minip [lindex $ip 0]

			if {$min == 0} { break }
		}
	}

	return $minip
}

proc vhost:autosetvhost {user} {
	setbncuser $user vhost [vhost:findip]
}

# iface commands
# +user
# getfreeip
# setvalue vhost
# getvhosts

proc iface-vhost:getfreeip {} {
	return [vhost:findip]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "vhost" "getfreeip" "iface-vhost:freeip"
}

proc iface-vhost:setvalue {setting value} {
	if {[lsearch -exact [info commands] "lock:islocked"] != -1} {
		if {![string equal [lock:islocked $account "vhost"] "0"]} { return }
	}

	if {![getbncuser $account admin] && [string equal -nocase $setting "vhost"]} {
		set limit [vhost:getlimit $value]
		if {$limit == 0} { return -code error "You may not use this virtual host." }

		set count [vhost:countvhost $value]
		if {$count >= $limit} { return -code error "Sorry, the virtual host $ip is already being used by $count users. Please use another virtual host." }

		setbncuser $account vhost $value

		return "1"
	}
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "vhost" "setvalue" "iface-vhost:setvalue"
}

proc iface-vhost:getvhosts {} {
	set vhosts [list]

	foreach host $trust {
		lappend vhosts "[lindex $host 0] [vhost:countvhost [lindex $host 0]] [lindex $host 1] [lindex $host 2]"
	}

	return [iface:list $vhosts]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "vhost" "getvhosts" "iface-vhost:getvhosts"
}

proc iface-vhost:adduser {username password} {
	set result [catch [list addbncuser $username $password]]

	if {!$result} {
		vhost:autosetvhost [lindex $params 0]
		return $result
	} else {
		return -code error $result
	}
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "vhost" "adduser" "iface-vhost:adduser"
}
