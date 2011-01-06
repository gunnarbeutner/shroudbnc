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

package require http
package require tls

::http::register https 443 ::tls::socket

internalbind command push:command
internalbind server push:servermsg
internalbind client push:clientmsg

proc push:displaysettings {client} {
	setctx $client
	set value [getbncuser $client tag prowl_apikey]
	if {$value == ""} { set value "Not set" }
	bncreply "prowl_apikey - $value"
	if {$value != ""} {
		set value [getbncuser $client tag prowl_highlight]
		if {$value == ""} { set value "Not set" }
		bncreply "prowl_highlight - $value"
		set value [getbncuser $client tag prowl_excludechans]
		if {$value == ""} { set value "Not set" }
		bncreply "prowl_excludechans - $value"
		set value [getbncuser $client tag prowl_online]
		if {$value == ""} { set value "Not set" }
		bncreply "prowl_online - $value"
		set value [getbncuser $client tag prowl_ignore]
		if {$value == ""} { set value "Not set" }
		bncreply "prowl_ignore - $value"
		set value [getbncuser $client tag prowl_spam]
		if {$value == ""} { set value "Not set" }
		bncreply "prowl_spam - $value"
	}
}

proc push:explainsettings {client} {
	setctx $client
	bncreply "prowl_apikey - API key supplied from prowl. At the time of writing available free when registrering on their website, http://prowl.weks.net"
	bncreply "prowl_highlight - Trigger when highlighted in a channel? Can be any of 'on', 'off' '1', '0', or 'smart'. Where 'smart' utilizes a regexp to only trigger on highlights of <yournick> followed by a space or common character like , . or :"
	bncreply "prowl_excludechans - Exclude given list of channels from the prowl_highlight feature. Useful for big public channels with lots of spam. Accepts *wildcards*."
	bncreply "prowl_online - Allows you to recieve push-notifications when online. Can be any of 'on', 'off' or a numerical value for number of minutes idle as a condition to trigger."
	bncreply "prowl_ignore - List of *!*@hostmask to ignore for private message and highlight. Eg. *!*@operserv.quakenet.org"
	bncreply "prowl_spam - Minimum amount of minutes since last push-notification before notifying again."
}


proc push:command {client params} {
	if {[string equal -nocase [lindex $params 0] "set"]} {
		if {[llength $params] == 1} {
			internaltimer 0 0 push:displaysettings $client
		} elseif {[llength $params] == 2 && [lsearch -exact [list "prowl" "prowl_apikey" "prowl_highlight" "prowl_excludechans" "prowl_online" "prowl_ignore" "prowl_spam"] [lindex $params 1]] != -1} {
			internaltimer 0 0 push:explainsettings $client
			haltoutput
		} elseif {[string equal -nocase [lindex $params 1] "prowl_apikey"]} {
			setbncuser $client tag prowl_apikey [lindex $params 2]
			bncreply "Done."
			haltoutput
		} elseif {[string equal -nocase [lindex $params 1] "prowl_highlight"]} {
			if {[lsearch -exact [list "on" "off" "smart" 1 0] [lindex $params 2]] != -1} {
				setbncuser $client tag prowl_highlight [lindex $params 2]
				bncreply "Done."
				haltoutput
			} else {
				bncreply "Invalid argument. Must be one of: on, off, smart, 1 or 0."
				haltoutput
			}
		} elseif {[string equal -nocase [lindex $params 1] "prowl_excludechans"]} {
			setbncuser $client tag prowl_excludechans [lrange $params 2 end]
			bncreply "Done."
			haltoutput
		} elseif {[string equal -nocase [lindex $params 1] "prowl_ignore"]} {
			setbncuser $client tag prowl_ignore [lrange $params 2 end]
			bncreply "Done."
			haltoutput
		} elseif {[string equal -nocase [lindex $params 1] "prowl_online"]} {
			if {[lsearch -exact [list "on" "off"] [lindex $params 2]] != -1 || [string is integer -strict [lindex $params 2]]} {
				setbncuser $client tag prowl_online [lindex $params 2]
				bncreply "Done."
				haltoutput
			} else {
				bncreply "Invalid argument. Must be one of: on, off or a numeric value for number of minutes idle as a condition to trigger."
				haltoutput
			}
		} elseif {[string equal -nocase [lindex $params 1] "prowl_spam"]} {
			if {[string is integer -strict [lindex $params 2]]} {
				setbncuser $client tag prowl_spam [lindex $params 2]
				bncreply "Done."
				haltoutput
			} else {
				bncreply "Invalid argument. Must a numeric value for number of minutes since last push-notification before triggering again."
				haltoutput
			}
		}
	}
}

proc push:sendalert {apikey application event description {priority 0}} {
	set query [::http::formatQuery apikey $apikey priority $priority application $application event $event description $description]
	::http::cleanup [::http::geturl https://prowl.weks.net/publicapi/add -query $query]
}

proc push:servermsg {client params} {
	global botnick

	set apikey [getbncuser $client tag prowl_apikey]
	
	if {$apikey == ""} {
		return
	}
	
	set online [getbncuser $client tag prowl_online]
	set highlight [getbncuser $client tag prowl_highlight]
	set ignore [getbncuser $client tag prowl_ignore]
	set excludechans [getbncuser $client tag prowl_excludechans]
	set lastactive [getbncuser $client tag prowl_lastactive]
	set spam [getbncuser $client tag prowl_spam]
	set lastpush [getbncuser $client tag prowl_lastpush]

	if {![string equal -nocase [lindex $params 1] "notice"] &&
			![string equal -nocase [lindex $params 1] "privmsg"]} {
		return
	}
	
	# check for spam
	if {[string is integer -strict $spam]} {
		if {$lastpush != "" && [expr [clock seconds] - $lastpush] < [expr $spam * 60]} {
			return
		}
	}
	
	# check if the user-settings for 'prowl_online' allow current event to be pushed
	if {[getbncuser $client hasclient]} {
		if {[string is integer -strict $online]} {
			if {$lastactive == "" || [expr [clock seconds] - $lastactive] < [expr $online * 60]} {
				return
			}
		} elseif {$online != "on"} {
			return
		}
	}
	
	# check if the event source is to be ignored through 'prowl_ignore'
	foreach ign $ignore {
		if {[string match $ign [lindex $params 0]]} {
			return
		}
	}

	set network [getisupport NETWORK]
	if {$network == ""} { set network "IRC" }

	# incoming privmsg/notice
	if {[string equal -nocase [lindex $params 2] $botnick]} {
		push:sendalert $apikey $network [lindex [split [lindex $params 0] "!"] 0] [lindex $params 3]
		setbncuser $client tag prowl_lastpush [clock seconds]
	} elseif {[lsearch -exact [list "on" "smart" 1] $highlight] != -1} {
		foreach chn $excludechans {
			if {[string match -nocase $chn [lindex $params 2]]} {
				return
			}
		}
		if {$highlight == "smart"} {
			if {[regexp "(^|\[\\s\])${botnick}(\[\\s,\\.:\\!\\?\]|$)" [lindex $params 3]]} {
				push:sendalert $apikey $network [lindex [split [lindex $params 0] "!"] 0] [lindex $params 3]
				setbncuser $client tag prowl_lastpush [clock seconds]
			}
		} else {
			if {[string match "*${botnick}*" [lindex $params 3]]} {
				push:sendalert $apikey $network [lindex [split [lindex $params 0] "!"] 0] [lindex $params 3]
				setbncuser $client tag prowl_lastpush [clock seconds]
			}
		}
	}
}

proc push:clientmsg {client params} {
	if {![string equal -nocase [lindex $params 0] "notice"] &&
			![string equal -nocase [lindex $params 0] "privmsg"]} {
		return
	}
	setbncuser $client tag prowl_lastactive [clock seconds]	
}
