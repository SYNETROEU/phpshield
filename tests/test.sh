#!/bin/bash
set -e

echo "PHPShield Test Suite"
echo "===================="
echo ""

# Find PHP extension
if [ -f "build/modules/phpshield.so" ]; then
    EXT="build/modules/phpshield.so"
elif [ -f "build/modules/php_phpshield.dll" ]; then
    EXT="build/modules/php_phpshield.dll"
else
    echo "Error: PHPShield extension not found in build/modules/"
    echo "Run 'cmake --build build' first"
    exit 1
fi

echo "Using extension: $EXT"
echo ""

# Run smoke tests
echo "Running smoke tests..."
php -d extension="$EXT" tests/smoke_test.php

echo ""
echo "All tests completed successfully!"
