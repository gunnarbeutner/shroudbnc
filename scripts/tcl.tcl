bind pub n&- % stcl

proc stcl {nick host hand chan arg} {
	set ctx [getctx]
	catch {eval $arg} result
	setctx $ctx

	if {$result == ""} { set result "<null>" }
	foreach sline [split $result \n] {
		putserv "PRIVMSG $chan :( $arg ) = $sline"
	}
}