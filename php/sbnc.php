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

include_once('itype.php');

class SBNC {
	var $socket, $num, $user, $pass;

	function SBNC($host, $port, $user, $pass) {
		$this->num = 0;
		$this->user = $user;
		$this->pass = $pass;
		$this->socket = @fsockopen($host, $port);

		if ($this->socket == FALSE) {
//			die('Socket could not be connected.');
			return;
		}

		fputs($this->socket, "RPC_IFACE\n");

		while (($line = @fgets($this->socket)) != FALSE) {
			if (strstr($line, "RPC_IFACE_OK") != FALSE) {
				break;
			}

			if (strstr($line, "[RPC_BLOCK]") != FALSE) {
				die('Runtime error occured in the RPC system: This IP address is blocked.');
			}
		}
	}

	function Destroy() {
		fclose($this->socket);
	}


	function CallAs($user, $command, $parameters = array()) {
		if ($user == FALSE) {
			$cmd = array($this->user, $this->pass);
		} else {
			$cmd = array($user, $this->user . ':' . $this->pass);
		}

		array_push($cmd, $command);
		array_push($cmd, $parameters);
	
		fputs($this->socket, itype_fromphp($cmd) . "\n");

		$line = fgets($this->socket, 128000);

		if ($line === false) {
			die('Transport layer error occured in the RPC system: fgets() failed.');
		}


		$line = str_replace(array("\r", "\n"), array("", ""), $line);

		$parsedResponse = itype_parse($line);

		if ($parsedResponse[0] == 'empty') {
			die('IType parsing error occured in RPC system when parsing the string "' . $line . '"');
		}

		$response = itype_flat($parsedResponse);

		if (IsError($response)) {
			$code = GetCode($response);

			if ($code != 'RPC_ERROR') {
				die('Runtime error occured in the RPC system: [' . $code . '] ' . GetResult($response));
			}
		}

		return $response;
	}

	function Call($command, $parameters = array()) {
		return $this->CallAs(FALSE, $command, $parameters);
	}

	function MultiCallAs($user, $commands) {
		$results = array();

		while (count($commands) > 0) {
			$spliced_commands = array_splice($commands, 0, 10);

			$spliced_results = $this->CallAs($user, 'multicall', array( $spliced_commands ));

			$results = array_merge($results, $spliced_results);
		}

		return $results;
	}

	function MultiCall($commands) {
		return $this->MultiCallAs(FALSE, $commands);
	}

	function IsValid() {
	    return ($this->socket != FALSE);
	}

	function Relogin($user, $pass) {
	    $this->user = $user;
	    $this->pass = $pass;
	}
}

function IsError($result) {
	if (is_a($result, 'itype_exception')) {
		return true;
	} else {
		return false;
	}
}

function GetCode($result) {
	if (IsError($result)) {
		return $result->GetCode();
	} else {
		return '';
	}
}

function GetResult($result) {
	if (IsError($result)) {
		return $result->GetMessage();
	} else {
		return $result;
	}
}

?>
