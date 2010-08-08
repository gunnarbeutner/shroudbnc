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

# Interface 2 - built-in commands

# User commands

proc iface:null {} {
	return [itype_string 1]
}

registerifacecmd "core" "null" "iface:null"

proc iface:raw {text} {
	puthelp $text

	return ""
}

registerifacecmd "core" "raw" "iface:raw"

proc iface:getisupport {value} {
	return [itype_string [getisupport $value]]
}

registerifacecmd "core" "getisupport" "iface:getisupport"

proc iface:getusercount {channel} {
	return [itype_string [llength [internalchanlist $channel]]]
}

registerifacecmd "core" "getusercount" "iface:getusercount"

proc iface:getchanprefix {channel {nick ""}} {
	if {[string equal $nick ""]} {
		set nick $::botnick
	}
	return [itype_string [getchanprefix $channel $nick]]
}

registerifacecmd "core" "getchanprefix" "iface:getchanprefix"

proc iface:getnick {} {
	return [itype_string $::botnick]
}

registerifacecmd "core" "getnick" "iface:getnick"

proc iface:getvalue {setting} {
	return [itype_string [getbncuser [getctx] $setting]]
}

registerifacecmd "core" "getvalue" "iface:getvalue"

proc iface:gettag {tag} {
	return [itype_string [getbncuser [getctx] tag $tag]]
}

registerifacecmd "core" "gettag" "iface:gettag" "access:admin"

proc iface:settag {tag value} {
	setbncuser [getctx] tag $tag $value
}

registerifacecmd "core" "settag" "iface:settag" "access:admin"

proc iface:getnetwork {} {
	return [itype_string [getisupport NETWORK]]
}

registerifacecmd "core" "getnetwork" "iface:getnetwork"

proc iface:getchannels {} {
	return [itype_list_strings [internalchannels]]
}

registerifacecmd "core" "getchannels" "iface:getchannels"

proc iface:getuptimehr {} {
	return [itype_string [duration [getbncuser [getctx] uptime]]]
}

registerifacecmd "core" "getuptimehr" "iface:getuptimehr"

proc iface:gettraffic {} {
	return [itype_list_strings_args [trafficstats [getctx] server in] [trafficstats [getctx] server out] [trafficstats [getctx] client in] [trafficstats [getctx] client out]]
}

registerifacecmd "core" "gettraffic" "iface:gettraffic"

proc iface:getchanmodes {channel} {
	return [itype_string [getchanmode $channel]]
}

registerifacecmd "core" "getchanmodes" "iface:getchanmodes"

proc iface:gettopic {channel} {
	return [itype_string [topic $channel]]
}

registerifacecmd "core" "gettopic" "iface:gettopic"

proc iface:getchanlist {channel} {
	return [itype_list_strings [internalchanlist $channel]]
}

registerifacecmd "core" "getchanlist" "iface:getchanlist"

proc iface:jump {} {
	jump

	return ""
}

registerifacecmd "core" "jump" "iface:jump"

proc iface:setvalue {setting value} {
	set allowedsettings [list server port serverpass realname nick awaynick away awaymessage channels vhost delayjoin password appendts quitasaway automodes dropmodes ssl ipv6]

	if {[lsearch -exact $allowedsettings $setting] == -1} {
		return -code error "You may not modify this setting."
	} else {
		setbncuser [getctx] $setting $value

		return ""
	}
}

registerifacecmd "core" "setvalue" "iface:setvalue"

proc iface:getlog {from to} {
	set users [bncuserlist]
	set index [lsearch -exact [string tolower $users] [string tolower [getctx]]]

	if {$index == -1} {
		return -code error "Invalid user."
	}

	set username [lindex $users $index]

	set file [open "users/$username.log" r]
	set stuff [read $file]
	close $file

	set stuff [string trim $stuff]
	set lines [split $stuff \n]

	if {$from == -1 && $to == -1} {
		return [itype_list_strings $lines]
	} else {
		return [itype_list_strings [lrange $lines $from $to]]
	}
}

registerifacecmd "core" "getlog" "iface:getlog"

proc iface:getloglines {} {
	set file [open "users/[getctx].log" r]
	set stuff [read $file]
	set stuff [string trim $stuff]
	close $file

	return [itype_string [llength [split $stuff \n]]]
}

registerifacecmd "core" "getloglines" "iface:getloglines"

proc iface:eraselog {} {
	set file [open users/[getctx].log w+]
	close $file

	return ""
}

registerifacecmd "core" "eraselog" "iface:eraselog"

proc iface:simul {command} {
	return [itype_list_strings [split [simul [getctx] $command] \n]]
}

registerifacecmd "core" "simul" "iface:simul"

proc iface:hasmodule {module} {
	if {[lsearch -exact [iface-reflect:modules] $module] != -1} {
		return [itype_string 1]
	} else {
		return [itype_string 0]
	}
}

registerifacecmd "core" "hasmodule" "iface:hasmodule"

proc iface:hascommand {command} {
	if {[lsearch -exact [iface-reflect:commands] $command] != -1} {
		return [itype_string 1]
	} else {
		return [itype_string 0]
	}
}

registerifacecmd "core" "hascommand" "iface:hascommand"

proc iface:setlanguage {language} {
	setbncuser [getctx] tag lang $language

	return ""
}

registerifacecmd "core" "setlanguage" "iface:setlanguage"

proc iface:getversion {} {
	return [itype_string [bncversion]]
}

registerifacecmd "core" "getversion" "iface:getversion"

# Admin commands

proc iface:tcl {command} {
	return [itype_string [eval $command]]
}

registerifacecmd "core" "tcl" "iface:tcl" "access:admin"

proc iface:getuserlist {} {
	return [itype_list_strings [bncuserlist]]
}

registerifacecmd "core" "getuserlist" "iface:getuserlist" "access:admin"

proc iface:getmainlog {from to} {
	set file [open sbnc.log r]
	set stuff [read $file]
	set stuff [string trim $stuff]
	close $file

	set lines [split $stuff \n]

	if {$from == -1 && $to == -1} {
		return [itype_list_strings $lines]
	} else {
		return [itype_list_strings [lrange $lines $from $to]]
	}
}

registerifacecmd "core" "getmainlog" "iface:getmainlog" "access:admin"

proc iface:getmainloglines {} {
	set file [open sbnc.log r]
	set stuff [read $file]
	set stuff [string trim $stuff]
	close $file

	return [itype_string [llength [split $stuff \n]]]
}

registerifacecmd "core" "getmainloglines" "iface:getmainloglines" "access:admin"

proc iface:erasemainlog {} {
	set file [open sbnc.log w+]
	close $file

	return ""
}

registerifacecmd "core" "erasemainlog" "iface:erasemainlog" "access:admin"

proc iface:adduser {username password} {
	addbncuser $username $password

	return ""
}

registerifacecmd "core" "adduser" "iface:adduser" "access:admin"

proc iface:deluser {username} {
	delbncuser $username

	return ""
}

registerifacecmd "core" "deluser" "iface:deluser" "access:admin"

proc iface:admin {username} {
	setbncuser $username admin 1

	return ""
}

registerifacecmd "core" "admin" "iface:admin" "access:admin"

proc iface:unadmin {username} {
	setbncuser $username admin 0

	return ""
}

registerifacecmd "core" "unadmin" "iface:unadmin" "access:admin"

proc iface:setident {username ident} {
	setbncuser $username ident $ident

	return ""
}

registerifacecmd "core" "setident" "iface:setident" "access:admin"

proc iface:suspend {username reason} {
	setbncuser $username lock 1
	setbncuser $username suspendreason $reason

	setctx $username
	bncdisconnect "Suspended: $reason"

	return ""
}

registerifacecmd "core" "suspend" "iface:suspend" "access:admin"

proc iface:unsuspend {username} {
	setbncuser $username lock 0
	setbncuser $username suspendreason ""

	return ""
}

registerifacecmd "core" "unsuspend" "iface:unsuspend" "access:admin"

proc iface:globalmsg {text} {
	foreach user [bncuserlist] {
		setctx $user
		bncnotc $text
	}

	return ""
}

registerifacecmd "core" "globalmsg" "iface:globalmsg" "access:admin"

proc iface:killuser {username reason} {
	setctx $username
	bnckill $reason

	return ""
}

registerifacecmd "core" "killuser" "iface:killuser" "access:admin"

proc iface:disconnectuser {username reason} {
	setctx $username
	bncdisconnect $reason

	return ""
}

registerifacecmd "core" "disconnectuser" "iface:disconnectuser" "access:admin"

proc iface:rehash {} {
	rehash

	return ""
}

registerifacecmd "core" "rehash" "iface:rehash" "access:admin"

proc iface:getglobaltag {tag} {
	return [itype_string [bncgetglobaltag $tag]]
}

registerifacecmd "core" "getglobaltag" "iface:getglobaltag" "access:admin"

proc iface:setglobaltag {tag value} {
	bncsetglobaltag $tag $value

	return ""
}

registerifacecmd "core" "setglobaltag" "iface:setglobaltag" "access:admin"

proc iface:getglobaltags {} {
	return [itype_list_strings [bncgetglobaltags]]
}

registerifacecmd "core" "getglobaltags" "iface:getglobaltags" "access:admin"

proc iface:gethosts {} {
	return [itype_list_strings [getbnchosts]]
}

registerifacecmd "core" "gethosts" "iface:gethosts" "access:admin"

proc iface:addhost {host} {
	addbnchost $host

	return ""
}

registerifacecmd "core" "addhost" "iface:addhost" "access:admin"

proc iface:delhost {host} {
	delbnchost $host

	return ""
}

registerifacecmd "core" "delhost" "iface:delhost" "access:admin"

proc iface:sendmessage {user message} {
	setctx $user

	if {[getbncuser $user hasclient]} {
		bncnotc $message
	} else {
		putlog $message
	}

	return ""
}

registerifacecmd "core" "sendmessage" "iface:sendmessage" "access:admin"
