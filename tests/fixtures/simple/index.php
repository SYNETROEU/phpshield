<?php
declare(strict_types=1);

$returnValue = require __DIR__ . '/lib.php';
require __DIR__ . '/Greeter.php';

echo fixture_function() . PHP_EOL;
echo (new Fixture\Greeter())->hello() . PHP_EOL;
echo $returnValue . PHP_EOL;
