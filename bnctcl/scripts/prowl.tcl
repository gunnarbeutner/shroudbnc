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

proc push:displaysettings {client} {
	setctx $client
	set value [getbncuser $client tag prowl_apikey]
	if {$value == ""} { set value "Not set" }
	bncreply "prowl_apikey - $value"
}

proc push:command {client params} {
	if {[string equal -nocase [lindex $params 0] "set"]} {
		if {[llength $params] == 1} {
			internaltimer 0 0 push:displaysettings $client
		} elseif {[string equal -nocase [lindex $params 1] "prowl_apikey"]} {
			setbncuser $client tag prowl_apikey [lindex $params 2]
			bncreply "Done."
			haltoutput
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

	if {$apikey == "" || [getbncuser $client hasclient] ||
			(![string equal -nocase [lindex $params 1] "notice"] &&
			![string equal -nocase [lindex $params 1] "privmsg"])} {
		return
	}

	if {[string equal -nocase [lindex $params 2] $botnick]} {
		set network [getisupport NETWORK]
		if {$network == ""} { set network "IRC" }
		push:sendalert $apikey $network [lindex [split [lindex $params 0] "!"] 0] [lindex $params 3]
	}
}
