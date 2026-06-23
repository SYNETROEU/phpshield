#!/usr/bin/env php
<?php
declare(strict_types=1);

function benchmark(string $name, callable $fn, int $iterations = 1000): array {
    gc_collect_cycles();
    $start = hrtime(true);
    $memStart = memory_get_usage(true);
    
    for ($i = 0; $i < $iterations; $i++) {
        $fn();
    }
    
    $end = hrtime(true);
    $memEnd = memory_get_usage(true);
    
    return [
        'name' => $name,
        'iterations' => $iterations,
        'total_time_ms' => ($end - $start) / 1_000_000,
        'avg_time_ms' => (($end - $start) / 1_000_000) / $iterations,
        'memory_delta_kb' => ($memEnd - $memStart) / 1024,
    ];
}

echo "PHPShield Performance Benchmark\n";
echo "================================\n\n";

// Benchmark 1: Plain PHP execution
$plainResult = benchmark('Plain PHP', function() {
    $x = 0;
    for ($i = 0; $i < 100; $i++) {
        $x += $i;
    }
    return $x;
}, 10000);

echo sprintf("%-30s %8.3f ms/iter  %8.2f KB\n", 
    $plainResult['name'], 
    $plainResult['avg_time_ms'],
    $plainResult['memory_delta_kb']
);

// Benchmark 2: PHPShield protected (if available)
if (function_exists('phpshield_load')) {
    // Create test file
    $testCode = '<?php $x = 0; for ($i = 0; $i < 100; $i++) $x += $i; return $x;';
    $testFile = sys_get_temp_dir() . '/phpshield_bench.php';
    file_put_contents($testFile, $testCode);
    
    // Encode it
    $outDir = sys_get_temp_dir() . '/phpshield_bench_out';
    $keyFile = sys_get_temp_dir() . '/bench.key';
    file_put_contents($keyFile, base64_encode(random_bytes(32)));
    
    exec(sprintf(
        '%s %s encode %s %s --key-file %s 2>&1',
        PHP_BINARY,
        __DIR__ . '/../bin/phpshield',
        escapeshellarg($testFile),
        escapeshellarg($outDir),
        escapeshellarg($keyFile)
    ), $output, $code);
    
    if ($code === 0 && is_file($outDir . '/phpshield_bench.php')) {
        ini_set('phpshield.bundle', $outDir . '/app.pshield');
        ini_set('phpshield.key', trim(file_get_contents($keyFile)));
        
        $protectedResult = benchmark('PHPShield Protected', function() use ($outDir) {
            return include $outDir . '/phpshield_bench.php';
        }, 1000);
        
        echo sprintf("%-30s %8.3f ms/iter  %8.2f KB\n", 
            $protectedResult['name'], 
            $protectedResult['avg_time_ms'],
            $protectedResult['memory_delta_kb']
        );
        
        $overhead = (($protectedResult['avg_time_ms'] / $plainResult['avg_time_ms']) - 1) * 100;
        echo sprintf("\nOverhead: %.1f%%\n", $overhead);
        
        // Cleanup
        array_map('unlink', glob($outDir . '/*'));
        rmdir($outDir);
        unlink($testFile);
        unlink($keyFile);
    }
} else {
    echo "PHPShield extension not loaded - skipping protected benchmarks\n";
}

// Benchmark 3: Crypto operations
if (extension_loaded('sodium')) {
    $key = random_bytes(32);
    $data = random_bytes(1024);
    
    $cryptoResult = benchmark('XChaCha20-Poly1305 Encrypt', function() use ($key, $data) {
        $nonce = random_bytes(SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NPUBBYTES);
        return sodium_crypto_aead_xchacha20poly1305_ietf_encrypt($data, '', $nonce, $key);
    }, 5000);
    
    echo sprintf("\n%-30s %8.3f ms/iter\n", 
        $cryptoResult['name'], 
        $cryptoResult['avg_time_ms']
    );
}

echo "\nBenchmark complete.\n";
