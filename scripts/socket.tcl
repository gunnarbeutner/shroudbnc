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

proc listen {port type {mask ""} {flag ""} {ssl 0} {bindip ""}} {
	return [internallisten $port $type $mask $flag $ssl $bindip]
}

proc connect {host port {ssl 0}} {
	return [internalconnect $host $port $ssl]
}

proc putdcc {idx text} {
	internalsocketwriteln $idx $text
}

proc killdcc {idx} {
	internalclosesocket $idx
}

proc valididx {idx} {
	return [internalvalidsocket $idx]
}
