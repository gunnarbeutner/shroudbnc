internalbind client sbnc:fakemode

proc sbnc:fakemode {client parameters} {
	if {[string equal -nocase "fakemode" [lindex $parameters 0]]} {
		haltoutput
		putclient ":[lindex [split $::server ":"] 0] MODE [join [lrange $parameters 1 end]]"
	}
}
