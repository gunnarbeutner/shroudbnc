# usys.tcl (c)2006 Alex Sajzew
# Written for shroudBNC

set max:handLength 9

internalbind server sbnc:ban:join JOIN
internalbind modec sbnc:ban:op +o
internalbind modec sbnc:ban:op +h
internalbind modec sbnc:ban:unban -b
internaltimer 60 1 sbnc:checkBans
internalbind usrdelete sbnc:userfiledelete

proc adduser {handle {hostmask ""}} {

	set handle [string range $handle 0 [expr ${::max:handLength} - 1]]

	if {[validuser $handle]} { return 0 }

	namespace eval [getns] {
		set handle [uplevel 1 {set handle}]
		if {![info exists userInfo(:[string tolower $handle])]} {
			set userInfo(:[string tolower $handle]) $handle
		}
		if {![info exists userList]} {
			set userList ""
		}
	}

	set handle [string tolower $handle]

	upvar [getns]::userInfo ui [getns]::userList ulist

	set ui(glb:$handle) "-"
	set ui(chf:$handle) ""
	lappend ulist $ui(:$handle)

	if {$hostmask != ""} {
		set ui(hosts:$handle) "{$hostmask}"
	}

	return 1

}

proc chattr {handle {changes ""} {channel ""}} {

	set handle [string tolower $handle]

	namespace eval [getns] {
		set handle [uplevel 1 {set handle}]
		if {![info exists userInfo(:$handle)]} {
			return "*"
		}
	}

	upvar [getns]::userInfo ui

	if {$changes == "" && $channel == ""} {
		return $ui(glb:$handle)
	}

	if {$changes != "" && $channel == ""} {
		if {[string index $changes 0] == "#"} {

			if {[info exists ui(chf:$handle)]} {
				foreach chan $ui(chf:$handle) {
					if {[string equal -nocase $changes [lindex [split $chan] 0]]} {
						return "$ui(glb:$handle)|[lindex [split $chan] 2]"
					}
				}
			}

			return "$ui(glb:$handle)|-"

		} else {

			set glb $ui(glb:$handle)

			if {$glb == "-"} {
				set glb ""
			}

			set action "add"

			foreach char [split $changes ""] {
				if {$char == " " || $char == ""} { continue }

				if {$char == "|"} {
					if {$glb == ""} {
						set glb "-"
					}

					set ui(glb:$handle) [join [lsort [split $glb ""]] ""]

					return [chattr $handle]
				}

				if {$char == "-"} {
					set action "remove"
				} elseif {$char == "+"} {
					set action "add"
				} else {

					if {![string is ascii $char] || ![string is alpha $char]} { continue }

					switch -- $action {
						remove {
							set glb [string map [list $char ""] $glb]
						}
						add {
							if {[string first $char $glb] == -1} {
								append glb $char
							}
						}
					}

				 }

			}

			if {$glb == ""} {
				set glb "-"
			}

			set ui(glb:$handle) [join [lsort [split $glb ""]] ""]

			return [chattr $handle]
		}

	} else {

		if {![validchan $channel]} { return -code error "no such channel" }

		set glbc [lindex [split $changes |] 0]
		set chfc [lindex [split $changes |] 1]

		set glb $ui(glb:$handle)
		set chf [lindex [split [chattr $handle $channel] |] 1]

		if {$glb == "-"} {
			set glb ""
		}

		if {$chf == "-"} {
			set chf ""
		}

		if {$glbc != "" && $glbc != "-"} {
			set glb [chattr $handle $glbc]
		}

		set action add

		foreach char [split $chfc ""] {
			if {$char == " " || $char == ""} { continue }

			if {$char == "-"} {
				set action "remove"
			} elseif {$char == "+"} {
				set action "add"
			} else {

				if {![string is ascii $char] || ![string is alpha $char]} { continue }

				switch -- $action {
					remove {
						set chf [string map [list $char ""] $chf]
					}
					add {
						if {[string first $char $chf] == -1} {
							append chf $char
						}
					}
				}

			}

		}

		set chf [join [lsort [split $chf ""]] ""]

		set i 0

		if {[info exists ui(chf:$handle)]} {

			foreach chan $ui(chf:$handle) {
				if {[string equal -nocase $channel [lindex [split $chan] 0]]} {
					set ui(chf:$handle) [lreplace $ui(chf:$handle) $i $i]
					continue
				}
				incr i
			}

		}

		if {$chf != "" && $chf != " "} {
			lappend ui(chf:$handle) "$channel [laston $handle $channel] $chf"
		}

		return [chattr $handle $channel]

	}
}

proc deluser {handle} {

	set handle [string tolower $handle]

	namespace eval [getns] {
		set handle [uplevel 1 {set handle}]
		if {![info exists userInfo(:$handle)]} {
			return 0
		}
		if {![info exists userList]} {
			set userList ""
		}
	}


	upvar [getns]::userInfo ui [getns]::userList ulist

	foreach var [array names ui "*$handle"] {
		if {[string equal -nocase [lrange [split $var :] 1 end] [split $handle :]]} {
			unset ui($var)
		}
	}

	set p [lsearch -exact [string tolower $ulist] [string tolower $handle]]
	set ulist [lreplace $ulist $p $p]

	return 1

}

proc countusers { } {

	namespace eval [getns] {
		if {![info exists userList]} {
			return 0
		} else {
			return [llength $userList]
		}
	}

}

proc laston {handle {chan ""}} {
	return 0
}

proc validuser {handle} {

	if {[chattr $handle] == "*"} {
		return 0
	} else {
		return 1
	}

}

proc matchattr {handle flags {channel ""}} {

	set gr 0
	set cr 0

	set gfn [lindex [split $flags |&] 0]
	set cfn [lindex [split $flags |&] 1]

	if {$gfn == "-"} {
		set gr 1
	}

	if {$cfn == "-"} {
		set cr 1
	}

	foreach f [split $gfn ""] {
		if {[string first $f [chattr $handle]] != -1} {
			set gr 1
		}
	}

	if {$cfn != "" && $channel != ""} {

		foreach f [split $cfn] {
			if {[string first $f [lindex [split [chattr $handle $channel] |] 1]] != -1} {
				set cr 1
			}
		}

	}

	if {[string first "&" $flags] != -1} {

		if {$gr && $cr} {
			return 1
		} else {
			return 0
		}

	} elseif {[string first "|" $flags] != -1} {

		if {$gfn == "-" || $cfn == "-"} {

			if {$gr && $cr} {
				return 1
			} else {
				return 0
			}

		} else {

			if {$gr || $cr} {
				return 1
			} else {
				return 0
			}

		}

	} else {

		return $gr

	}
}

proc setuser {args} {

	if {[llength $args] < 3} {
		return -code error "wrong # args: should be \"setuser handle type ?setting....?\""
	}

	set hand [string tolower [lindex $args 0]]
	set type [lindex $args 1]
	set setting [string map {\{ \\\{ \} \\\}} [join [lrange $args 2 end]]]

	namespace eval [getns] {
		set hand [uplevel 1 {set hand}]
		if {![info exists userInfo(:$hand)]} {
			return -code error "No such user."
		}
	}

	upvar [getns]::userInfo ui

	switch -exact -- [string tolower $type] {
		hosts {
			if {$setting == ""} {
				if {[info exists ui(hosts:$hand)]} {
					unset ui(hosts:$hand)
				}
			} else {
				set setting [lindex [split $setting] 0]
				set i 0
				set w 0

				if {[info exists ui(hosts:$hand)]} {

					foreach host $ui(hosts:$hand) {
						if {[string match -nocase $setting $host]} {
							set ui(hosts:$hand) [lreplace $ui(hosts:$hand) $i $i]
							continue
						}
						if {[string match -nocase $host $setting]} {
							set w 1
						}
						incr i
					}

				}

				if {!$w} {
					lappend ui(hosts:$hand) $setting
				}
			}
		}
		laston {
			if {[llength $setting] == 1} {
				set ui(laston:$hand) $setting
			} else {

				set v 0
				set i 0

				if {![string is integer [lindex [split $setting] 0]]} {
					set t 0
				} else {
					set t [lindex [split $setting ] 0]
				}

				foreach chan $ui(chf:$hand) {
					if {[string equal -nocase [lindex [split $chan] 0] [lindex [split $setting] 1]]} {
						set ui(chf:$hand) [lreplace $ui(chf:$hand) $i $i "[lindex [split $chan] 0] $t [lindex [split $chan] 2]"]
						set v 1
					}
					incr i
				}

				if {!$v} {
					set ui(laston:$hand) "$t [lindex [split $setting] 1]"
				}
			}
		}
		info {
			set ui(info:$hand) "{$setting}"
		}
		comment {
			set ui(comment:$hand) "{$setting}"
		}
		pass {
			set ui(pass:$hand) [md5 $setting]
			return
		}
		xtra {
			set name [lindex [split $setting] 0]
			set arg [lrange [split $setting] 1 end]

			if {[info exists ui(xtra:$hand)]} {

				set i 0

				foreach xtra $ui(xtra:$hand) {
					if {[string equal -nocase $name [lindex [split $xtra] 0]]} {
						set ui(xtra:$hand) [lreplace $ui(xtra:$hand) $i $i]
						continue
					}
					incr i
				}

				if {$arg != ""} {
					lappend ui(xtra:$hand) "$name $arg"
				}

			} else {
				set ui(xtra:$hand) "{$name $arg}"
			}
		}
		default {
			return -code error "No such info type: $type"
		}
	}
}

proc getuser {args} {

	if {[llength $args] < 2} {
		return -code error "wrong # args: should be \"getuser handle type\""
	}

	set hand [string tolower [lindex $args 0]]
	set type [lindex $args 1]
	set opt [lindex $args 2]

	namespace eval [getns] {
		set hand [uplevel 1 {set hand}]
		if {![info exists userInfo(:$hand)]} {
			return "No such user."
		}
	}

	upvar [getns]::userInfo ui

	switch -exact -- [string tolower $type] {
		hosts {
			if {[info exists ui(hosts:$hand)]} {
				return $ui(hosts:$hand)
			} else {
				return
			}
		}
		laston {
			if {![info exists $ui(laston:$hand)]} { return }
			if {$opt == ""} {
				return $ui(laston:$hand)
			} else {
				foreach chan ui(chf:$hand) {
					if {[string equal -nocase [lindex [split $chan] 0] $opt]} {
						return [lindex [split $chan] 1]
					}
			  	}

			  	return 0

			}
		}
		info {
			if {[info exists ui(info:$hand)]} {
				return [join $ui(info:$hand)]
			} else {
				return
			}
		}
		comment {
			if {[info exists ui(comment:$hand)]} {
				return [join $ui(comment:$hand)]
			} else {
				return
			}
		}
		pass {
			if {[info exists ui(pass:$hand)]} {
				return [join $ui(pass:$hand)]
			} else {
				return
			}
		}
		xtra {
			if {[info exists ui(xtra:$hand)]} {

				foreach xtra $ui(xtra:$hand) {
					if {[string equal -nocase [lindex [split $xtra] 0] $opt]} {
						return [lrange [split $xtra] 1 end]
					}
				}

			}
			return
		}
		default {
			return -code error "No such info type: $type"
		}
	}
}

proc addhost {hand host} {
	setuser $hand hosts $host
	return
}

proc delhost {handle hostmask} {

	namespace eval [getns] {
		set hand [string tolower [uplevel 1 {set handle}]]
		set host [uplevel 1 {set hostmask}]

		if {![info exists userInfo(hosts:$hand)]} {
			return 0
		}
		set i 0
		foreach mask $userInfo(hosts:$hand) {
			if {[string equal -nocase $host $mask]} {
				set userInfo(hosts:$hand) [lreplace $userInfo(hosts:$hand) $i $i]
				return 1
			}
			incr i
		}
		return 0
	}

}

proc save { } {
	global userfile

	namespace eval [getns] {
		if {![info exists userList]} {
			set userList ""
		}
	}

	upvar [getns]::userList ulist

	set file [open $userfile w+]

	puts $file "#4v: sBNC [lindex [split $::version] 0] -- [getctx] -- written [clock format [clock seconds] -format {%a %b %d %T %Y}]"

	foreach hand [lsort $ulist] {

		namespace eval [getns] {
			set hand [uplevel 1 {set hand}]
			if {![info exists userInfo(:[string tolower $hand])]} {
				continue
			}
		}

		set user $hand
		set hand [string tolower $hand]

		upvar [getns]::userInfo ui

		puts $file "$user[string repeat " " [expr ${::max:handLength}+2-[string length $hand]]]- $ui(glb:$hand)"

		set type "chf hosts laston info comment xtra"

		if {[info exists ui(chf:$hand)]} {

			foreach chan $ui(chf:$hand) {
				puts $file "! [lindex [split $chan] 0][string repeat " " [expr 21-[string length [lindex [split $chan] 0]]]][lindex [split $chan] 1] [lindex [split $chan] 2]"
			}
		}

		foreach type "hosts laston info comment pass xtra" {

			if {[info exists ui($type:$hand)]} {

				foreach arg $ui($type:$hand) {
					puts $file "--[string toupper $type] $arg"
				}

			}

		}
	}

	foreach type "bans exempts invites" {

		namespace eval [getns] {
			set type [uplevel 1 {set type}]
			set file [uplevel 1 {set file}]
			if {[info exists glbInfo($type)]} {
				switch $type {
					bans {
						puts $file "*ban - -"
						foreach ban $glbInfo($type) {
							puts $file "- $ban"
						}
					}
					exempts {
						puts $file "*exempt - -"
						foreach exempt $glbInfo($type) {
							puts $file "% $exempt"
						}
					}
					invites {
						puts $file "*Invite - -"
						foreach invite $glbInfo($type) {
							puts $file "@ $invite"
						}
					}
				}
			}
		}


		foreach c [channels] {
			set chan [string tolower $c]

			namespace eval [getns] {
				set chan [uplevel 1 {set chan}]
				set c [uplevel 1 {set c}]
				if {![info exists chanInfo(:$chan)]} {
					set chanInfo(:$chan) $c
				}
			}

			upvar [getns]::chanInfo ci

			switch $type {
				bans {
					puts $file "::$c $type"
					if {[info exists ci($type:$chan)]} {
						foreach ban $ci($type:$chan) {
							puts $file "- $ban"
						}
					}
				}
				exempts {
					puts $file "&&$c $type"
					if {[info exists ci($type:$chan)]} {
						foreach exempt $ci($type:$chan) {
							puts $file "% $exempt"
						}
					}
				}
				invites {
					puts $file "\$\$$c $type"
					if {[info exists ci($type:$chan)]} {
						foreach invite $ci($type:$chan) {
							puts $file "@ $invite"
						}
					}
				}
			}
		}
	}

	close $file

	set file [open $userfile "RDONLY CREAT"]
	set info [read $file]
	close $file

	set found 0
	foreach line [split $info \n] {
		if {$found == 0} {
			set found -1
			continue
		}

		set ftwo [string range $line 0 1]

		if {$ftwo != "*b" && $ftwo != "*e" && $ftwo != "*i" && $ftwo != "::" && $ftwo != "&&" && $ftwo != "\$\$" && $ftwo != ""} {
			set found 1
		}
	}

	if {$found == "-1"} {
		file delete $userfile
	}

	if {[lsearch -exact [info procs] "savechannels"] != -1} {
		savechannels
	}

}

proc reload { } {
	global userfile

	set file [open $userfile "RDONLY CREAT"]
	set info [read $file]
	close $file

	set u -1

	namespace eval [getns] {
		set userList ""
		array unset userInfo
		array unset chanInfo
		array unset glbInfo
	}

	upvar [getns]::userList ulist

	set ginfo 0

	foreach line [split $info \n] {
		if {$line == ""} { continue }

		if {[llength $line] == 3 && [lindex $line 1] == "-" && [string equal -nocase "[lindex $line 0][string repeat " " [expr ${::max:handLength}+2-[string length [lindex $line 0]]]]-" [string range $line 0 [expr ${::max:handLength}+2]]]} {
			set user [lindex $line 0]

			namespace eval [getns] {
				set user [uplevel 1 {set user}]
				lappend userList $user
				set userInfo(:[string tolower $user]) $user
			}

			set hand [string tolower $user]

			upvar [getns]::userInfo ui

			set ui(glb:$hand) [lindex $line 2]
			set u 1

			continue

		}

		if {$u == 1} {

			if {[string range $line 0 1] == "! "} {
				lappend ui(chf:$hand) [lrange $line 2 end]
				continue
			}

			if {[string range $line 0 1] == "--"} {
				lappend ui([string tolower [string range [lindex $line 0] 2 end]]:$hand) [lrange $line 1 end]
				continue
			}

		}

		switch -exact -- [string range $line 0 1] {
			"*b" {
				namespace eval [getns] {
					if {![info exists glbInfo(bans)]} {
						set glbInfo(bans) ""
					}
				}

				set act bans
				upvar [getns]::glbInfo gi
				set ginfo 1
			}
			"*e" {
				namespace eval [getns] {
					if {![info exists glbInfo(exempts)]} {
						set glbInfo(exempts) ""
					}
				}

				set act exempts
				upvar [getns]::glbInfo gi
				set ginfo 1
			}
			"*I" {
				namespace eval [getns] {
					if {![info exists glbInfo(invites)]} {
						set glbInfo(invites) ""
					}
				}

				set act invites
				upvar [getns]::glbInfo gi
				set ginfo 1
			}
			"::" {
				set c [string range [lindex $line 0] 2 end]
				set chan [string tolower $c]

				namespace eval [getns] {
					set chan [uplevel 1 {set chan}]
					set c [uplevel 1 {set c}]
					if {![info exists chanInfo(:$chan)]} {
						set chanInfo(:$chan) $c
					}
				}

				upvar [getns]::chanInfo ci

				set act bans
				set ginfo 0
				set u 0
			}
			"&&" {
				set c [string range [lindex $line 0] 2 end]
				set chan [string tolower $c]

				namespace eval [getns] {
					set chan [uplevel 1 {set chan}]
					set c [uplevel 1 {set c}]
					if {![info exists chanInfo(:$chan)]} {
						set chanInfo(:$chan) $c
					}
				}

				upvar [getns]::chanInfo ci

				set act exempts
				set ginfo 0
				set u 0
			}
			"\$\$" {
				set c [string range [lindex $line 0] 2 end]
				set chan [string tolower $c]

				namespace eval [getns] {
					set chan [uplevel 1 {set chan}]
					set c [uplevel 1 {set c}]
					if {![info exists chanInfo(:$chan)]} {
						set chanInfo(:$chan) $c
					}
				}

				upvar [getns]::chanInfo ci

				set act invites
				set ginfo 0
				set u 0
			}
			"- " {
				if {$ginfo} {
					lappend gi($act) [string range $line 2 end]
				} else {
					lappend ci($act:$chan) [string range $line 2 end]
				}
			}
			"% " {
				if {$ginfo} {
					lappend gi($act) [string range $line 2 end]
				} else {
					lappend ci($act:$chan) [string range $line 2 end]
				}
			}
			"@ " {
				if {$ginfo} {
					lappend gi($act) [string range $line 2 end]
				} else {
					lappend ci($act:$chan) [string range $line 2 end]
				}
			}
		}
	}
}

proc newban {ban creator comment {lifetime ""} {options ""}} {

	if {[string match -nocase $ban $::botname]} { return }

	if {[string first "!" $ban] == -1 && [string first "@" $ban] == -1} {
		set ban "$ban!*@*"
	} elseif {[string first "!" $ban] == -1 && [string first "@" $ban] != -1} {
		set ban "[lindex [split $ban @] 0]!*@[join [lrange [split $ban @] 1 end] @]"
	} elseif {[string first "!" $ban] != -1 && [string first "@" $ban] == -1} {
		set ban "$ban@*"
	}

	set last 0

	if {$lifetime == ""} {
		set lifetime +0
	} else {
		if {[string is integer $lifetime] && $lifetime != "0"} {
			set lifetime +[expr [unixtime] + ($lifetime*60)]
		} else {
			set lifetime +0
		}
	}

	if {[string equal -nocase $options "sticky"]} {
		set lifetime "${lifetime}*"
	} elseif {![string equal -nocase $options "none"] && $options != ""} {
		return -code error "invalid option $options (must be one of: sticky, none)"
	}

	foreach chan [channels] {
		foreach user [chanlist $chan] {
			if {[string match -nocase $ban "$user![getchanhost $user]"]} {
				set last [clock seconds]
				pushmode $chan -o $user
				pushmode $chan +b $ban
				putkick $chan $user $comment
			} else {
				if {[string equal -nocase $options "sticky"]} {
					pushmode $chan +b $ban
				}
			}
		}
	}

	set banline "$ban:$lifetime:+[unixtime]:$last:$creator:$comment"

	namespace eval [getns] {
		set banline [uplevel 1 {set banline}]

		if {[info exists glbInfo(bans)] && $glbInfo(bans) != ""} {
			set i 0
			set r 0
			foreach ban $glbInfo(bans) {
				if {[string equal -nocase [lindex [split $ban :] 0] [lindex [split $banline :] 0]]} {
					set glbInfo(bans) [lreplace $glbInfo(bans) $i $i $banline]
					set r 1
					continue
				}
				incr i
			}
			if {!$r} {
				set glbInfo(bans) [linsert $glbInfo(bans) 0 $banline]
			}
		} else {
			set glbInfo(bans) "{$banline}"
		}
	}

	return
}

proc killban {ban} {

	namespace eval [getns] {
		set ban [uplevel 1 {set ban}]

		if {[string is integer $ban]} {

			if {[llength $glbInfo(bans)] >= $ban} {
				incr ban -1
				set glbInfo(bans) [lreplace $glbInfo(bans) $ban $ban]
				return 1
			} else {
				return 0
			}

		} else {

			set i 0
			foreach host $glbInfo(bans) {
				if {[string equal -nocase $ban [lindex [split $host :] 0]]} {
					set glbInfo(bans) [lreplace $glbInfo(bans) $i $i]

					foreach chan [channels] {
						pushmode $chan -b $ban
					}

					return 1
				}
				incr i
			}

			return 0

		}
	}
}

proc newchanban {channel ban creator comment {lifetime ""} {options ""}} {

	if {![validchan $channel]} {
		return -code error "invalid channel: $channel"
	}

	if {[string match -nocase $ban $::botname]} { return }

	if {[string first "!" $ban] == -1 && [string first "@" $ban] == -1} {
		set ban "$ban!*@*"
	} elseif {[string first "!" $ban] == -1 && [string first "@" $ban] != -1} {
		set ban "[lindex [split $ban @] 0]!*@[join [lrange [split $ban @] 1 end] @]"
	} elseif {[string first "!" $ban] != -1 && [string first "@" $ban] == -1} {
		set ban "$ban@*"
	}

	set last 0

	if {$lifetime == ""} {
		set lifetime +0
	} else {
		if {[string is integer $lifetime] && $lifetime != "0"} {
			set lifetime +[expr [unixtime] + ($lifetime*60)]
		} else {
			set lifetime +0
		}
	}

	if {[string equal -nocase $options "sticky"]} {
		set lifetime "${lifetime}*"
	} elseif {![string equal -nocase $options "none"] && $options != ""} {
		return -code error "invalid option $options (must be one of: sticky, none)"
	}

	set chan [string tolower $channel]

	foreach user [chanlist $chan] {
		if {[string match -nocase $ban "$user![getchanhost $user]"]} {
			set last [clock seconds]
			pushmode $chan -o $user
			pushmode $chan +b $ban
			putkick $chan $user $comment
		} else {
			if {[string equal -nocase $options "sticky"]} {
				pushmode $chan +b $ban
			}
		}
	}

	set banline "$ban:$lifetime:+[unixtime]:$last:$creator:$comment"

	namespace eval [getns] {
		set banline [uplevel 1 {set banline}]
		set chan [uplevel 1 {set chan}]

		if {[info exists chanInfo(bans:$chan)] && $chanInfo(bans:$chan) != ""} {
			set i 0
			set r 0
			foreach ban $chanInfo(bans:$chan) {
				if {[string equal -nocase [lindex [split $ban :] 0] [lindex [split $banline :] 0]]} {
					set chanInfo(bans:$chan) [lreplace $chanInfo(bans:$chan) $i $i $banline]
					set r 1
					continue
				}
				incr i
			}
			if {!$r} {
				set chanInfo(bans:$chan) [linsert $chanInfo(bans:$chan) 0 $banline]
			}
		} else {
			set chanInfo(bans:$chan) "{$banline}"
		}
	}

	return
}

proc killchanban {channel ban} {

	if {![validchan $channel]} {
		return -code error "invalid channel: $channel"
	}

	namespace eval [getns] {
		set ban [uplevel 1 {set ban}]
		set chan [string tolower [uplevel 1 {set channel}]]

		if {[string is integer $ban]} {

			if {[llength $chanInfo(bans:$chan)] >= $ban} {
				incr ban -1
				set chanInfo(bans:$chan) [lreplace $chanInfo(bans:$chan) $ban $ban]
				return 1
			} else {
				return 0
			}

		} else {

			set i 0
			foreach host $chanInfo(bans:$chan) {
				if {[string equal -nocase $ban [lindex [split $host :] 0]]} {
					set chanInfo(bans:$chan) [lreplace $chanInfo(bans:$chan) $i $i]
					pushmode $chan -b $ban
					return 1
				}
				incr i
			}

			return 0

		}
	}
}

proc banlist {{channel ""}} {

	if {$channel == ""} {
		namespace eval [getns] {
			if {![info exists glbInfo(bans)]} {
				return
			}
		}

		upvar [getns]::glbInfo(bans) bans

	} else {
	  	if {![validchan $channel]} {
			return -code error "invalid channel: $channel"
		}
		namespace eval [getns] {
			set chan [string tolower [uplevel 1 {set channel}]]
			if {![info exists chanInfo(bans:$chan)]} {
				return
			}
		}

		upvar [getns]::chanInfo(bans:[string tolower $channel]) bans

	}

	foreach ban $bans {
		set comment [join [lrange [split $ban :] 5 end] :]
		set host [lindex [split $ban :] 0]
		set creator [lindex [split $ban :] 4]
		set etime [string range [lindex [split $ban :] 1] 1 end]

		if {[llength [split $comment]] > 1} {
			set comment "{$comment}"
		}
		if {[llength [split $host]] > 1} {
			set host "{$host}"
		}
		if {[llength [split $creator]] > 1} {
			set creator "{$creator}"
		}
		if {[string index $etime end] == "*"} {
			set etime [string range $etime 0 end-1]
		}

		lappend banlist "$host $comment $etime [string range [lindex [split $ban :] 2] 1 end] [lindex [split $ban :] 3] $creator"
	}

	if {[info exists banlist]} {
		return $banlist
	} else {
		return ""
	}
}

proc sbnc:ban:join {client parameters} {

	namespace eval [getns] {
		set parameters [uplevel 1 {set parameters}]
		set host [lindex $parameters 0]
		set user [lindex [split $host !] 0]
		set chan [string tolower [lindex $parameters 2]]

		if {[string equal -nocase $::botname $host]} { return }

		if {[info exists glbInfo(bans)]} {

			set i 0
			foreach gban $glbInfo(bans) {
				if {[string match -nocase [lindex [split $gban :] 0] $host]} {
					set comment [join [lrange [split $gban :] 5 end] :]
					set glbInfo(bans) [lreplace $glbInfo(bans) $i $i [join [lreplace [split $gban :] 3 3 [clock seconds]] :]]
					pushmode $chan -o $user
					pushmode $chan +b [lindex [split $gban :] 0]
					putkick $chan $user $comment
				}
				incr i
			}

		}

		if {[info exists chanInfo(bans:$chan)]} {

			set i 0
			foreach cban $chanInfo(bans:$chan) {
				if {[string match -nocase [lindex [split $cban :] 0] $host]} {
					set comment [join [lrange [split $cban :] 5 end] :]
					set chanInfo(bans:$chan) [lreplace $chanInfo(bans:$chan) $i $i [join [lreplace [split $cban :] 3 3 [clock seconds]] :]]
					pushmode $chan -o $user
					pushmode $chan +b [lindex [split $cban :] 0]
					putkick $chan $user $comment
				}
				incr i
			}

		}
	}

}

proc sbnc:ban:op {client parameters} {

	if {![string equal -nocase $::botnick [lindex $parameters 3]]} { return }

	namespace eval [getns] {
		set parameters [uplevel 1 {set parameters}]
		set chan [string tolower [lindex $parameters 1]]

		foreach user [chanlist $chan] {
			set host "$user![getchanhost $user]"

			if {[info exists glbInfo(bans)]} {

				set i 0
				foreach gban $glbInfo(bans) {
					if {[string match -nocase [lindex [split $gban :] 0] $host]} {
						set comment [join [lrange [split $gban :] 5 end] :]
						set glbInfo(bans) [lreplace $glbInfo(bans) $i $i [join [lreplace [split $gban :] 3 3 [clock seconds]] :]]
						pushmode $chan -o $user
						pushmode $chan +b [lindex [split $gban :] 0]
						putkick $chan $user $comment
					} else {
						if {[string index [lindex [split $gban :] 1] end] == "*"} {
							pushmode $chan +b [lindex [split $gban :] 0]
						}
					}
				}

			}

			if {[info exists chanInfo(bans:$chan)]} {

				foreach cban $chanInfo(bans:$chan) {
					if {[string match -nocase [lindex [split $cban :] 0] $host]} {
						set comment [join [lrange [split $cban :] 5 end] :]
						set chanInfo(bans:$chan) [lreplace $chanInfo(bans:$chan) $i $i [join [lreplace [split $cban :] 3 3 [clock seconds]] :]]
						pushmode $chan -o $user
						pushmode $chan +b [lindex [split $cban :] 0]
						putkick $chan $user $comment
					} else {
						if {[string index [lindex [split $cban :] 1] end] == "*"} {
							pushmode $chan +b [lindex [split $cban :] 0]
						}
					}
				}

			}
		}
	}

}

proc sbnc:ban:unban {client parameters} {

	namespace eval [getns] {
		set parameters [uplevel 1 {set parameters}]
		set chan [string tolower [lindex $parameters 1]]
		set host [lindex $parameters 3]

		if {[info exists glbInfo(bans)]} {
			foreach ban $glbInfo(bans) {
				if {[string equal -nocase $host [lindex [split $ban :] 0]] && [string index [lindex [split $ban :] 1] end] == "*"} {
					pushmode $chan +b [lindex [split $ban :] 0]
				}
			}
		}

		if {[info exists chanInfo(bans:$chan)]} {
			foreach ban $chanInfo(bans:$chan) {
				if {[string equal -nocase $host [lindex [split $ban :] 0]] && [string index [lindex [split $ban :] 1] end] == "*"} {
					pushmode $chan +b [lindex [split $ban :] 0]
				}
		   	}
		}
	}

}


proc sbnc:checkBans { } {

	foreach user [bncuserlist] {
		setctx $user
		namespace eval [getns] {
			if {[info exists glbInfo(bans)]} {
				set i 0
				foreach ban $glbInfo(bans) {
					set lifetime [lindex [split $ban :] 1]
					if {[string index $lifetime end] == "*"} {
						set lifetime [string range $lifetime 1 end-1]
					} else {
						set lifetime [string range $lifetime 1 end]
					}
					if {[clock seconds] >= $lifetime && $lifetime != 0} {
						set glbInfo(bans) [lreplace $glbInfo(bans) $i $i]

						foreach chan [channels] {
							pushmode $chan -b [lindex [split $ban :] 0]
						}

						continue
					}
					incr i
				}
			}

			set i 0

			foreach chan [channels] {
				if {[info exists chanInfo(bans:$chan)]} {
					foreach ban $chanInfo(bans:$chan) {
						set lifetime [lindex [split $ban :] 1]
						if {[string index $lifetime end] == "*"} {
							set lifetime [string range $lifetime 1 end-1]
						} else {
							set lifetime [string range $lifetime 1 end]
						}
						if {[clock seconds] >= $lifetime && $lifetime != 0} {
							set chanInfo(bans:$chan) [lreplace $chanInfo(bans:$chan) $i $i]
							pushmode $chan -b [lindex [split $ban :] 0]
							continue
						}
						incr i
					}
				}
			}
		}
	}
}

proc stick {banmask {channel ""}} {

	if {$channel == ""} {
		namespace eval [getns] {
			if {![info exists glbInfo(bans)] || $glbInfo(bans) == ""} {
				return 0
			}
			set host [uplevel 1 {set banmask}]

			set i 0
			foreach ban $glbInfo(bans) {
				if {[string equal -nocase $host [lindex [split $ban :] 0]]} {
					if {[string index [lindex [split $ban :] 1] end] == "*"} {
						return 1
					} else {
						set glbInfo(bans) [lreplace $glbInfo(bans) $i $i [join [lreplace [split $ban :] 1 1 [lindex [split $ban :] 1]*] :]]
						foreach chan [channels] {
							pushmode $chan +b [lindex [split $ban :] 0]
					  	}
						return 1
					}
				}
				incr i
			}
		}
	} else {
		namespace eval [getns] {
			set chan [string tolower [uplevel 1 {set channel}]]
			if {![info exists chanInfo(bans:$chan)] || $chanInfo(bans:$chan) == ""} {
				return 0
			}
			set host [uplevel 1 {set banmask}]

			set i 0
			foreach ban $chanInfo(bans:$chan) {
				if {[string equal -nocase $host [lindex [split $ban :] 0]]} {
					if {[string index [lindex [split $ban :] 1] end] == "*"} {
						return 1
					} else {
						 set chanInfo(bans:$chan) [lreplace $chanInfo(bans:$chan) $i $i [join [lreplace [split $ban :] 1 1 [lindex [split $ban :] 1]*] :]]
						 pushmode $chan +b [lindex [split $ban :] 0]
						 return 1
					}
				}
				incr i
			}
		}
	}

	return 0

}

proc unstick {banmask {channel ""}} {

	if {$channel == ""} {
		namespace eval [getns] {
			if {![info exists glbInfo(bans)] || $glbInfo(bans) == ""} {
				return 0
			}
			set host [uplevel 1 {set banmask}]

			set i 0
			foreach ban $glbInfo(bans) {
				if {[string equal -nocase $host [lindex [split $ban :] 0]]} {
					if {[string index [lindex [split $ban :] 1] end] != "*"} {
						return 1
					} else {
						set glbInfo(bans) [lreplace $glbInfo(bans) $i $i [join [lreplace [split $ban :] 1 1 [string range [lindex [split $ban :] 1] 0 end-1]] :]]
						return 1
					}
				}
				incr i
			}
		}
	} else {
		namespace eval [getns] {
			set chan [string tolower [uplevel 1 {set channel}]]
			if {![info exists chanInfo(bans:$chan)] || $chanInfo(bans:$chan) == ""} {
				return 0
			}
			set host [uplevel 1 {set banmask}]

			set i 0
			foreach ban $chanInfo(bans:$chan) {
				if {[string equal -nocase $host [lindex [split $ban :] 0]]} {
					if {[string index [lindex [split $ban :] 1] end] != "*"} {
						return 1
					} else {
						 set chanInfo(bans:$chan) [lreplace $chanInfo(bans:$chan) $i $i [join [lreplace [split $ban :] 1 1 [string range [lindex [split $ban :] 1] 0 end-1]] :]]
						 return 1
					}
				}
				incr i
			}
		}
	}

	return 0

}

proc isban {ban {channel ""}} {

	namespace eval [getns] {
		set ban [uplevel 1 {set ban}]

		if {[info exists glbInfo(bans)] && $glbInfo(bans) != ""} {

			foreach banmask $glbInfo(bans) {
				if {[string equal -nocase $ban [lindex [split $banmask :] 0]]} {
					return 1
				}
			}
		}

		set chan [string tolower [uplevel 1 {set channel}]]

		if {$chan != ""} {
			if {[info exists chanInfo(bans:$chan)] && $chanInfo(bans:$chan) != ""} {
				foreach banmask $chanInfo(bans:$chan) {
					if {[string equal -nocase $ban [lindex [split $banmask :] 0]]} {
						return 1
					}
				}
			}
		}
	}

	return 0

}

proc ispermban {ban {channel ""}} {

	namespace eval [getns] {
		set ban [uplevel 1 {set ban}]

		if {[info exists glbInfo(bans)] && $glbInfo(bans) != ""} {

			foreach banmask $glbInfo(bans) {
				if {[string equal -nocase $ban [lindex [split $banmask :] 0]] && [string range [lindex [split $banmask :] 1] 0 1] == +0} {
					return 1
				}
			}
		}

		set chan [string tolower [uplevel 1 {set channel}]]

		if {$chan != ""} {
			if {[info exists chanInfo(bans:$chan)] && $chanInfo(bans:$chan) != ""} {
				foreach banmask $chanInfo(bans:$chan) {
					if {[string equal -nocase $ban [lindex [split $banmask :] 0]] && [string range [lindex [split $banmask :] 1] 0 1] == +0} {
						return 1
					}
				}
			}
		}
	}

	return 0

}

proc matchban {args} {

	if {[llength $args] == 0 || [llength $args] > 2} {
		return -code error "wrong # args: should be \"matchban user!nick@host ?channel?\""
	}

	namespace eval [getns] {
		set args [uplevel 1 {set args}]
		set mask [lindex $args 0]
		set chan [string tolower [lindex $args 1]]

		if {[info exists glbInfo(bans)] && $glbInfo(bans) != ""} {

			foreach ban $glbInfo(bans) {
				if {[string match -nocase [lindex [split $ban :] 0] $mask]} {
					return 1
				}
			}
		}

		if {$chan != ""} {
			if {[info exists chanInfo(bans:$chan)] && $chanInfo(bans:$chan) != ""} {
				foreach ban $chanInfo(bans:$chan) {
					if {[string match -nocase [lindex [split $ban :] 0] $mask]} {
						return 1
					}
				}
			}
		}
	}

	return 0

}

proc finduser {args} {

	if {[llength $args] != 1} {
		return -code error "wrong # args: should be \"finduser nick!user@host\""
	}

	namespace eval [getns] {
		if {![info exists userList]} {
			set userList ""
		}
	}

	upvar [getns]::userList ulist

	foreach user [string tolower $ulist] {

		namespace eval [getns] {
			set user [uplevel 1 {set user}]
			if {![info exists userInfo(hosts:$user)]} {
				continue
			}
		}

		upvar [getns]::userInfo ui

		foreach host $ui(hosts:$user) {
			if {[string match -nocase $host $args]} {
				set i [string length [join [lrange [regexp -inline [string tolower [string map {* "(.*?)"} $host]] [string tolower $args]] 1 end] ""]]
				if {[info exists b]} {
					if {$i<$b} {
						set h $user
					}
				} else {
					set h $user
				}
			}
		}

	}

	if {[info exists h]} {
		return $h
	} else {
		return "*"
	}

}

proc userlist {args} {

	if {[llength [split $args]] > 2} {
		return -code error "wrong # args: should be \"userlist ?flags ?channel??\""
	}

	set flags [lindex [split $args] 0]
	set glb [lindex [split $flags |&] 0]
	set chan [lindex [split $flags |&] 1]
	set channel [lindex [split $args] 1]

	if {$channel != "" && ![validchan $channel]} {
		return -code error "Invalid channel: $channel"
	}

	namespace eval [getns] {
		if {![info exists userList]} {
			return
		}
	}

	upvar [getns]::userList ul

	if {[llength [split $args]] == 0} {
		return $ul
	} else {

		foreach user $ul {
			if {$glb != "" && [matchattr $user $glb]} {
				set go($user) 1
			}
			if {$channel != ""} {
				if {$chan != "" && [matchattr $user |$chan $channel]} {
					set co($user) 1
				}
			} else {
				foreach schan [channels] {
					if {[matchattr $user |$chan $schan]} {
						set co($user) 1
					}
				}
			}

			if {[string first "&" $flags] != -1} {
				if {(($glb != "" && $chan != "") && ([info exists go($user)] && [info exists co($user)])) || ($glb == "" && [info exists co($user)]) || ($chan == "" && [info exists go($user)])} {
					lappend rlist $user
				}
			} elseif {[string first "|" $flags] != -1} {
				if {(($glb != "" && $chan != "") && ([info exists go($user)] || [info exists co($user)])) || ($glb == "" && [info exists co($user)]) || ($chan == "" && [info exists go($user)])} {
					lappend rlist $user
				}
			} else {
				if {[info exists go($user)]} {
					lappend rlist $user
				}
			}
		}

		if {[info exists rlist]} {
			return $rlist
		} else {
			return
		}
	}
}

proc maskhost {args} {

	if {[llength $args] != 1} {
		return -code error "wrong # args: should be \"maskhost nick!user@host\""
	}

	set args [join $args]

	if {[string first "@" $args] == -1} {
		set host [join $args]
	} else {
		set host [join [lrange [split $args @] 1 end] @]
	}

	if {[string first "!" $args] != -1 && ![string equal $args $host]} {
		set user [join [lrange [split [lindex [split $args @] 0] !] 1 end] !]
	} elseif {[string first "!" $args] == -1 && ![string equal $args $host]} {
		set user [lindex [split $args @] 0]
	} else {
		set user "*"
	}

	if {[string index $user 0] == "~"} {
		set user "*[string range $user 1 end]"
	}

	if {[llength [split $host .]] > 2} {
		regexp {([0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.)[0-9]{1,3}} $host tmp res

		if {![info exists res]} {
			regexp {(\..*)} $host tmp res
			set res "*$res"
		} else {
			set res "$res*"
		}
	} else {
		set res $host
	}

	return [join *!$user@$res]

}

proc passwdok {handle passwd} {

	if {$passwd == "-"} {
		set passwd ""
	}

	return [string equal [md5 $passwd] [getuser $handle pass]]

}

proc sbnc:userfiledelete {client} {
	file delete "users/$client.user"
}

internaltimer 300 1 sbnc:userpulse
internalbind unload sbnc:userunload

proc sbnc:userpulse {} {
	foreach user [bncuserlist] {
		setctx $user
		save
	}
}

proc sbnc:userunload { } {
	foreach user [bncuserlist] {
		setctx $user
		save
	}
}

if {![info exists sbnc:userinit]} {
	foreach user [bncuserlist] {
		setctx $user
		reload
	}
	set sbnc:userinit 1
}
