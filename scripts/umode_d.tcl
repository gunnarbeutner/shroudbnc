# automatically sets/unsets umode +d when appropriate

internalbind attach umode_minus_d
internalbind detach umode_plus_d

proc umode_minus_d {client} {
	putquick "MODE $::botnick -d"
}

proc umode_plus_d {client} {
	putquick "MODE $::botnick +d"
}
