Write-Host "PHPShield Test Suite" -ForegroundColor Cyan
Write-Host "====================" -ForegroundColor Cyan
Write-Host ""

# Find PHP extension
$ext = $null
if (Test-Path "build\modules\php_phpshield.dll") {
    $ext = "build\modules\php_phpshield.dll"
} elseif (Test-Path "build\modules\phpshield.so") {
    $ext = "build\modules\phpshield.so"
} else {
    Write-Host "Error: PHPShield extension not found in build\modules\" -ForegroundColor Red
    Write-Host "Run 'cmake --build build --config Release' first" -ForegroundColor Yellow
    exit 1
}

Write-Host "Using extension: $ext" -ForegroundColor Green
Write-Host ""

# Run smoke tests
Write-Host "Running smoke tests..." -ForegroundColor Yellow
& php -d "extension=$ext" tests\smoke_test.php

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "All tests completed successfully!" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "Tests failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}
