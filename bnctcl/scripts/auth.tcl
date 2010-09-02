# Copyright (C) 2010 Sandro Hummel
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

#
# binds
#
internalbind command auth:commands
internalbind svrlogon auth:logon

#
# checks /ison reply for 'NickServ'
#
proc auth:ison {client params} {
	if {[string equal -nocase [lindex $params 3] "NickServ"]} {
		set ::sbnc_auth([string tolower $client],nickserv) 1
		killtimer [timerexists [list sbnc:multi:command [list "internalunbind server auth:server NOTICE $client" "internalunbind server auth:server PRIVMSG $client"]]]
		internalunbind server auth:ison 303 $client
		haltoutput
	}
}

#
# cleans up
#
proc auth:cleanup {client} {
	internalunbind server auth:server NOTICE $client
	internalunbind server auth:server PRIVMSG $client
	internalunbind server auth:ison 303 $client
	killtimer [timerexists [list internalunbind server auth:server NOTICE $client]]
	killtimer [timerexists [list sbnc:multi:command [list "internalunbind server auth:server NOTICE $client" "internalunbind server auth:server PRIVMSG $client"]]]
	killtimer [timerexists [list internalunbind server auth:ison 303 $client]]
}

#
# sends login data after connecting to a server.
#
proc auth:logon {client} {
	auth:cleanup $client
	if {[info exists ::sbnc_auth([string tolower $client],nickserv)]} {
		unset ::sbnc_auth([string tolower $client],nickserv)
	}
	set authuser [getbncuser $client tag authuser]
	set authpass [getbncuser $client tag authpass]
	set auth [getbncuser $client tag auth]
	switch -nocase -- [getisupport network] {
		"QuakeNet" {
			if {![string equal -nocase $authuser ""] && ![string equal -nocase $authpass ""] && [string equal -nocase $auth "on"]} { 
				if {![llength $::qauth_supported_algorithms]} {
					putquick "PRIVMSG Q@CServe.QuakeNet.Org :AUTH $authuser $authpass"
					
				} else {
					internalbind server auth:server NOTICE $client
					putquick "PRIVMSG Q@CServe.QuakeNet.Org :CHALLENGE"
					timer 2 [list internalunbind server auth:server NOTICE $client]
				}
			}			
		}
		"GameSurge" {
			if {![string equal -nocase $authuser ""] && ![string equal -nocase $authpass ""] && [string equal -nocase $auth "on"]} { 
				putquick "AUTHSERV auth $authuser $authpass"
			}
		}	
		"UnderNet" {
			if {![string equal -nocase $authuser ""] && ![string equal -nocase $authpass ""] && [string equal -nocase $auth "on"]} { 
				putquick "PRIVMSG X@channels.undernet.org :login $authuser $authpass"
			}		
		}
		default {
			internalbind server auth:server NOTICE $client
			internalbind server auth:server PRIVMSG $client
			internalbind server auth:ison 303 $client
			timer 30 [list sbnc:multi:command [list "internalunbind server auth:server NOTICE $client" "internalunbind server auth:server PRIVMSG $client"]]]
			timer 2 [list internalunbind server auth:ison 303 $client]
			putquick "ISON NickServ"
		}
	}
}

#
# Q-auth challenge encryptions
#
set ::qauth_algorithms [list md5 sha1 sha256]
set ::qauth_supported_algorithms [list]
foreach algorithm $::qauth_algorithms {
	if {![catch [list package require $algorithm] error]} {
		lappend ::qauth_supported_algorithms $algorithm
	}
}

#
# server response parser
#
proc auth:server {client params} {
	set host [lindex $params 0]
	set msg [lindex $params 3]
	#
	# QuakeNet
	#
	if {[string equal -nocase $host "Q!TheQBot@CServe.quakenet.org"]} {
		if {[string match -nocase "You are now logged in as*" $msg]} {
			bncjoinchans $client
			if {![getbncuser $client hasclient]} {
				putlog "Successfully Authed to Q"
			}
			internalunbind server auth:server NOTICE $client
		}
		if {[string match -nocase "*CHALLENGE*" $msg]} {
			set challenge [lindex [split $msg " "] 1]
			set authuser [getbncuser $client tag authuser]
			set authuserlower [string tolower [string map -nocase {"[" "{" "]" "}" "|" "\\"} [getbncuser $client tag authuser]]]
			set authpass [string range [getbncuser $client tag authpass] 0 9]
			if {[string match -nocase "*HMAC-SHA-256*" $msg] && [lsearch $::qauth_supported_algorithms sha256] > "-1"} {
				set authpass [string tolower [::sha2::sha256 -hex $authpass]]
				set response [string tolower [::sha2::sha256 -hex "${authuserlower}:${authpass}"]]
				set response [string tolower [::sha2::hmac -hex -key $response $challenge]]
				putquick "PRIVMSG Q@CServe.QuakeNet.Org :CHALLENGEAUTH $authuser $response HMAC-SHA-256"
			} elseif {[string match -nocase "*HMAC-SHA-1*" $msg] && [lsearch $::qauth_supported_algorithms sha1] > "-1"} {
				set authpass [string tolower [::sha1::sha1 -hex $authpass]]
				set response [string tolower [::sha1::sha1 -hex "${authuserlower}:${authpass}"]]
				set response [string tolower [::sha1::hmac -hex -key $response $challenge]]
				putquick "PRIVMSG Q@CServe.QuakeNet.Org :CHALLENGEAUTH $authuser $response HMAC-SHA-1"
			} elseif {[string match -nocase "*HMAC-MD5*" $msg] && [lsearch $::qauth_supported_algorithms md5] > "-1"} {
				set authpass [string tolower [::md5::md5 -hex $authpass]]
				set response [string tolower [::md5::md5 -hex "${authuserlower}:${authpass}"]]
				set response [string tolower [::md5::hmac -hex -key $response $challenge]]
				putquick "PRIVMSG Q@CServe.QuakeNet.Org :CHALLENGEAUTH $authuser $response HMAC-MD5"
			} else {
				putquick "PRIVMSG Q@CServe.QuakeNet.Org :AUTH $authuser $authpass"
			}
		}
	}
	#
	# NickServ
	#	
	if {[string equal -nocase [lindex [split $host "!"] 0] "NickServ"]} {
		set ::sbnc_auth([string tolower $client],nickserv) 1
		if {[string match -nocase [getbncuser $client tag authphrase] $msg]} {
			if {[string equal -nocase [getbncuser $client tag authuser] $::botnick] && ![string equal -nocase [getbncuser $client tag authpass] ""] && [string equal -nocase [getbncuser $client tag auth] "on"]} { 
				putquick "NICKSERV identify [getbncuser $client tag authpass]"
			}
		}
	}		
}

#
# outputs auth info during "set"
#
proc auth:help {client} {
	if {![string equal -nocase [getbncuser $client tag authuser] ""]} {
		set authuser [getbncuser $client tag authuser]
	} else {
		set authuser "Not set"
	}
	if {![string equal -nocase [getbncuser $client tag authpass] ""]} {
		set authpass "Set"
	} else {
		set authpass "Not set"
	}
	if {[string equal -nocase [getbncuser $client tag auth] "on"]} {
		set auth "on"
	} else {
		set auth "off"
	}
	if {[info exists ::sbnc_auth([string tolower $client],nickserv)]} {
		if {![string equal -nocase [getbncuser $client tag authphrase] ""]} {
			set authphrase [getbncuser $client tag authphrase]
		} else {
			set authphrase "Not set"
		}
	}
	bncreply "authuser   - $authuser"
	bncreply "authpass   - $authpass"
	if {[info exists authphrase]} {
		bncreply "authphrase - $authphrase"
	}
	bncreply "auth       - $auth"
}

#
# commands for changing auth data/settings
#
proc auth:commands {client params} {
	if {[string equal -nocase "help" [lindex $params 0]]} {
		bncaddcommand authmanual "User" "Manually authenticates to a server's Authentication Service"
	}
	if {[string equal -nocase "authmanual" [lindex $params 0]]} {
		if {![string equal -nocase [getbncuser $client tag authuser] ""] && ![string equal -nocase [getbncuser $client tag authpass] ""] && [string equal -nocase [getbncuser $client tag auth] "on"]} {
			auth:logon $client
			bncreply "Manually triggered authing"
			haltoutput
		} else {
			bncreply "Error: either authuser or authpass isn't set or auth isn't turned on. Check /sbnc set"
			haltoutput
		}
	}
	if {[string equal -nocase "set" [lindex $params 0]]} {
		if {[llength $params] == 1} {
			internaltimer 0 0 auth:help $client
		} else {
			switch -nocase -- [lindex $params 1] {
				"authuser" {
					setbncuser $client tag authuser [lindex $params 2]
					bncreply "Done."
					haltoutput
				}
				"authpass" {
					setbncuser $client tag authpass [lindex $params 2]
					bncreply "Done."
					haltoutput
				}
				"auth" {
					if {![string equal -nocase [getbncuser $client tag authuser] ""] && ![string equal -nocase [getbncuser $client tag authpass] ""]} {
						if {![string equal -nocase [lindex $params 2] "on"] && ![string equal -nocase [lindex $params 2] "off"]} {
							bncreply "Value should be either 'on' or 'off'."
							haltoutput
						} {
							setbncuser $client tag auth [lindex $params 2]
							bncreply "Done."
							haltoutput
						}		
					} else {
						bncreply "Please set authuser and authpass first. Check /sbnc set"
						haltoutput
					}	
				}
				"authphrase" {
					if {[info exists ::sbnc_auth([string tolower $client],nickserv)]} {
						set text [join [lrange $params 2 end] " "]
						if {![string equal -nocase $text ""] && [string length $text] > 7} {
							setbncuser $client tag authphrase $text
							bncreply "Done."
							haltoutput						
						} else {
							bncreply "Error: no text specified or too short. Wildcards (*) may be used."
							haltoutput
						}
					}
				}
			}
			if {[getbncuser $client tag authuser] != "" && [getbncuser $client tag authpass] != ""} {
				setbncuser $client delayjoin 1
			} else {
				setbncuser $client delayjoin 0
			}
		}
	}
}

#
# used by auth.tcl to execute multiple commands in timers
#
proc sbnc:multi:command {commands} {
	foreach command $commands {
		catch $command
	}	
}