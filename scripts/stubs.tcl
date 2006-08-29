# shroudBNC - an object-oriented framework for IRC
# Copyright (C) 2005 Gunnar Beutner
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

proc addchanrec {handle channel} { }
proc delchanrec {handle channel} { }
proc haschanrec {handle channel} { return 1 }

proc getting-users {} { return 0 }

proc channame2dname {chan} { return $chan }
proc chandname2name {chan} { return $chan }

proc onchansplit {nick {chan ""}} { return 0 }

proc dccbroadcast {message} { }
proc dccputchan {channel message} { }
proc boot {user {reason ""}} { }
proc dccsimul {idx text} { }
proc hand2idx {hand} { }
proc idx2hand {idx} { }
proc valididx {idx} { return 0 }
proc getchan {idx} { }
proc setchan {idx channel} { }
proc console {idx {chan ""} {modes ""}} { }
proc echo {idx {status ""}} { }
proc strip {idx flags} { }
proc putbot {nick message} { }
proc putallbots {message} { }
proc bots {} { }
proc botlist {} { }
proc islinked {bot} { return 0 }
proc dccused {} { return 0 }
proc dcclist {{type ""}} { }
proc whom {chan} { }
proc getdccidle {idx} { return 0 }
proc getdccaway {idx} { }
proc dccdumpfile {idx file} { }

proc maskhost {host} { return $host }

proc link {args} { }
proc unlink {bot} { }

proc chanlist {chan} { return [internalchanlist $chan] }
