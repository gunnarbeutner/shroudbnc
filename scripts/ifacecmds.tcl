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

# Interface 2 - built-in commands

# User commands

proc iface:null {} {
	return 1
}

registerifacecmd "core" "null" "iface:null"

proc iface:raw {text} {
	puthelp $text
}

registerifacecmd "core" "raw" "iface:raw"

proc iface:getnick {} {
	return $::botnick
}

registerifacecmd "core" "getnick" "iface:getnick"

proc iface:getvalue {setting} {
	return [getbncuser [getctx] $setting]
}

registerifacecmd "core" "getvalue" "iface:getvalue"

proc iface:gettag {tag} {
	return [getbncuser [getctx] tag $tag]
}

registerifacecmd "core" "gettag" "iface:gettag"

proc iface:getnetwork {} {
	return [getisupport NETWORK]
}

registerifacecmd "core" "getnetwork" "iface:getnetwork"

proc iface:getchannels {} {
	return [iface:list [internalchannels]]
}

registerifacecmd "core" "getchannels" "iface:getchannels"

proc iface:getuptimehr {} {
	return [duration [getbncuser [getctx] uptime]]
}

registerifacecmd "core" "getuptimehr" "iface:getuptimehr"

proc iface:gettraffic {} {
	return [iface:list [list [trafficstats [getctx] server in] [trafficstats [getctx] server out] [trafficstats [getctx] client in] [trafficstats [getctx] client out]]]
}

registerifacecmd "core" "gettraffic" "iface:gettraffic"

proc iface:getchanmodes {channel} {
	return [getchanmode $channel]
}

registerifacecmd "core" "getchanmodes" "iface:getchanmodes"

proc iface:gettopic {channel} {
	return [topic $channel]
}

registerifacecmd "core" "gettopic" "iface:gettopic"

proc iface:getchanlist {channel} {
	return [iface:list [internalchanlist $channel]]
}

registerifacecmd "core" "getchanlist" "iface:getchanlist"

proc iface:jump {} {
	jump
}

registerifacecmd "core" "jump" "iface:jump"

proc iface:setvalue {setting value} {
	set allowedsettings [list server port serverpass realname nick awaynick away channels vhost delayjoin password appendts quitasaway automodes dropmodes]

	if {[lsearch -exact $allowedsettings $setting] == -1} {
		return -code error "You may not modify this setting."
	} else {
		setbncuser [getctx] $setting $value
	}
}

registerifacecmd "core" "setvalue" "iface:setvalue"

proc iface:getlog {} {
	set file [open "users/[getctx].log" r]
	set stuff [read $file]
	close $file

	return [iface:list [split $stuff \n]]
}

registerifacecmd "core" "getlog" "iface:getlog"

proc iface:eraselog {} {
	set file [open users/[getctx].log w+]
	close $file
}

registerifacecmd "core" "eraselog" "iface:eraselog"

proc iface:simul {command} {
	return [iface:list [split [simul [getctx] $command] \n]]
}

registerifacecmd "core" "simul" "iface:simul"

# Admin commands

proc iface:tcl {command} {
	return [eval $command]
}

registerifacecmd "core" "tcl" "iface:tcl" "access:admin"

proc iface:getuserlist {} {
	return [iface:list [bncuserlist]]
}

registerifacecmd "core" "getuserlist" "iface:getuserlist" "access:admin"

proc iface:getmainlog {} {
	set file [open sbnc.log r]
	set stuff [read $file]
	close $file

	return [iface:list [split $stuff \n]]
}

registerifacecmd "core" "getmainlog" "iface:getmainlog" "access:admin"

proc iface:erasemainlog {} {
	set file [open sbnc.log w+]
	close $file
}

registerifacecmd "core" "erasemainlog" "iface:erasemainlog" "access:admin"

proc iface:adduser {username password} {
	addbncuser $username $password
}

registerifacecmd "core" "adduser" "iface:adduser" "access:admin"

proc iface:deluser {username} {
	delbncuser $username
}

registerifacecmd "core" "deluser" "iface:deluser" "access:admin"

proc iface:admin {username} {
	setbncuser $username admin 1
}

registerifacecmd "core" "admin" "iface:admin" "access:admin"

proc iface:unadmin {username} {
	setbncuser $username admin 0
}

registerifacecmd "core" "unadmin" "iface:unadmin" "access:admin"

proc iface:setident {username ident} {
	setbncuser $username ident $ident
}

registerifacecmd "core" "setident" "iface:setident" "access:admin"

proc iface:suspend {username reason} {
	setbncuser $username lock 1
	setbncuser $username suspendreason $reason

	setctx $username
	bncdisconnect "Suspended: $reason"
}

registerifacecmd "core" "suspend" "iface:suspend" "access:admin"

proc iface:unsuspend {username} {
	setbncuser $username lock 0
	setbncuser $username suspendreason ""
}

registerifacecmd "core" "unsuspend" "iface:unsuspend" "access:admin"

proc iface:global {text} {
	foreach user [bncuserlist] {
		setctx $user
		bncnotc $text
	}
}

registerifacecmd "core" "global" "iface:global" "access:admin"

proc iface:killuser {username reason} {
	setctx $username
	bnckill $reason
}

registerifacecmd "core" "killuser" "iface:killuser" "access:admin"

proc iface:disconnectuser {username reason} {
	setctx $username
	bncdisconnect $reason
}

registerifacecmd "core" "disconnectuser" "iface:disconnectuser" "access:admin"

proc iface:rehash {} {
	rehash
}

registerifacecmd "core" "rehash" "iface:rehash" "access:admin"
