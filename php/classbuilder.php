<?php

include_once('sbnc.php');

function indentCode($lines, $tabs) {
	$lines_out = array();
	$lines_in = explode("\n", $lines);

	foreach ($lines_in as $line) {
		$lines_out[] = str_repeat("\t", $tabs) . $line;
	}

	return implode("\n", $lines_out);
}

class ClassBuilder {
	var $className, $methods, $customFunctionBody;

	function ClassBuilder($className) {
		$this->className = $className;
		$this->methods = array();
		$this->customFunctionBody = '';

		$this->addVariable('wrappedObj');
	}

	function loadClass() {
		eval($this->buildClass());
	}

	function addMethod($name, $parameterNames) {
		$this->methods[] = array( $name, $parameterNames );
	}

	function addVariable($variableName) {
		$this->variables[] = $variableName;
	}

	function setCustomFunctionBody($body) {
		$this->customFunctionBody = $body;
	}

	function buildClass() {
		$classBody = "class {$this->className} \{\n";

		foreach ($this->variables as $variable) {
			$classBody .= indentCode($this->buildVariable($variable), 1) . "\n";
		}

		$classBody .= indentCode($this->buildConstructor(), 1) . "\n";

		foreach ($this->methods as $method) {
			$classBody .= "\n" . indentCode($this->buildFunction($method[0], $method[1]), 1) . "\n";
		}

		$classBody .= "}\n";

		return $classBody;
	}

	function buildVariable($variableName) {
		$variableBody = "var \${$variableName};\n";

		return $variableBody;
	}

	function buildConstructor() {
		$constructorBody = "function {$this->className}(\$wrappedObj) {\n\t\$this->wrappedObj = \$wrappedObj;\n}";

		return $constructorBody;
	}

	function buildFunction($name, $parameters) {
		$functionBody = "function {$name}(";

		$firstParam = true;

		foreach ($parameters as $parameter) {
			if ($firstParam) {
				$firstParam = false;
			} else {
				$functionBody .= ", ";
			}

			$functionBody .= "\${$parameter}";
		}

		$functionBody .= ") {\n";
		$functionBody .= indentCode($this->buildFunctionBody($name, $parameters), 1) . "\n";
		$functionBody .= "}";

		return $functionBody;
	}

	function buildFunctionBody($name, $parameters) {
		$functionBody = "\$functionName = '{$name}';\n";
		$functionBody .= "\$parameters = array( ";

		$firstParameter = true;

		foreach ($parameters as $parameter) {
			if ($firstParameter) {
				$firstParameter = false;
			} else {
				$functionBody .= ", ";
			}

			$functionBody .= "\${$parameter}";
		}

		$functionBody .= " );\n\n";

		$functionBody .= $this->customFunctionBody;

		return $functionBody;
	}
}

function buildSbncClass($className, $sbncObject) {
	$result = $sbncObject->Call("commands");

	if (IsError($result)) {
		return false;
	}

	$commands = GetResult($result);

	$params_calls = array();

	foreach ($commands as $command) {
		array_push( $params_calls, array( 'params', array( $command ) ) );
	}

	$result = $sbncObject->Call("multicall", array( $params_calls ) );

	if (IsError($result)) {
		return false;
	}

	$builder = new ClassBuilder($className);

	$i = 0;

	foreach ($commands as $command) {
		if (IsError($result[$i])) {
			die(GetCode($result[$i]));
		}

		if ($command != 'global') {
			$builder->addMethod($command, $result[$i]);
		}

		$i++;
	}	

	$builder->setCustomFunctionBody("\$result = \$this->wrappedObj->Call(\$functionName, \$parameters);\n\nreturn \$result;");

	$builder->loadClass();

	return true;
}

?>
