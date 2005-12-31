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

# options
set ::ifaceport 8090
set ::ifacessl 0

# iface commands:
# +user:
# null
# raw text
# nick
# value parameter
# tag parameter
# network
# channels
# uptimehr
# traffic
# chanmode channel
# topic channel
# chanlist channel
# usercount channel
# jump
# set option value
# log
# eraselog
# simul text
# +admin
# tcl command
# userlist
# adm user command parameters
# mainlog
# erasemainlog
# adduser user password
# deluser user
# admin user
# unadmin user
# setident user ident
# hasplugin plugin

if {$::ifacessl} {
	catch [list listen $::ifaceport script sbnc:iface "" 1]
} else {
	catch [list listen $::ifaceport script sbnc:iface]
}

set ::ifacehandlers [list]

proc sbnc:iface {socket} {
	control $socket sbnc:ifacemsg
}

proc sbnc:ifacecmd {command params account} {
	global ifacehandlers

	set result ""

	if {[info exists ifacehandlers]} {
		foreach handler $ifacehandlers {
			set tempResult [[lindex $handler 1] $command $params $account]

			if {$tempResult != ""} {
				return $tempResult
			}
		}
	}

	switch -- $command {
		"null" {
			set result 1
		}
		"raw" {
			puthelp [join [lrange $params 0 end]]
		}
		"nick" {
			set result $::botnick
		}
		"value" {
			set result [getbncuser $account [lindex $params 0]]
		}
		"tag" {
			set result [getbncuser $account tag [lindex $params 0]]
		}
		"network" {
			set result [getisupport NETWORK]
		}
		"channels" {
			set result [join [internalchannels]]
		}
		"uptimehr" {
			set result [duration [getbncuser $account uptime]]
		}
		"traffic" {
			set result "[trafficstats $account server in] [trafficstats $account server out] [trafficstats $account client in] [trafficstats $account client out]"
		}

		"chanmode" {
			set result [getchanmode [lindex $params 0]]
		}
		"topic" {
			set result [topic [lindex $params 0]]
		}
		"chanlist" {
			set result [join [internalchanlist [lindex $params 0]]]
		}
		"usercount" {
			set result [llength [internalchanlist [lindex $params 0]]]
		}

		"jump" {
			jump
		}

		"set" {
			if {[lsearch -exact [list server port serverpass realname nick awaynick away channels vhost delayjoin password appendts quitasaway automodes dropmodes] [lindex $params 0]] == -1} {
				set result "denied"
			} else {
				setbncuser $account [lindex $params 0] [join [lrange $params 1 end]]
				set result 1
			}
		}

		"log" {
			set error [catch "open users/$account.log r" file]

			if {$error} {
				set result "Log could not be opened."
			} else {
				set stuff [read $file]
				close $file

				set result [join [split $stuff \n] \005]
			}
		}
		"eraselog" {
			set file [open users/$account.log w+]
			close $file
		}

		"simul" {
			simul $account [join $params]
		}
		"hasplugin" {
			set result 0

			foreach plugin $ifacehandlers {
				if {[string equal -nocase [lindex $plugin 0] [lindex $params 0]]} {
					set result 1
					break
				}
			}
		}
	}

	return $result
}

proc sbnc:ifacemsg {socket line} {
	global ifacehandlers

	set toks [split $line]

	set code [lindex $toks 0]
	set account [lindex $toks 1]
	set pass [lindex $toks 2]
	set command [lindex $toks 3]
	set params [lrange $toks 4 end]

#	setctx fnords
#	puthelp "PRIVMSG #shroudtest2 :debug: $socket $line"

	if {![bnccheckpassword $account $pass]} {
		putdcc $socket "$code 105 Access denied."

		return
	}

	setctx $account

	set result ""

	set result [sbnc:ifacecmd $command $params $account]

	if {[getbncuser $account admin]} {
		switch -- $command {
			"tcl" {
				if {[catch [join $params] result] != 0} {
					set result [lindex [split $result \n] 0]
				}
			}
			"userlist" {
				set result [join [bncuserlist]]
			}
			"adm" {
				setctx [lindex $params 0]
				set account [getctx]
				set result [sbnc:ifacecmd [lindex $params 1] [lrange $params 2 end] [getctx]]
			}
			"mainlog" {
				set error [catch {open sbnc.log r} file]

				if {$error} {
					set result "Log could not be opened."
				} else {
					set stuff [read $file]
					close $file

					set result [join [split $stuff \n] \005]
				}
			}
			"erasemainlog" {
				set file [open sbnc.log w+]
				close $file
			}
			"adduser" {
				set result [catch [list addbncuser [lindex $params 0] [lindex $params 1]]]
			}
			"deluser" {
				set result [catch [list delbncuser [lindex $params 0]]]
			}
			"admin" {
				setbncuser [lindex $params 0] admin 1
			}
			"unadmin" {
				setbncuser [lindex $params 0] admin 0
			}
			"setident" {
				setbncuser [lindex $params 0] ident [lindex $params 1]
			}
			"suspend" {
				setbncuser [lindex $params 0] lock 1
				setbncuser [lindex $params 0] suspendreason [lrange $params 1 end]
			}
			"unsuspend" {
				setbncuser [lindex $params 0] lock 0
				setbncuser [lindex $params 0] suspendreason ""
			}
			"global" {
				foreach user [bncuserlist] {
					bncnotc [join [lrange $params 0 end]]
				}
			}
			"bncuserkill" {
				setctx [lindex $params 0]
				bnckill [join [lrange $params 1 end]]
			}
			"bncuserdisconnect" {
				setctx [lindex $params 0]
				bncdisconnect [join [lrange $params 1 end]]
			}

		}
	}

	putdcc $socket "$code $result"
}

proc registerifacehandler {plugin handler} {
	if {![info exists ::ifacehandlers]} {
		set ::ifacehandlers [list [list $plugin $handler]]
	} else {
		lappend ::ifacehandlers [list $plugin $handler]
	}

	return ""
}

return ""
