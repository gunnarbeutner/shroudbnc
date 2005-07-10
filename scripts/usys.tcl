set max:handLength 9

proc adduser {handle {hostmask ""}} {

	set handle [string range [string map {: ""} $handle] 0 [expr ${::max:handLength} - 1]]

	if {[validuser $handle]} { return 0 }

	namespace eval [getns] {
		set handle [uplevel 1 {set handle}]
		if {![array exists userInfo:[string tolower ${handle}]]} {
			array set userInfo:[string tolower ${handle}] ""
		}
	}

	upvar [getns]::userInfo:[string tolower ${handle}] u [getns]::userList ulist

	set u(glb) "-"
	lappend ulist $handle

	if {$hostmask != ""} {
		set u(hosts) "{$hostmask}"
	}

	return 1

}

proc chattr {handle {changes ""} {channel ""}} {

	namespace eval [getns] {
		set handle [uplevel 1 {set handle}]
		if {![array exists userInfo:[string tolower ${handle}]]} {
			array set userInfo:[string tolower ${handle}] ""
		}
	}

	upvar [getns]::userInfo:[string tolower ${handle}] u

	if {$changes == "" && $channel == ""} {

		if {[info exists u(glb)]} {
			return [set u(glb)]
		} else {
			return "*"
		}

	}

	if {$changes != "" && $channel == ""} {

		if {[string index $changes 0] == "#"} {

			if {![validuser $handle]} { return "*" }

			if {![info exists u(chanf)]} {
				return "[chattr $handle]|-"
			}

			foreach chan [set u(chanf)] {

				if {[string equal -nocase $changes [lindex $chan 0]]} {
					return "[chattr $handle]|[lindex $chan 2]"
				}

			}

			return "[chattr $handle]|-"

		} elseif {[string index $changes 0] == "-"} {

			if {![validuser $handle]} { return "*" }

			set glob [chattr $handle]

			if {$glob == "-"} {
				set glob ""
			}

			foreach f [split [string range $changes 1 end] ""] {
				if {![string is ascii "$f"] || ![string is alpha "$f"]} { continue }
				set glob [string map [list $f ""] $glob]
			}

			if {$glob == " " || $glob == ""} {
				set glob "-"
			}

			set glob [join $glob ""]

			set glob [join [lsort [split $glob ""]] ""]

			set u(glb) $glob

			return [chattr $handle]

		} else {

			if {[string index $changes 0] == "+"} {
				set changes [string range $changes 1 end]
			}

			if {![validuser $handle]} { return "*" }

			set glob [chattr $handle]

			if {$glob == "-"} {
				set glob ""
			}

			foreach f [split $changes ""] {
				if {![string is ascii "$f"] || ![string is alpha "$f"]} { continue }
				if {[string first $f $glob] == -1} {
					lappend glob $f
				}
			}

			set glob [join $glob ""]

			set glob [join [lsort [split $glob ""]] ""]

			set u(glb) $glob

			return [chattr $handle]

		}

	} else {

		set i 0

		if {![validuser $handle]} { return "*" }
		if {![validchan $channel]} { return -code error "no such channel" }

	  	set globc [lindex [split $changes |] 0]
	  	set cfc [lindex [split $changes |] 1]

	  	set glob [chattr $handle]
	  	set cf [lindex [split [chattr $handle $channel] |] 1]

	  	if {$glob == "-"} {
	  		set glob ""
	  	}

	  	if {$cf == "-"} {
	  		set cf ""
	  	}

	  	if {$globc != "" && $globc != "-"} {
	  		set glob [chattr $handle $globc]
	  	} else {
	  		set glob [chattr $handle]
	  	}

	  	if {[string index $cfc 0] == "-"} {

	  		foreach f [split [string range $cfc 1 end] ""] {
	  			if {![string is ascii "$f"] || ![string is alpha "$f"]} { continue }
	  			set cf [string map [list $f ""] $cf]
	  	   	}

	  	} else {

	  		if {[string index $cfc 0] == "+"} {
	  			set cfc [string range $cfc 1 end]
	  		}

	  		foreach f [split $cfc ""] {

				if {![string is ascii "$f"] || ![string is alpha "$f"]} { continue }
	  			if {[string first $f $cf] == -1} {
	  				lappend cf $f
	  			}

	  		}

	  	}

	  	set cf [join $cf ""]
		set cf [join [lsort [split $cf ""]] ""]

		if {$cf == "" || $cf == " "} {
			set cf "-"
		}

		set u(glb) $glob

		set i 0

		if {[info exists u(chanf)]} {
			foreach chan $u(chanf) {
				if {[string equal -nocase $channel [lindex $chan 0]]} {
					set u(chanf) [lreplace $u(chanf) $i $i]
					continue
				}
				incr i
	  		}
	  	}

		if {$cf != "-"} {

	  		lappend u(chanf) "$channel [laston $handle $channel] $cf"

	  	}

	  	return [chattr $handle $channel]

	}
}

proc deluser {handle} {

	namespace eval [getns] {
		set handle [uplevel 1 {set handle}]
		if {![array exists userInfo:[string tolower ${handle}]]} {
			array set userInfo:[string tolower ${handle}]
		}
		if {![info exists userList]} {
			set userList ""
		}
	}

	upvar [getns]::userInfo:[string tolower ${handle}] u [getns]::userList ulist

	if {[validuser $handle]} {

		set i [lsearch -exact [string tolower $ulist] [string tolower $handle]]
		set ulist [lreplace $ulist $i $i]

		if {[array exists u]} {
			array unset u
		}

		return 1

	} else {

		return 0

	}

}

proc countusers { } {

	namespace eval [getns] {
		if {![info exists userList]} {
			set userList ""
		}
	}

	upvar [getns]::userList ulist

	return [llength $ulist]

}

proc validuser {handle} {

	namespace eval [getns] {
		if {![info exists userList]} {
			set userList ""
		}
	}

	upvar [getns]::userList ulist

	if {![info exists ulist]} { return 0 }

	if {[lsearch -exact [string tolower $ulist] [string tolower $handle]] != -1} {
		return 1
	} else {
		return 0
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

		foreach f [split $cfn ""] {
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

		if {$gr || $cr} {
			return 1
		} else {
			return 0
		}

	} else {

		return $gr

	}
}




proc laston {handle {chan ""}} {
	return 0
}


proc setuser {args} {

	if {[llength $args] < 3} {
		return -code error "wrong # args: should be \"setuser handle type ?setting....?\""
	}

	set hand [lindex $args 0]
	set type [lindex $args 1]
	set setting [string map {\{ \\\{ \} \\\}} [join [lrange $args 2 end]]]

	if {![validuser $hand]} {
		return "No such user."
	}

	namespace eval [getns] {
		set hand [uplevel 1 {set hand}]
		if {![array exists userInfo:[string tolower ${hand}]]} {
			array set userInfo:[string tolower ${hand}] ""
		}
	}

	upvar [getns]::userInfo:[string tolower ${hand}] u

	switch -exact -- [string tolower $type] {
		hosts {
			if {$setting == ""} {
				if {[info exists u(hosts)]} {
					unset u(hosts)
				}
			} else {
				set setting [lindex [split $setting] 0]
				set i 0
				set w 0

				if {[info exists u(hosts)]} {

					foreach h $u(hosts) {
						if {[string match -nocase $setting $h]} {
							set u(hosts) [lreplace $u(hosts) $i $i]
							continue
						}
						if {[string match -nocase $h $setting]} {
							set w 1
						}
						incr i
					}

				}

				if {!$w} {
					lappend u(hosts) $setting
				}
			}
		}
		laston {
			if {[llength $setting] == 1} {
				set u(laston) $setting
			} else {

				set v 0
				set i 0

				if {![string is integer [lindex [split $setting] 0]]} {
					set t 0
				} else {
					set t [lindex [split $setting] 0]
				}

				foreach c $u(chanf) {
					if {[string equal -nocase [lindex [split $c] 0] [lindex [split $setting] 1]]} {
						set u(chanf) [lreplace $u(chanf) $i $i "[lindex [split $c] 0][string repeat " " [expr 21 - [string length [lindex [split $c] 0]]]]$t [lindex [split $c] 2]"]
						set v 1
					}
					incr i
				}

				if {!$v} {
					set u(laston) "$t [lindex [split $setting] 1]"
				}
			}
		}
		info {
			set u(info) "{$setting}"
		}
		comment {
			set u(comment) "{$setting}"
		}
		xtra {
			set name [lindex [split $setting] 0]
			set arg [lrange [split $setting] 1 end]

			if {[info exists u(xtra)]} {

				set i 0

				foreach x $u(xtra) {
					if {[string equal -nocase $name [lindex [split $x] 0]]} {
						set u(xtra) [lreplace $u(xtra) $i $i]
					}
				}
				if {$arg != ""} {
					lappend u(xtra) "$name $arg"
				}

			} else {
				set u(xtra) "{$name $arg}"
			}
		}
		default {
			return -code error "No such info type: $type"
		}
	}
	return
}

proc getuser {args} {
	global userfile

	if {[llength $args] < 2} {
		return -code error "wrong # args: should be \"getuser handle type\""
	}

	set hand [lindex $args 0]
	set type [lindex $args 1]
	set opt [lindex $args 2]

	if {![validuser $hand]} {
		return "No such user."
	}

	namespace eval [getns] {
		set hand [uplevel 1 {set hand}]
		if {![array exists userInfo:[string tolower ${hand}]]} {
			array set userInfo:[string tolower ${hand}] ""
		}
	}

	upvar [getns]::userInfo:[string tolower ${hand}] u

	switch -exact -- [string tolower $type] {
		hosts {
			if {[info exists u(hosts)]} {
				return $u(hosts)
			} else {
				return
			}
		}
		laston {
			if {$opt == ""} {
				return $u(laston)
			} else {
				foreach c $u(chanf) {
					if {[string equal -nocase [lindex [split $c] 0] $opt]} {
						return [lindex $c 1]
					} else {
						return 0
					}
				}
			}
		}
		info {
			if {[info exists u(info)]} {
				return [join $u(info)]
			} else {
				return
			}
		}
		comment {
			if {[info exists u(comment)]} {
				return [join $u(comment)]
			} else {
				return
			}
		}
		xtra {
			if {[info exists u(xtra)]} {
				foreach x $u(xtra) {
					if {[string equal -nocase [lindex [split $x] 0] $opt]} {
						return [lrange [split $x] 1 end]
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

proc reload { } {
	global userfile

	set file [open $userfile "RDONLY CREAT"]
	set info [read $file]
	close $file

	set u -1

	namespace eval [getns] {
		set chanList ""
		if {![info exists userList]} {
			set userList ""
		} else {
			foreach nick $userList {
				if {[array exists userInfo:[string tolower ${nick}]]} {
					array unset userInfo:[string tolower ${nick}]
				}
		  	}
		  	set userList ""
		}
	}

	upvar [getns]::chanList clist [getns]::userList ulist

	foreach line [split $info \n] {
		if {$line == ""} { continue }

		if {[llength $line] == 3 && [lindex $line 1] == "-" && [string equal "[lindex $line 0][string repeat " " [expr ${::max:handLength}+2-[string length [lindex $line 0]]]]-" [string range $line 0 11]]} {
			set user [lindex $line 0]

			namespace eval [getns] {
				set user [uplevel 1 {set user}]
				if {![array exists userInfo:[string tolower ${user}]]} {
					array set userInfo:[string tolower ${user}] ""
				}
			}

			upvar [getns]::userInfo:[string tolower ${user}] us
			set u 1
			lappend ulist $user
			set us(glb) [lindex $line 2]
		}

		if {$u == 1} {

			if {[string range $line 0 1] == "! "} {
				lappend us(chanf) [lrange $line 1 end]
			}

			if {[string range $line 0 1] == "--"} {
				lappend us([string tolower [string range [lindex $line 0] 2 end]]) [lrange $line 1 end]
			}

		}

		switch -exact -- [string range $line 0 1] {
			"::" {
				set chan [string range [lindex $line 0] 2 end]

				namespace eval [getns] {
					set chan [uplevel 1 {set chan}]
					if {![array exists chanInfo:[string tolower ${chan}]]} {
						set chanInfo:[string tolower ${chan}] ""
					}
				}

				upvar [getns]::chanInfo:[string tolower ${chan}] ch
				set act bans
				set u 0
				lappend clist $chan
			}
			"&&" {
				set chan [string range [lindex $line 0] 2 end]

				namespace eval [getns] {
					set chan [uplevel 1 {set chan}]
					if {![array exists chanInfo:[string tolower ${chan}]]} {
						set chanInfo:[string tolower ${chan}] ""
					}
				}

				upvar [getns]::chanInfo:[string tolower ${chan}] ch
				set act exempts
				set u 0
				lappend clist $chan
			}
			"\$\$" {
				set chan [string range [lindex $line 0] 2 end]

				namespace eval [getns] {
					set chan [uplevel 1 {set chan}]
					if {![array exists chanInfo:[string tolower ${chan}]]} {
						set chanInfo:[string tolower ${chan}] ""
					}
				}

				upvar [getns]::chanInfo:[string tolower ${chan}] ch
				set act invites
				set u 0
				lappend clist $chan
			}
		}

		if {$u == 0} {

			if {[string range $line 0 1] == "- "} {
				lappend ch($act) [string range $line 2 end]
			}

		}
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

	foreach user $ulist {

		namespace eval [getns] {
			set user [uplevel 1 {set user}]
			if {![array exists userInfo[string tolower ${user}]]} {
				array set userInfo[string tolower ${user}] ""
			}
		}

		upvar [getns]::userInfo:[string tolower ${user}] u
		puts $file "$user[string repeat " " [expr ${::max:handLength}+2-[string length $user]]]- [set u(glb)]"
		foreach a [lsort [array names u]] {
			if {[string equal -nocase $a "glb"]} { continue }
			foreach arg $u($a) {
				if {$arg == "" || $arg == " "} { continue }
				if {[string equal -nocase $a "chanf"]} {
					puts $file "! [lindex $arg 0][string repeat " " [expr 21-[string length [lindex $arg 0]]]][lindex $arg 1] [lindex $arg 2]"
				} else {
					puts $file "--[string toupper $a] $arg"
				}
			}
		}
	}

	foreach type "bans exempts invites" {
		foreach chan [string tolower [channels]] {

			namespace eval [getns] {
				set chan [uplevel 1 {set chan}]
				if {![array exists chanInfo:[string tolower ${chan}]]} {
					set chanInfo:[string tolower ${chan}] ""
				}
			}

			upvar [getns]::chanInfo:[string tolower ${chan}] ch

			switch $type {
				bans {
					puts $file "::$chan bans"
					if {[info exists ch(bans)]} {
						foreach b $ch(bans) {
							puts $file "- $b"
					   	}
					}
				}
				exempts {
					puts $file "::$chan exempts"
					if {[info exists ch(exempts)]} {
						foreach e $ch(exempts) {
							puts $file "- $e"
					   	}
					}
				}
				invites {
					puts $file "::$chan invites"
					if {[info exists ch(invites)]} {
						foreach i $ch(invites) {
							puts $file "- $i"
						}
					}
				}
			}
		}
	}

	close $file

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
			if {![array exists userInfo:${user}]} {
				array set userInfo:${user} ""
			}
		}

		upvar [getns]::userInfo:${user} u

		if {![info exists u(hosts)]} { continue }

		foreach host $u(hosts) {
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
