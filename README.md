# PHPShield

Open-source PHP 8.4 code protection. Free alternative to ionCube and SourceGuardian.

[![PHP Version](https://img.shields.io/badge/PHP-8.4%2B-blue.svg)](https://www.php.net/)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

## What it does

- XChaCha20-Poly1305 encryption with authentication
- Direct opcode execution without reconstructing source
- Bytecode optimization (constant folding, dead code elimination)
- PHP 8.4 JIT compatible
- Anti-debugging (breakpoints, timing, VM detection)
- Ed25519-signed licenses with hardware binding
- Memory-mapped bundles (zero-copy)
- Works with Laravel, Symfony, WordPress

## Quick Start

```bash
git clone https://github.com/SYNETROEU/phpshield.git
cd phpshield
composer install

# Build extension
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Test it
php -d extension=build/modules/phpshield.so -m | grep phpshield
```

### Protect your code

```bash
bin/phpshield init
bin/phpshield encode ./myapp ./protected --key-file tmp/master.key

# Run it
php -d extension=build/modules/phpshield.so \
    -d phpshield.bundle=protected/app.pshield \
    -d phpshield.key="$(cat tmp/master.key)" \
    protected/index.php
```

[Full documentation](USAGE.md)

## How it works

**Encoding:**
1. Parse source with php-parser
2. Extract opcodes and optimize bytecode
3. Encrypt with XChaCha20-Poly1305
4. Sign with HMAC-SHA256

**Runtime:**
1. Memory-map bundle (zero-copy)
2. Verify signature
3. Check license
4. Decrypt and execute opcodes

## Protection

- Polymorphic encryption (different output each build)
- Code virtualization with custom VM instructions
- String encryption (decrypted on-demand)
- Function name hashing
- Control flow obfuscation
- Anti-debugging (breakpoints, timing checks, VM detection)
- Runtime integrity checks

## Framework support

**Laravel:**
```bash
# Protect only specific folders (recommended for Laravel)
bin/phpshield encode ./laravel-app ./protected \
  --key-file master.key \
  --profile=laravel \
  --include="app/" \
  --include="routes/" \
  --include="database/"

# Or protect everything except config (alternative)
composer install --no-dev -o -a
php artisan config:cache && php artisan route:cache && php artisan view:cache
bin/phpshield encode ./app ./protected --profile=laravel --key-file master.key
```

**WordPress:**
```bash
bin/phpshield encode ./wp-content/plugins/my-plugin ./protected --key-file master.key
```

**Symfony:**
```bash
bin/phpshield encode ./symfony-app ./protected --key-file master.key --exclude="var/*"
```

## License system

Distribute your protected apps with Ed25519-signed licenses:

```bash
bin/phpshield make-license \
  --private-key license.sk \
  --out customer.pslic \
  --product-id my-app \
  --expires 2027-12-31 \
  --domains "example.com"
```

License constraints: expiration, domains, IPs, hardware binding, PHP version.

## Project license

Apache 2.0. Use PHPShield to protect commercial or open-source projects. No royalties.

## Requirements

- PHP 8.4 with `php-config`
- CMake 3.20+
- C compiler
- libsodium or OpenSSL headers
- PHP `ext-sodium` or `ext-openssl`

On Windows, use `-DPHP_ROOT=C:\path\to\php` instead of `php-config`. See [docs/windows.md](docs/windows.md) and [docs/usage.md](docs/usage.md) for platform-specific setup.

## Build With CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
php -d extension=build/modules/phpshield.so -m | grep phpshield
```

On Windows CMake builds produce `build/modules/php_phpshield.dll`:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPHP_ROOT=C:\php
cmake --build build --config Release
php -d extension=build/modules/php_phpshield.dll -m | findstr phpshield
```

Useful options:

```bash
-DPHPSHIELD_ENABLE_SODIUM=ON
-DPHPSHIELD_ENABLE_OPENSSL=ON
-DPHPSHIELD_STRICT_HARDENING=ON
-DPHPSHIELD_BUILD_TESTS=ON
```

## phpize Fallback

```bash
cd loader
phpize
./configure
make
php -d extension=modules/phpshield.so -m | grep phpshield
```

## Encode A Project

```bash
bin/phpshield init
bin/phpshield encode tests/fixtures/simple tmp/simple --key-file tmp/master.key
php -d extension=build/modules/phpshield.so \
    -d phpshield.bundle=tmp/simple/app.pshield \
    -d phpshield.key="$(cat tmp/master.key)" \
    tmp/simple/index.php
```

Signed license example:

```bash
bin/phpshield encode ./app ./build \
  --key-file ./master.key \
  --require-license \
  --product-id=my-product \
  --license-public-key=./license.ed25519.pub

bin/phpshield make-license \
  --private-key ./license.ed25519.sk \
  --out ./license.pslic \
  --product-id=my-product
```

Protected PHP files are replaced with real stubs:

```php
<?php
return phpshield_load(__FILE__);
```

## Laravel

Recommended deployment preparation:

```bash
composer install --no-dev -o -a
php artisan config:cache
php artisan route:cache
php artisan view:cache
php artisan event:cache
bin/phpshield encode ./my-laravel-app ./build --profile=laravel --key-file ./master.key
```

Runtime example:

```bash
php -d extension=build/modules/phpshield.so \
    -d phpshield.bundle=./build/app.pshield \
    -d phpshield.license=./license.pslic \
    -d phpshield.key="$(cat ./master.key)" \
    ./build/artisan about
```
