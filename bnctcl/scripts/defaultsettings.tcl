# shroudBNC - an object-oriented framework for IRC
# Copyright (C) 2010 Christoph Wiese
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

set ::defaultsettings_settings [list default-server default-ssl default-serverpass default-realname default-awaynick default-away default-awaymessage default-usequitasaway default-automodes default-dropmodes]

internalbind usrcreate defaultsettings:applydefaultsettings
internalbind command defaultsettings:command

proc defaultsettings:applydefaultsettings {user} {
	if {![getbncuser $user admin]} {
		foreach defaultsetting $::defaultsettings_settings {
			if {[bncgetglobaltag $defaultsetting] != ""} {
				regsub "default-" $defaultsetting "" setting
				set value [bncgetglobaltag $defaultsetting]

				if {$setting == "server"} {
					set value [split $value " "]
					setbncuser $user server [lindex $value 0]
					setbncuser $user port [lindex $value 1]
				} elseif {($setting == "ssl" || $setting == "usequitasaway") && $value == "on" } {
					setbncuser $user $setting 1
				} elseif {($setting == "ssl" || $setting == "usequitasaway") && $value == "off" } {
					setbncuser $user $setting 0
				} else {
					setbncuser $user $setting $value
				}
			}
		}
	}
}

proc defaultsettings:command {client params} {
	set command [string tolower [lindex $params 0]]

	if {$command == "resetsettings" && [getbncuser $client admin]} {
		if {[llength $params] == "1"} {
			bncreply "Syntax: \"/sbnc resetsettings <target>\", where <target> is either a valid non-admin user or \"-all\" to target all users but the admins."
			haltoutput
			return
		}

		set target [string tolower [lindex $params 1]]

		if {[bncvaliduser $target] && ![getbncuser $target admin]} {
			defaultsettings:applydefaultsettings $target
			bncreply "Done."
		} elseif {$target == "-all"} {
			foreach user [bncuserlist] {
				defaultsettings:applydefaultsettings $user
			}

			bncreply "Done."
		} else {
			bncreply "\"$target\" is not a valid target."
		}

		haltoutput
		return
	}

	if {$command == "help" && [getbncuser $client admin]} {
		bncaddcommand resetsettings Admin "applys defaultsettings to a target" "/sbnc resetsettings <target> \nApplies the default-settings to <target>, which is either a valid non-admin user or \"-all\" to target all users but the admins."
		return
	}

	if {$command == "globalset" && [getbncuser $client admin]} {
		if {[llength $params] < 3} {
			foreach defaultsetting $::defaultsettings_settings {
				if {[bncgetglobaltag $defaultsetting] != ""} {
					set reply [bncgetglobaltag $defaultsetting]
				} else {
					set reply "Not set"
				}

				internaltimer 0 0 bncreply "$defaultsetting - $reply"
			}

			return
		}

		set setting [string tolower [lindex $params 1]]
		set value [join [lrange $params 2 end] " "]

		if {[lsearch -exact $::defaultsettings_settings $setting] > -1} {
			if {![defaultsettings:setdefaults $setting $value]} {
				if {$setting == "default-server"} {
					bncreply "Setting \"default-server\" needs to be in the form of \"<server> <port>\"."
				} else {
					bncreply "Setting \"$setting\" needs to be either \"on\" or \"off\"."
				}
			} else {
				bncreply "Done."
			}

			haltoutput
		}
	}
}

proc defaultsettings:setdefaults {setting value} {
	set setting [string tolower $setting]

	switch $setting {
		default-server {
			if {[regexp {(^$|^.*[[:blank:]][[:digit:]]+$)} $value]} {
				bncsetglobaltag default-server $value
				return "1"
			} else {
				return "0"
			}
		}
		default-ssl {
			if {[regexp -nocase {(^$|^on$|^off$)} $value]} {
				bncsetglobaltag default-ssl [string tolower $value]
				return "1"
			} else {
				return "0"
			}
		}
		default-usequitasaway {
			if {[regexp -nocase {(^$|^on$|^off$)} $value]} {
				bncsetglobaltag default-usequitasaway [string tolower $value]
				return "1"
			} else {
				return "0"
			}
		}
		default {
			bncsetglobaltag $setting $value
			return "1"
		}
	}
}

proc defaultsettings:iface_resetsettings {target} {
	set target [string tolower $target]

	if {[bncvaliduser $target] && ![getbncuser $target admin]} {
		defaultsettings:applydefaultsettings $target
	} elseif {$target == "-all"} {
		foreach user [bncuserlist] {
			defaultsettings:applydefaultsettings $user
		}
	} else {
		return -code error "Invalid target."
	}

	return ""
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "defaultsettings" "resetsettings" "defaultsettings:iface_resetsettings" access:admin
}

proc defaultsettings:iface_setdefaults {setting value} {
	set setting [string tolower $setting]

	if {[lsearch -exact $::defaultsettings_settings $setting] > -1} {
		if {![defaultsettings:setdefaults $setting $value]} {
			if {$setting == "default-server"} {
			return -code error "default-server needs to be in the form of \"<server> <port>\""
			} else {
				return -code error "$setting needs to be either \"on\" or \"off\""
			}
		} else {
			return ""
		}
	} else {
		return -code error "Invalid setting."
	}
}

if {[info commands "registerifacecmd"] != ""} {
	registerifacecmd "defaultsettings" "setdefaults" "defaultsettings:iface_setdefaults" access:admin
}