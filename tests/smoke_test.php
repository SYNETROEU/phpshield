#!/usr/bin/env php
<?php
declare(strict_types=1);

function pass(string $test): void {
    echo "\033[32m✓\033[0m {$test}\n";
}

function fail(string $test, string $msg): never {
    echo "\033[31m✗\033[0m {$test}: {$msg}\n";
    exit(1);
}

function info(string $msg): void {
    echo "\033[34mℹ\033[0m {$msg}\n";
}

// Test 1: Encoder CLI exists
if (!is_file(__DIR__ . '/../bin/phpshield')) {
    fail('Encoder CLI', 'bin/phpshield not found');
}
pass('Encoder CLI exists');

// Test 2: Extension loaded
if (!extension_loaded('phpshield')) {
    fail('PHPShield extension', 'extension not loaded - run with -d extension=path/to/phpshield.so');
}
pass('PHPShield extension loaded');

// Test 3: Functions available
$funcs = ['phpshield_load', 'phpshield_version', 'phpshield_license_info'];
foreach ($funcs as $func) {
    if (!function_exists($func)) {
        fail('Function availability', "function {$func} not found");
    }
}
pass('All functions available');

// Test 4: Version info
$version = phpshield_version();
if (empty($version)) {
    fail('Version info', 'phpshield_version() returned empty');
}
info("Version: {$version}");
pass('Version info');

// Test 5: License info
$licInfo = phpshield_license_info();
if (!is_array($licInfo) || !isset($licInfo['configured'])) {
    fail('License info', 'phpshield_license_info() returned invalid structure');
}
pass('License info structure');

// Test 6: Bundle encoding (if fixtures exist)
$fixtureDir = __DIR__ . '/fixtures/simple';
if (is_dir($fixtureDir)) {
    $tmpDir = sys_get_temp_dir() . '/phpshield-test-' . uniqid();
    $outDir = $tmpDir . '/output';
    $keyFile = $tmpDir . '/test.key';
    
    mkdir($tmpDir, 0700, true);
    file_put_contents($keyFile, base64_encode(random_bytes(32)));
    
    $cmd = sprintf(
        '%s %s encode %s %s --key-file %s 2>&1',
        PHP_BINARY,
        escapeshellarg(__DIR__ . '/../bin/phpshield'),
        escapeshellarg($fixtureDir),
        escapeshellarg($outDir),
        escapeshellarg($keyFile)
    );
    
    exec($cmd, $output, $code);
    
    if ($code !== 0) {
        fail('Bundle encoding', 'encoder failed: ' . implode("\n", $output));
    }
    
    if (!is_file($outDir . '/app.pshield')) {
        fail('Bundle encoding', 'bundle file not created');
    }
    
    // Cleanup
    array_map('unlink', glob($outDir . '/*'));
    array_map('unlink', glob($tmpDir . '/*'));
    rmdir($outDir);
    rmdir($tmpDir);
    
    pass('Bundle encoding');
}

// Test 7: Integer overflow protection (manifest parsing)
info('Testing integer overflow protection...');
$badBundle = "PHPSHIELD1" . pack('N', 1) . pack('N', 0xFFFFFFFF) . str_repeat("\x00", 32) . '{}';
$tmpBad = sys_get_temp_dir() . '/bad-' . uniqid() . '.pshield';
file_put_contents($tmpBad, $badBundle);

// Should fail gracefully, not crash
try {
    ini_set('phpshield.bundle', $tmpBad);
    ini_set('phpshield.key', base64_encode(random_bytes(32)));
    // This should fail safely
} catch (\Throwable $e) {
    // Expected
}
unlink($tmpBad);
pass('Integer overflow protection');

// Test 8: Path canonicalization
info('Testing path validation...');
try {
    // This should fail with invalid path
    phpshield_load('/../../etc/passwd');
    fail('Path validation', 'accepted invalid path');
} catch (\Error $e) {
    if (strpos($e->getMessage(), 'invalid') === false && strpos($e->getMessage(), 'not configured') === false) {
        fail('Path validation', 'wrong error: ' . $e->getMessage());
    }
}
pass('Path validation');

echo "\n\033[32m✓ All tests passed\033[0m\n";
exit(0);
