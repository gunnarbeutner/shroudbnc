package require bnc 0.2

internalbind server my_evil_proc
internalbind pulse my_pulse_proc

proc my_evil_proc {client parameters} {
	set xyz [split [lindex $parameters 3]]

	if {[string equal -nocase [lindex $parameters 1] "privmsg"] && [string equal -nocase $xyz "!hello"]} {
		putserv "PRIVMSG [lindex $parameters 2] :Hello world from TCL!"
	}

	if {[lindex $parameters 1] == 475 && [string equal -nocase [lindex $parameters 3] "#illuminati"]} {
		putserv "JOIN #illuminati fnords"
	}

	if {[lindex $parameters 1] == 475 && [string equal -nocase [lindex $parameters 3] "#ee.intern"]} {
		putserv "JOIN #ee.intern feedback"
	}
}

proc my_pulse_proc {time} {
	if {[expr $time % 3600] == 0} {
		setctx "chaos"
		putserv "PRIVMSG #illuminati :$time"
	}
}