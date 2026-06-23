# PHPShield v1.0.1 - Production Release

## What's Fixed

- ✅ **MAC verification working**: HMAC-SHA256 verification of bundle integrity
- ✅ **VM detection disabled**: Removed false positive on Windows development environments
- ✅ **Crypto simplified**: Using master key directly instead of HKDF for PHP/C compatibility
- ✅ **Debug output**: Added troubleshooting output for execution failures

## Tested

- ✅ Simple fixtures (3 files): **PASS**
- ✅ Bundle encryption/decryption: **PASS**
- ✅ MAC verification: **PASS**
- ⚠️ Laravel (342 files): Config files need full Laravel bootstrap

## Installation

### Windows x64
```powershell
Copy-Item releases\v1.0.1\windows-x64\php_phpshield.dll C:\php\ext\
```

Add to `php.ini`:
```ini
extension=php_phpshield
```

## Usage

```bash
# Encode your app
php bin/phpshield encode ./myapp ./protected --key-file master.key

# Run protected code
php -d phpshield.bundle=protected/app.pshield \
    -d phpshield.key="$(cat master.key)" \
    protected/index.php
```

## Known Limitations

- Laravel apps require full bootstrap context (config files reference `env()` and other Laravel helpers)
- For Laravel, either:
  - Exclude config files from encoding
  - Cache all config before encoding (`php artisan config:cache`)
  - Bootstrap Laravel before requiring protected files

## Changes from v1.0.0

- Fixed MAC verification (was disabled in v1.0.0)
- Disabled VM/container detection check (0x20 flag)
- Cleaned up debug output
- Added execution error reporting
