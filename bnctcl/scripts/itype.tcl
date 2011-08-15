# shroudBNC - an object-oriented framework for IRC
# Copyright (C) 2005-2011 Gunnar Beutner
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

proc itype_parse {value} {
	set chars [split $value ""]

	set escape 0
	set type ""
	set offset 0
	set data [list]
	set codeCount 0

	foreach char $chars {
		incr offset

		if {$escape} {
			set wasEscape 1
		} else {
			set wasEscape 0
		}

		if {$char == "\\" && !$escape} {
			set escape 1
		} else {
			set escape 0
		}

		if {!$wasEscape} {
			set controlCode 1

			switch -- $char {
				"\[" {
					if {$type == ""} {
						set type "exception"
					}

					set controlCode 0

					if {$type == "exception"} {
						incr codeCount
					}
				}

				"\{" {
					if {$type == ""} {
						set type "list"
					}

					set controlCode 0

					if {$type == "list"} {
						incr codeCount
					}
				}

				"(" {
					if {$type == ""} {
						set type "string"
					}

					set controlCode 0

					if {$type == "string"} {
						incr codeCount
					}
				}

				"\]" {
					if {$type != "exception"} {
						set controlCode 0
					} else {
						incr codeCount -1
					}
				}

				"\}" {
					if {$type != "list"} {
						set controlCode 0
					} else {
						incr codeCount -1
					}
				}

				")" {
					if {$type != "string"} {
						set controlCode 0
					} else {
						incr codeCount -1
					}
				}

				default {
					set controlCode 0
				}
			}
		} else {
			if {$char == "n"} {
				set char "\n"
			} elseif {$char == "r"} {
				set char "\r"
			}
		}

		if {$type == "list" && $wasEscape} {
			lappend data "\\"
		}

		if {$type != "" && !$escape} {
			lappend data $char
		}

		if {!$wasEscape && $controlCode && $codeCount == 0} {
			switch -- $type {
				"string" {
					return [list "string" [join [lrange $data 1 end-1] ""] $offset]
				}

				"exception" {
					return [list "exception" [join [lrange $data 1 end-1] ""] $offset]
				}

				"list" {
					set dataString [join [lrange $data 1 end-1] ""] 
					set listData [list]

					while {[string length $dataString] > 0} {
						set innerData [itype_parse $dataString]

						if {[lindex $innerData 0] == "empty"} {
							break
						}

						set dataString [string range $dataString [lindex $innerData 2] end]

						lappend listData $innerData
					}

					return [list "list" $listData $offset]
				}
			}
		}
	}

	return [list "empty" "" $offset]
}

proc itype_flat {value} {
	set type [lindex $value 0]

	if {$type == "list"} {
		set listItems [list]

		foreach item [lindex $value 1] {
			lappend listItems [itype_flat $item]
		}

		return $listItems
	} else {
		return [itype_unescape [lindex $value 1]]
	}
}

proc itype_escape {value} {
	set output $value

	set output [string map [list "\\" "\\\\"] $output]

	set output [string map [list "\r" "\\r"] $output]
	set output [string map [list "\n" "\\n"] $output]

	set output [string map [list "\{" "\\\{"] $output]
	set output [string map [list "\}" "\\\}"] $output]

	set output [string map [list "\[" "\\\["] $output]
	set output [string map [list "\]" "\\\]" ] $output]

	set output [string map [list "(" "\\("] $output]
	set output [string map [list ")" "\\)"] $output]

	return $output
}

proc itype_unescape {value} {
	set output $value

	set output [string map [list "\\r" "\r"] $output]
	set output [string map [list "\\n" "\n"] $output]

	set output [string map [list "\\\{" "\{"] $output]
	set output [string map [list "\\\}" "\}"] $output]

	set output [string map [list "\\\[" "\["] $output]
	set output [string map [list "\\\]" "\]"] $output]

	set output [string map [list "\\(" "("] $output]
	set output [string map [list "\\)" ")"] $output]

	set output [string map [list "\\\\" "\\"] $output]

	return $output
}

proc itype_totcl {value} {
	set parsed [itype_parse $value]
	return [itype_flat $parsed]
}

proc itype_string {value} {
	return "([itype_escape $value])"
}

proc itype_exception {value} {
	return "\[[itype_escape $value]\]"
}

proc itype_list_create {} {
	return "\{"
}

proc itype_list_insert {listName itype_value} {
	upvar 1 $listName list

	append list $itype_value
}

proc itype_list_end {listName} {
	upvar 1 $listName list

	append list "\}"
}

proc itype_list_strings {strings} {
	set itype_list [itype_list_create]

	foreach arg $strings {
		itype_list_insert itype_list [itype_string $arg]
	}

	itype_list_end itype_list

	return $itype_list
}

proc itype_list_strings_args {args} {
	return [itype_list_strings $args]
}
