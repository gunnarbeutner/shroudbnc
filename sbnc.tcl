# This is an example configuration file for shroudBNC's TCL module
# It will be sourced whenever you (re-)load the tcl module or
# use the tcl command 'rehash' (e.g. /sbnc tcl :rehash)

# You should not modify this block
source "scripts/namespace.tcl"
source "scripts/timers.tcl"
source "scripts/misc.tcl"
source "scripts/variables.tcl"
source "scripts/channel.tcl"
source "scripts/pushmode.tcl"
source "scripts/bind.tcl"
source "scripts/usys.tcl"
source "scripts/socket.tcl"
source "scripts/botnet.tcl"
source "scripts/stubs.tcl"

# Load some useful procs
source "scripts/alltools.tcl"

#setctx "example"
#source "scripts/tcl.tcl"

#set ::account354 "example"
#source "scripts/account.tcl"

#set ::versionreply "example"
#source "scripts/version.tcl"

#source "scripts/partyline.tcl"
