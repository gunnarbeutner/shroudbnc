package require bnc 0.2

internalbind server my_evil_proc

proc my_evil_proc {client parameters} {
	set xyz [split [lindex $parameters 3]]

	if {[lindex $parameters 1] == 475 && [string equal -nocase [lindex $parameters 3] "#illuminati"]} {
		putserv "JOIN #illuminati fnords"
	}

	if {[lindex $parameters 1] == 475 && [string equal -nocase [lindex $parameters 3] "#ee.intern"]} {
		putserv "JOIN #ee.intern feedback"
	}
}
