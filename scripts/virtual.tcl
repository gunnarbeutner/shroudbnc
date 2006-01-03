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

set ::vsbncserver [list irc.quakenet.org 6667]
set ::vsbncdeluser 0

proc virtual:commandiface {client parameters} {
	global vsbncdeluser

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
	if {$vsbncdeluser && [string equal -nocase $command "deluser"]} { virtual:cmd:deluser $client $parameters }
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

			if {$vsbncdeluser} {
				bncaddcommand "deluser" "Admin" "removes a user" "Syntax: deluser <username>\nDeletes a user."
			}

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

	foreach user [bncuserlist] {
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

# 0 - ok
# -1 - human readable error
proc virtual:vresetpass {caller user pass} {
	if {![getbncuser $caller admin] && ![string equal -nocase [virtual:getgroup $caller] [virtual:getgroup $user]]} {
		return [list -1 "There's no such user."]
	}

	setbncuser $user password $pass

	return [list 0]
}

proc virtual:cmd:resetpass {client parameters} {
	set user [lindex $parameters 1]
	set pass [lindex $parameters 2]

	if {$pass == ""} {
		bncnotc "Syntax: resetpass <User> <Password>"
		haltoutput

		return
	}

	set result [virtual:vresetpass $client $user $pass]

	if {[lindex $result 0] == -1} {
		bncreply [lindex $result 1]
		haltoutput

		return
	}

	bncreply "Done."
	haltoutput
}

# 0 - done
# -1 - human readable error
proc virtual:vdeluser {caller user {overridegroups 0}} {
	global vsbncdeluser

	if {[string equal -nocase $user $caller]} {
		return [list -1 "You can't remove your own account."]
	}

	if {![getbncuser $caller admin]} {
		if {![string equal -nocase [virtual:getgroup $user] [virtual:getgroup $caller]]} {
			return [list -1 "There's no such user."]
		}

		if {[getbncuser $user lock]} {
			return [list -1 "You can't remove suspended accounts."]
		}
	}

	if {!$overridegroups && [getbncuser $caller admin]} {
		if {[getbncuser $user tag maxbncs] != ""} {
			return [list -1 "You cannot remove this user. Delete the group instead if you really want to delete this user."]
		}
	}

	if {!$vsbncdeluser && ![getbncuser $caller admin]} {
		return [list -1 "Operation not permitted."]
	}

	delbncuser $user

	return [list 0]
}

proc virtual:cmd:deluser {client parameters} {
	set user [lindex $parameters 1]

	if {$user == ""} {
		bncreply "Syntax: deluser <User>"
		haltoutput
		return
	}

	set result [virtual:vdeluser $client $user]

	if {[lindex $result 0] == -1} {
		bncreply [lindex $result 1]
		haltoutput

		return
	}

	bncreply "Done."
	haltoutput
}

# 0 - ok
# -1 - human-readable error
# -2 - exceeding limit
proc virtual:vadduser {user pass group {checklimits 1} {override 0}} {
	global vsbncserver

	if {![string is alnum $user]} {
		return [list -1 "Sorry, the ident must be alpha-numeric."]
	}

	set error [catch [list getbncuser $user server] result]

	if {!$error} {
		return [list -1 "The ident '$user' is already in use."]
	}

	set nogroup 1
	foreach u [bncuserlist] {
		if {[string equal -nocase $group [virtual:getgroup $u]]} {
			set nogroup 0
			break
		}
	}

	if {$nogroup && !$override} {
		return [list -1 "There is no such group: $group"]
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
			return [join [list -2 [llength $bncs]]]
		}
	}

	addbncuser $user $pass
	setbncuser $user server [lindex $vsbncserver 0]
	setbncuser $user port [lindex $vsbncserver 1]
	virtual:setgroup $user $group

	if {[lsearch -exact [info commands] "vhost:autosetvhost"] != -1} {
		vhost:autosetvhost $user
	}

	return [list 0]
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

	set return [split [virtual:vadduser $user $pass $group $checklimits $override]]

	if {[lindex $return 0] == -1} {
		bncreply [lindex $return 1]
		haltoutput

		return
	}

	if {[lindex $return 0] == -2} {
		bncreply "You already have [lindex $return 1] bouncers. Ask the bouncer admin for an upgrade of your account."
		haltoutput

		return
	}

	bncreply "Done."
	haltoutput
}

# 0 - ok
# -1 - human readable error
proc virtual:vaddgroup {group limit} {
	set pass [randstring 8]

	if {![string is integer $limit]} {
		return [list -1 "Limit should be a number."]
	}

	foreach user [bncuserlist] {
		if {[string equal -nocase $group [virtual:getgroup $user]]} {
			return [list -1 "This groupname is already in use. Try /sbnc who $group to view a list of users."]
		}
	}

	set result [virtual:vadduser $group $pass $group 0 1]

	if {[lindex $result 0] == -1} {
		return $result
	}

	virtual:setadmin $group $group
	setbncuser $group tag maxbncs $limit

	return [list 0 $pass]
}

proc virtual:cmd:addgroup {client parameters} {
	set group [lindex $parameters 1]
	set limit [lindex $parameters 2]

	if {$limit == ""} {
		bncreply "Syntax: addgroup <Group> <Limit>"
		haltoutput
		return
	}

	set result [virtual:vaddgroup $group $limit]

	if {[lindex $result 0] == -1} {
		bncreply [lindex $result 1]
		haltoutput
		return
	}

	bncreply "Created a new group and user '$group' with password '$pass'."
	haltoutput
}

# 0 - ok
proc virtual:vdelgroup {group} {
	foreach user [bncuserlist] {
		if {[string equal -nocase $group [virtual:getgroup $user]] && ![getbncuser $user admin]} {
			delbncuser $user
		}
	}

	return [list 0]
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

# 0 - ok
# -1 - human readable error
proc virtual:vadmin {user} {
	if {[virtual:getgroup $user] == ""} {
		return [list -1 "'$user' is not in a group."]
	}

	virtual:setadmin [virtual:getgroup $user] $user

	return [list 0]
}

proc virtual:cmd:vadmin {client parameters} {
	set user [lindex $parameters 1]

	if {$user == ""} {
		bncreply "Syntax: vadmin <User>"
		haltoutput
		return
	}

	set result [virtual:vadmin $user]

	if {[lindex $result 0] == -1} {
		bncreply [lindex $result 1]
		haltoutput
		return
	}

	bncreply "'$user' is now a virtual admin."
	haltoutput
}

# 0 - ok
# -1 - human readable error
proc virtual:vunadmin {user} {
	if {[virtual:getgroup $user] == ""} {
		return [list -1 "'$user' is not in a group."]
	}

	if {[getbncuser $user tag maxbncs] != ""} {
		return [list -1 "You cannot remove this user's admin privilesges. Delete the group instead.."]
	}

	virtual:setadmin [virtual:getgroup $user] $user 0

	return [list 0]
}

proc virtual:cmd:vunadmin {client parameters} {
	set user [lindex $parameters 1]

	if {$user == ""} {
		bncreply "Syntax: vunadmin <User>"
		haltoutput
		return
	}

	set result [virtual:vunadmin $user]

	if {[lindex $result 0] == -1} {
		bncreply [lindex $result 1]
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

# 0 - ok
# -1 - human readable error
proc virtual:vsetlimit {group limit} {
	if {[virtual:getadmin $group] == ""} {
		return [list -1 "'$group' is not a group."]
	}

	if {![string is integer $limit]} {
		return [list -1 "You did not specify a valid limit."]
	}

	virtual:setmaxbncs $group $limit

	return [list 0]
}

proc virtual:cmd:setlimit {client parameters} {
	set group [lindex $parameters 1]
	set limit [lindex $parameters 2]

	if {$group == ""} {
		bncreply "Syntax: setlimit <Group> <Limit>"
		haltoutput
		return
	}

	set result [virtual:vsetlimit $group $limit]

	if {[lindex $result 0] == -1} {
		bncreply [lindex $result 1]
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
proc virtual:ifacecmd {command params account} {
	set group [virtual:getgroup $account]

	switch -- $command {
		"vgetgroup" {
			if {[getbncuser $account admin] && [lindex $params 0] != ""} {
				set group [virtual:getgroup [lindex $params 0]]
			}

			return $group
		}
		"visadmin" {
			if {[lindex $params 0] != "" && ([getbncuser $account admin] || ([virtual:isadmin $account] && [string equal -nocase $group [virtual:getgroup [lindex $params 0]]]))} {
				set user [lindex $params 0]
			} else {
				set user $account
			}

			return [virtual:isadmin $user]
		}
	}

	if {([virtual:isadmin $account] && $group != "") || [getbncuser $account admin]} {
		switch -- $command {
			"vgetlimit" {
				if {[getbncuser $account admin] && [lindex $params 0] != ""} {
					set group [lindex $params 0]
				}

				return [virtual:getmaxbncs $group]
			}
			"vgroupusers" {
				if {[getbncuser $account admin]} {
					set group [lindex $params 0]
				}

				return [join [virtual:vgroupusers $group]]
			}
			"vadduser" {
				if {[getbncuser $account admin]} {
					set checklimits 0
				} else {
					set checklimits 1
				}

				return [join [virtual:vadduser [lindex $params 0] [lindex $params 1] $group $checklimits]]
			}
			"vdeluser" {
				return [join [virtual:vdeluser $account [lindex $params 0]]]
			}
			"vresetpass" {
				return [join [virtual:vresetpass $account [lindex $params 0] [lindex $params 1]]]
			}
			"vuptimehr" {
				if {[virtual:isadmin $account] && [string equal -nocase [virtual:getgroup [lindex $params 0]] $group]} {
					set user [lindex $params 0]
				} else {
					return
				}

				return [duration [getbncuser $user uptime]]
			}
			"vchannels" {
				if {[virtual:isadmin $account] && [string equal -nocase [virtual:getgroup [lindex $params 0]] $group]} {
					set user [lindex $params 0]
				} else {
					return
				}

				setctx $user
				return [join [internalchannels]]
			}
			"vclient" {
				if {[virtual:isadmin $account] && [string equal -nocase [virtual:getgroup [lindex $params 0]] $group]} {
					set user [lindex $params 0]
				} else {
					return
				}

				return [getbncuser $user client]
			}
			"vsuspended" {
				if {[virtual:isadmin $account] && [string equal -nocase [virtual:getgroup [lindex $params 0]] $group]} {
					set user [lindex $params 0]
				} else {
					return
				}

				return [getbncuser $user lock]
			}
		}
	}

	if {[getbncuser $account admin]} {
		switch -- $command {
			"vsetlimit" {
				return [join [virtual:vsetlimit [lindex $params 0] [lindex $params 1]]]
			}
			"vadmin" {
				return [join [virtual:vadmin [lindex $params 0]]]
			}
			"vunadmin" {
				return [join [virtual:vunadmin [lindex $params 0]]]
			}
			"vgroups" {
				set bncs [list]
				foreach u [bncuserlist] {
					if {[virtual:getgroup $u] != ""} {
						lappend bncs [virtual:getgroup $u]
					}
				}

				return [join [sbnc:uniq $bncs]]
			}
			"vaddgroup" {
				return [join [virtual:vaddgroup [lindex $params 0] [lindex $params 1]]]
			}
			"vdelgroup" {
				return [join [virtual:vdelgroup [lindex $params 0]]]
			}
			"vsetgroup" {
				setbncuser [lindex $params 0] tag group [lindex $params 1]
			}
		}
	}
}

if {[lsearch -exact [info commands] "registerifacehandler"] != -1} {
	registerifacehandler virtual virtual:ifacecmd
}
