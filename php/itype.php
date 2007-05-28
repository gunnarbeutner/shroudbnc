<?php
/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005 Gunnar Beutner                                           *
 *                                                                             *
 * This program is free software; you can redistribute it and/or               *
 * modify it under the terms of the GNU General Public License                 *
 * as published by the Free Software Foundation; either version 2              *
 * of the License, or (at your option) any later version.                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *******************************************************************************/

class itype_exception {
	var $code;
	var $message;

	function itype_exception($text) {
		$toks = explode(' ', $text, 2);

		$this->code = $toks[0];
		$this->message = $toks[1];
	}

	function GetCode() {
		return $this->code;
	}

	function GetMessage() {
		return $this->message;
	}

	function GetRawMessage() {
		return $this->code . ' ' . $this->message;
	}
}

function itype_fromphp($value) {
	if (is_array($value)) {
		$ret = '{';
		$empty = true;

		foreach ($value as $item) {
			$ret .= itype_fromphp($item);
		}

		$ret .= '}';
	} else {
		$value = str_replace(
				array("\r",	"\n",	"\\",	"{",	"}",	"[",	"]",	"(",	")"),
				array("\\\r",	"\\n",	"\\\\",	"\\{",	"\\}",	"\\[",	"\\]",	"\\(", "\\)"),
					$value);

		$ret = "({$value})";
	}

	return $ret;
}

function itype_parse($value) {
	$escape = false;
	$type = '';
	$offset = 0;
	$data = '';
	$codeCount = 0;

	$len = strlen($value);

	for ($i = 0; $i < $len; $i++) {
		$char = $value{$i};
		$offset++;

		if ($escape) {
			$wasEscape = true;
		} else {
			$wasEscape = false;
		}

		if ($char == "\\" && !$escape) {
			$escape = true;
		} else {
			$escape = false;
		}

		if (!$wasEscape) {
			$controlCode = true;

			switch ($char) {
				case '[':
					if ($type == '') {
						$type = 'exception';
					}

					$controlCode = false;

					if ($type == 'exception') {
						$codeCount++;
					}

					break;

				case '{':
					if ($type == '') {
						$type = 'list';
					}

					$controlCode = false;

					if ($type == 'list') {
						$codeCount++;
					}

					break;

				case '(':
					if ($type == '') {
						$type = 'string';
					}

					$controlCode = false;

					if ($type == 'string') {
						$codeCount++;
					}

					break;

				case ']':
					if ($type != 'exception') {
						$controlCode = false;
					} else {
						$codeCount--;
					}

					break;

				case '}':
					if ($type != 'list') {
						$controlCode = false;
					} else {
						$codeCount--;
					}

					break;

				case ')':
					if ($type != 'string') {
						$controlCode = false;
					} else {
						$codeCount--;
					}

					break;

				default:
					$controlCode = false;
			}
		} else {
			if ($char == 'n') {
				$char = "\n";
			} else if ($char == 'r') {
				$char = "\r";
			}
		}

		if ($type == 'list' && $wasEscape) {
			$data .= '\\';
		}

		if ($type != '' && !$escape) {
			$data .= $char;
		}

		if (!$wasEscape && $controlCode && $codeCount == 0) {
			switch ($type) {
				case 'string':
					return array('string', substr($data, 1, -1), $offset);
				case 'exception':
					return array('exception', new itype_exception(substr($data, 1, -1)), $offset);
				case 'list':
					$dataString = substr($data, 1, -1);
					$listData = array();

					while ($dataString != '') {
						$innerData = itype_parse($dataString);

						if ($innerData[0] == 'empty') {
							break;
						}

						$dataString = substr($dataString, $innerData[2]);

						array_push($listData, $innerData);
					}

					return array('list', $listData, $offset);
			}
		}
	}

	return array('empty', '', $offset);
}

function itype_flat($value) {
	$type = $value[0];

	if ($type == 'list') {
		$listItems = array();

		foreach ($value[1] as $item) {
			array_push($listItems, itype_flat($item));
		}

		return $listItems;
	} else if ($type == 'empty') {
		return new itype_exception("Invalid itype string encountered.");
	} else {
		if (is_a($value[1], 'itype_exception')) {
			return $value[1];
		} else {
			return str_replace(
					array("\r",	"\n",	"\\{",	"\\}",	"\\[",	"\\]",	"\\(", "\\)",	"\\\\"),
					array("\\r",	"\\n",	"{",	"}",	"[",	"]",	"(",	")",	"\\",),
						$value[1]);
		}
	}
}

function itype_tophp($value) {
	return itype_flat(itype_parse($value));
}

?>