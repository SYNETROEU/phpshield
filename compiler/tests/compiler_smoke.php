<?php
declare(strict_types=1);

require __DIR__ . '/../src/Crypto.php';
require __DIR__ . '/../src/PathPolicy.php';

use PhpShield\Compiler\Crypto;
use PhpShield\Compiler\PathPolicy;

$tmpKey = tempnam(sys_get_temp_dir(), 'phpshield-key-');
file_put_contents($tmpKey, Crypto::newMasterKey());
$key = Crypto::masterKeyFromFile($tmpKey);
unlink($tmpKey);
$keys = Crypto::deriveKeys($key);
$enc = Crypto::encryptSegment('hello', $keys['payload'], 'smoke.php');
assert($enc['ciphertext'] !== 'hello');
assert(!(new PathPolicy(PathPolicy::defaultExcludes()))->shouldExclude('src/App.php'));
assert((new PathPolicy(PathPolicy::defaultExcludes()))->shouldExclude('vendor/autoload.php'));
echo "compiler smoke ok\n";
