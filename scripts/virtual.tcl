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

internalbind command virtual:commandiface

set ::vsbncdeluser 0

proc virtual:commandiface {client parameters} {
	if {![virtual:isadmin $client] && ![getbncuser $client admin]} { return }

	set command [lindex $parameters 0]

	if {[getbncuser $client admin]} {
		if {[string equal -nocase $command "addgroup"]} { virtual:cmd:addgroup $client $parameters }
		if {[string equal -nocase $command "delgroup"]} { virtual:cmd:delgroup $client $parameters }
		if {[string equal -nocase $command "vadmin"]} { virtual:cmd:vadmin $client $parameters }
		if {[string equal -nocase $command "vunadmin"]} { virtual:cmd:vunadmin $client $parameters }
		if {[string equal -nocase $command "getlimit"]} { virtual:cmd:getlimit $client $parameters }
		if {[string equal -nocase $command "setlimit"]} { virtual:cmd:setlimit $client $parameters }
		if {[string equal -nocase $command "groups"]} { virtual:cmd:groups $client $parameters }
	}

	if {[string equal -nocase $command "who"]} { virtual:cmd:who $client $parameters }
	if {[string equal -nocase $command "resetpass"]} { virtual:cmd:resetpass $client $parameters }
	if {[string equal -nocase $command "deluser"]} { virtual:cmd:deluser $client $parameters }
	if {[string equal -nocase $command "adduser"]} { virtual:cmd:adduser $client $parameters }

	if {[string equal -nocase $command "help"]} {
		if {[getbncuser $client admin]} {
			bncaddcommand "addgroup" "VAdmin" "creates a new group"
			bncaddcommand "delgroup" "VAdmin" "deletes a group and all its users"
			bncaddcommand "vadmin" "VAdmin" "gives a user virtual admin privileges"
			bncaddcommand "vunadmin" "VAdmin" "removes a user's virtual admin privileges"
			bncaddcommand "getlimit" "VAdmin" "gets a group's limit"
			bncaddcommand "setlimit" "VAdmin" "sets a group's limit"
			bncaddcommand "groups" "VAdmin" "lists all groups"

			set help "Syntax: who \[group\]\n"
			append help "Shows a list of all users (in a specific group).\n"
			append help "Flags (which are displayed in front of the username):\n"
			append help "@ user is an admin\n"
			append help "* user is currently logged in\n"
			append help "! user is suspended\n"
			append help "\$ user is a virtual admin"
			bncaddcommand "who" "Admin" "shows users" $help
		} else {
			bncaddcommand "adduser" "Admin" "creates a new user" "Syntax: adduser <username> <password>\nCreates a new user."
			bncaddcommand "deluser" "Admin" "removes a user" "Syntax: deluser <username>\nDeletes a user."

			bncaddcommand "resetpass" "Admin" "sets a user's password" "Syntax: resetpass <user> <password>\nResets another user's password."

			set help "Syntax: who\n"
			append help "Shows a list of all users.\n"
			append help "Flags (which are displayed in front of the username):\n"
			append help "@ user is an admin\n"
			append help "\$ user is a virtual admin"
			append help "* user is currently logged in\n"
			append help "! user is suspended\n"
			bncaddcommand "who" "Admin" "shows users" $help
		}
	}
}

proc virtual:isadmin {account} {
	if {[getbncuser $account tag reseller] == "1"} {
		return 1
	} else {
		return 0
	}
}

proc virtual:setadmin {group account {value 1}} {
	setbncuser $account tag group $group
	setbncuser $account tag reseller $value
}

proc virtual:getadmin {group} {
	foreach user [bncuserlist] {
		if {[virtual:isadmin $user] && [string equal -nocase [virtual:getgroup $user] $group] && [getbncuser $user tag maxbncs] != ""} {
			return $user
		}
	}

	return ""
}

proc virtual:setgroup {account group} {
	setbncuser $account tag group $group
}

proc virtual:getgroup {account} {
	variable result

	set error [catch [list getbncuser $account tag group] result]

	if {$error} { return "" }

	return $result
}

proc virtual:setmaxbncs {group maxbncs} {
	set account [virtual:getadmin $group]

	if {$account == ""} { return 0 }

	setbncuser $account tag maxbncs $maxbncs

	return 1
}

proc virtual:getmaxbncs {group} {
	set account [virtual:getadmin $group]

	if {$account == ""} { return 0 }

	return [getbncuser $account tag maxbncs]
}

proc virtual:cmd:who {account parameters} {
	if {[getbncuser $account admin]} {
		set group [lindex $parameters 1]
	} else {
		set group [virtual:getgroup $account]
	}

	foreach user [lsort [bncuserlist]] {
		if {[string equal -nocase [virtual:getgroup $user] $group] || $group == ""} {
			set out ""

			setctx $user

			if {[getbncuser $user admin]} {
				append out "@"
			}

			if {[virtual:isadmin $user]} {
				if {[getbncuser $account admin]} {
					append out "\$"
				} else {
					append out "@"
				}
			}

			if {[getbncuser $user hasclient]} {
				append out "*"
			}

			if {[getbncuser $user lock]} {
				append out "!"
			}

			append out $user
			append out "([getcurrentnick])@[getbncuser $user client] \[[getbncuser $user realserver]\]"

			append out " \[Last seen: "

			if {[getbncuser $user hasclient]} {
				append out "Now"
			} elseif {[getbncuser $user seen] == 0} {
				append out "Never"
			} else {
				append out [strftime "%c" [getbncuser $user seen]]
			}

			append out "\]"

			if {[getbncuser $account admin] && $group == ""} {
				set grp [virtual:getgroup $user]
				if {$grp == ""} { set grp "<none>" }
				append out " $grp"
			}

			append out " :[getbncuser $user realname]"

			setctx $account
			bncnotc $out
		}
	}

	bncnotc "End of USERS."

	haltoutput
}

proc virtual:vresetpass {caller user pass} {
	if {![getbncuser $caller admin] && ![string equal -nocase [virtual:getgroup $caller] [virtual:getgroup $user]]} {
		return -code error "There's no such user."
	}

	setbncuser $user password $pass

	return 0
}

proc virtual:cmd:resetpass {client parameters} {
	set user [lindex $parameters 1]
	set pass [lindex $parameters 2]

	if {$pass == ""} {
		bncnotc "Syntax: resetpass <User> <Password>"
		haltoutput

		return
	}

	if {[catch [list virtual:vresetpass $client $user $pass] error]} {
		bncreply $error
		haltoutput

		return
	}

	bncreply "Done."
	haltoutput
}

proc virtual:vdeluser {caller user} {
	global vsbncdeluser

	if {[string equal -nocase $user $caller]} {
		return -code error "You can't remove your own account."
	}

	if {![getbncuser $caller admin]} {
		if {![string equal -nocase [virtual:getgroup $user] [virtual:getgroup $caller]]} {
			return -code error "There's no such user."
		}

		if {[getbncuser $user lock]} {
			return -code error "You can't remove suspended accounts."
		}
	}

	if {[getbncuser $caller admin]} {
		if {[getbncuser $user tag maxbncs] != ""} {
			return -code error "You cannot remove this user. Delete the group instead if you really want to delete this user."
		}
	}

	if {!$vsbncdeluser && ![getbncuser $caller admin]} {
		return -code error "Operation not permitted."
	}

	delbncuser $user

	return 0
}

proc virtual:cmd:deluser {client parameters} {
	set user [lindex $parameters 1]

	if {$user == ""} {
		bncreply "Syntax: deluser <User>"
		haltoutput
		return
	}

	if {[catch [list virtual:vdeluser $client $user] error]} {
		bncreply $error
		haltoutput

		return
	}

	bncreply "Done."
	haltoutput
}

proc virtual:vadduser {user pass group {checklimits 1} {override 0}} {
	set error [catch [list getbncuser $user server] result]

	if {!$error} {
		return -code error "The ident '$user' is already in use."
	}

	set nogroup 1
	foreach u [bncuserlist] {
		if {[string equal -nocase $group [virtual:getgroup $u]]} {
			set nogroup 0
			break
		}
	}

	if {$nogroup && !$override} {
		return -code error "There is no such group: $group"
	}

	if {$checklimits} {
		set maxbncs [virtual:getmaxbncs $group]

		set bncs [list]
		foreach u [bncuserlist] {
			if {[string equal -nocase $group [getbncuser $u tag group]]} {
				lappend bncs $u
			}
		}

		if {[llength $bncs] >= $maxbncs} {
			return -code error "You cannot add more than $maxbncs users."
		}
	}

	addbncuser $user $pass
	virtual:setgroup $user $group

	return 0
}

proc virtual:cmd:adduser {client parameters} {
	set user [lindex $parameters 1]
	set pass [lindex $parameters 2]

	if {[getbncuser $client admin]} {
		set group [lindex $parameters 3]
		set override [lindex $parameters 4]
	} else {
		set group [virtual:getgroup $client]
		set override 0
	}

	if {$pass == ""} {
		if {[getbncuser $client admin]} {
			bncreply "Syntax: adduser <User> <Password> \[<Group>\]"
		} else {
			bncreply "Syntax: adduser <User> <Password>"
		}

		haltoutput
		return
	}

	if {[getbncuser $client admin]} {
		set checklimits 0
	} else {
		set checklimits 1
	}

	if {[catch [list virtual:vadduser $user $pass $group $checklimits $override] error]} {
		bncreply $error
		haltoutput

		return
	}

	bncreply "Done."
	haltoutput
}

proc virtual:vaddgroup {group limit} {
	set pass [randstring 8]

	if {$limit == "" || ![string is integer $limit]} {
		return -code error "Limit should be a number."
	}

	foreach user [bncuserlist] {
		if {[string equal -nocase $group [virtual:getgroup $user]]} {
			return -code error "This groupname is already in use. Try /sbnc who $group to view a list of users."
		}
	}

	virtual:vadduser $group $pass $group 0 1
	virtual:setadmin $group $group
	setbncuser $group tag maxbncs $limit

	return $pass
}

proc virtual:cmd:addgroup {client parameters} {
	set group [lindex $parameters 1]
	set limit [lindex $parameters 2]

	if {$limit == ""} {
		bncreply "Syntax: addgroup <Group> <Limit>"
		haltoutput
		return
	}

	if {[catch [list virtual:vaddgroup $group $limit] error]} {
		bncreply $error
		haltoutput
		return
	} else {
		set pass $error
	}

	bncreply "Created a new group and user '$group' with password '$pass'."
	haltoutput
}

proc virtual:vdelgroup {group} {
	foreach user [bncuserlist] {
		if {[string equal -nocase $group [virtual:getgroup $user]] && ![getbncuser $user admin]} {
			delbncuser $user
		}
	}

	return 0
}

proc virtual:cmd:delgroup {client parameters} {
	set group [lindex $parameters 1]

	if {$group == ""} {
		bncreply "Syntax: delgroup <Group>"
		haltoutput
		return
	}

	virtual:vdelgroup $group

	bncreply "Deleted all users in group '$group' (except real admins)."
	haltoutput
}

proc virtual:vadmin {user} {
	if {[virtual:getgroup $user] == ""} {
		return -code error "'$user' is not in a group."
	}

	virtual:setadmin [virtual:getgroup $user] $user

	return 0
}

proc virtual:cmd:vadmin {client parameters} {
	set user [lindex $parameters 1]

	if {$user == ""} {
		bncreply "Syntax: vadmin <User>"
		haltoutput
		return
	}

	if {[catch [list virtual:vadmin $user] error]} {
		bncreply $error
		haltoutput
		return
	}

	bncreply "'$user' is now a virtual admin."
	haltoutput
}

proc virtual:vunadmin {user} {
	if {[virtual:getgroup $user] == ""} {
		return -code error "'$user' is not in a group."
	}

	if {[getbncuser $user tag maxbncs] != ""} {
		return -code error "You cannot remove this user's admin privilesges. Delete the group instead."
	}

	virtual:setadmin [virtual:getgroup $user] $user 0

	return 0
}

proc virtual:cmd:vunadmin {client parameters} {
	set user [lindex $parameters 1]

	if {$user == ""} {
		bncreply "Syntax: vunadmin <User>"
		haltoutput
		return
	}

	if {[catch [list virtual:vunadmin $user] error]} {
		bncreply $error
		haltoutput
		return
	}

	bncreply "'$user' is no longer a virtual admin."
	haltoutput
}

proc virtual:cmd:getlimit {client parameters} {
	set group [lindex $parameters 1]

	if {$group == ""} {
		bncreply "Syntax: getlimit <Group>"
		haltoutput
		return
	}

	if {[virtual:getadmin $group] == ""} {
		bncreply "'$group' is not a group."
		haltoutput
		return
	}

	bncreply "$group is limited to [virtual:getmaxbncs $group] users."
	haltoutput
}

proc virtual:vsetlimit {group limit} {
	if {[virtual:getadmin $group] == ""} {
		return -code error "'$group' is not a group."
	}

	if {![string is integer $limit]} {
		return -code error "You did not specify a valid limit."
	}

	virtual:setmaxbncs $group $limit

	return 0
}

proc virtual:cmd:setlimit {client parameters} {
	set group [lindex $parameters 1]
	set limit [lindex $parameters 2]

	if {$group == ""} {
		bncreply "Syntax: setlimit <Group> <Limit>"
		haltoutput
		return
	}

	if {[catch [list virtual:vsetlimit $group $limit] error]} {
		bncreply $error
		haltoutput
		return
	}

	bncreply "$group is now limited to $limit users."
	haltoutput
}

proc virtual:vgroupusers {group} {
	set bncs [list]
	foreach u [bncuserlist] {
		if {[string equal -nocase $group [getbncuser $u tag group]]} {
			lappend bncs $u
		}
	}

	return $bncs
}

proc virtual:cmd:groups {client parameters} {
	set bncs [list]
	foreach u [bncuserlist] {
		if {[virtual:getgroup $u] != ""} {
			lappend bncs [virtual:getgroup $u]
		}
	}

	set groups [sbnc:uniq $bncs]

	bncreply "Groups:"

	foreach group $groups {
		set bncs [list]
		foreach u [bncuserlist] {
			if {[string equal -nocase $group [getbncuser $u tag group]]} {
				lappend bncs $u
			}
		}

		bncreply "$group [llength $bncs]/[virtual:getmaxbncs $group]"
	}

	bncreply "End of GROUPS."
	haltoutput 
}

# iface commands
# +user:
# vgetgroup
# visadmin
# +vadmin:
# vgetlimit
# vgroupusers
# vadduser user pass
# vdeluser user
# vresetpass user pass
# vuptimehr [user]
# vchannels [user]
# vsuspended [user]
# vclient [user]
# +admin:
# vgetlimit [group]
# vsetlimit [group] [limit]
# vgetgroup [user]
# visadmin [user]
# vgroups
# vgroupusers [group]
# vadmin [user]
# vunadmin [user]
# vaddgroup [group] [limit]
# vdelgroup [group]
# vsetgroup user group

proc access:vadmin {user} {
	if {[access:admin $user]} { return 1}
	if {[virtual:getgroup $user] == ""} { return 0 }
	if {![virtual:isadmin $user]} { return 0 }

	return 1
}

proc virtual:canmodify {user} {
	if {[string equal -nocase [virtual:getgroup $user] [virtual:getgroup [getctx]]] || [getbncuser [getctx] admin]} {
		return 1
	} else {
		return -code error "You cannot modify this user."
	}
}

proc virtual:canmodifygroup {group} {
	if {[string equal -nocase $group [virtual:getgroup [getctx]]] || [getbncuser [getctx] admin]} {
		return 1
	} else {
		return -code error "You cannot modify this user."
	}
}

# Iface2 commands

proc iface-virtual:getgroup {username} {
	virtual:canmodify $username

	return [virtual:getgroup $username]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "getgroup" "iface-virtual:getgroup"
}

proc iface-virtual:isvadmin {username} {
	virtual:canmodify $username

	return [access:vadmin $username]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "isvadmin" "iface-virtual:isvadmin"
}

proc iface-virtual:getgrouplimit {group} {
	virtual:canmodifygroup $group

	return [virtual:getmaxbncs $group]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "getgrouplimit" "iface-virtual:getgrouplimit" "access:vadmin"
}

proc iface-virtual:getgroupusers {group} {
	virtual:canmodifygroup $group

	return [iface:list [virtual:vgroupusers $group]]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "getgroupusers" "iface-virtual:getgroupusers" "access:vadmin"
}

proc iface-virtual:adduser {username password} {
	if {[getbncuser [getctx] admin]} {
		set checklimits 0
	} else {
		set checklimits 1
	}

	return [virtual:vadduser $username $password [virtual:getgroup [getctx]] $checklimits]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "adduser" "iface-virtual:adduser" "access:vadmin"
}

proc iface-virtual:deluser {username} {
	virtual:canmodify $username

	return [virtual:vdeluser [getctx] $username]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "deluser" "iface-virtual:deluser" "access:vadmin"
}

proc iface-virtual:vresetpass {username password} {
	virtual:canmodify $username

	setbncuser $username password $password
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "vresetpass" "iface-virtual:vresetpass" "access:vadmin"
}

proc iface-virtual:vgetuptimehr {username} {
	virtual:canmodify $username

	return [duration [getbncuser $username uptime]]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "vgetuptimehr" "iface-virtual:vgetuptimehr" "access:vadmin"
}

proc iface-virtual:vgetchannels {username} {
	virtual:canmodify $username

	setctx $username
	return [iface:list [internalchannels]]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "vgetchannels" "iface-virtual:vgetchannels" "access:vadmin"
}

proc iface-virtual:vissuspended {username} {
	virtual:canmodify $username

	return [getbncuser $username lock]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "vissuspended" "iface-virtual:vissuspended" "access:vadmin"
}

proc iface-virtual:vgetclient {username} {
	virtual:canmodify $username

	setctx $username
	return [getbncuser $username client]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "vgetclient" "iface-virtual:vgetclient" "access:vadmin"
}

proc iface-virtual:setgrouplimit {group limit} {
	return [virtual:vsetlimit $group $limit]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "setgrouplimit" "iface-virtual:setgrouplimit" "access:admin"
}

proc iface-virtual:getgroups {} {
	set groups [list]

	foreach user [bncuserlist] {
		if {[virtual:getgroup $user] != ""} {
			lappend groups [virtual:getgroup $user]
		}
	}

	return [iface:list $groups]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "getgroups" "iface-virtual:getgroups" "access:admin"
}

proc iface-virtual:getgroup {username} {
	return [virtual:getgroup $username]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "getgroup" "iface-virtual:getgroup" "access:admin"
}

proc iface-virtual:setgroup {username group} {
	virtual:setgroup $username $group
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "setgroup" "iface-virtual:setgroup" "access:admin"
}

proc iface-virtual:vadmin {username} {
	return [virtual:vadmin $username]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "vadmin" "iface-virtual:vadmin" "access:admin"
}

proc iface-virtual:vunadmin {username} {
	return [virtual:vunadmin $username]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "vunadmin" "iface-virtual:vunadmin" "access:admin"
}

proc iface-virtual:addgroup {group limit} {
	return [virtual:vaddgroup $group $limit]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "addgroup" "iface-virtual:addgroup" "access:admin"
}

proc iface-virtual:delgroup {group} {
	return [virtual:vdelgroup $group]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "delgroup" "iface-virtual:delgroup" "access:admin"
}

proc iface-virtual:getgroupadmin {group} {
	return [virtual:getadmin $group]
}

if {[lsearch -exact [info commands] "registerifacecmd"] != -1} {
	registerifacecmd "virtual" "getgroupadmin" "iface-virtual:getgroupadmin" "access:admin"
}
