<?php

include_once('sbnc.php');

$sbnc = new SBNC("192.168.5.6", 9000, "test", "******");

if (!isset($_REQUEST['invoke'])) {
	$result = $sbnc->Call("commands");

	if (IsError($result)) {
		die(GetCode($result));
	}

	$commands = GetResult($result);

	natsort($commands);

	$params_calls = array();

	foreach ($commands as $command) {
		if ($command == "") { break; }

		array_push( $params_calls, array( 'params', array( $command ) ) );
	}

	$result = $sbnc->Call("multicall", array( $params_calls ) );

	if (IsError($result)) {
		die(GetCode($result));
	}

	$i = 0;

	foreach ($commands as $command) {
		if (IsError($result[$i])) {
			die(GetCode($result[$i]));
		}

		echo '<a href="index.php?invoke=' . $command . '">' . $command . '</a>' . ' ' . implode(' ', $result[$i]) . '<br>' . "\n";

		$i++;
	}
} else if (isset($_REQUEST['run'])) {
	$params = array();

	$i = 0;
	while (true) {
		if (isset($_REQUEST['param' . $i])) {
			array_push($params, $_REQUEST['param' . $i]);
		} else {
			break;
		}

		$i++;
	}

	if (isset($_REQUEST['user']) && $_REQUEST['user'] != '') {
		$user = $_REQUEST['user'];
	} else {
		$user = FALSE;
	}

	$result = $sbnc->CallAs($user, $_REQUEST['invoke'], $params);

	echo 'Code: ' . GetCode($result) . '<br>';
	echo 'Return value: <pre>';
	var_dump($result);
	echo '</pre><br>';

	echo '<br><br><a href="index.php">Back</a>';	
} else {
	$result = $sbnc->Call("params", array( $_REQUEST['invoke'] ));

	if (IsError($result)) {
		die(GetCode($result));
	}

	echo '<form>';
	echo 'Command: ' . $_REQUEST['invoke'] . '<br><br>';
	echo 'User: <input name="user" /><br><br>';
	echo 'Parameters:<br>';
	echo '<input type="hidden" name="invoke" value="' . $_REQUEST['invoke'] . '" />';
	echo '<input type="hidden" name="run" value="1" />';

	$i = 0;
	foreach (GetResult($result) as $param) {
		if ($param == "") { break; }
		echo $param . ' <input name="param' . $i . '" /><br>';
		$i++;
	}

	if ($i == 0) {
		echo 'No parameters can be specified for this command.';
	}

	echo '<br><input type="submit" value="Invoke">';

	echo '</form>';

	echo '<br><br><a href="index.php">Back</a>';	
}

$sbnc->Destroy();

?>
