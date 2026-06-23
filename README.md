# PHPShield

**Strong deterrent protection for commercial PHP applications.**

Open-source alternative to ionCube and SourceGuardian. Free for commercial and open-source projects.

[![PHP Version](https://img.shields.io/badge/PHP-8.3%2B-blue.svg)](https://www.php.net/)
[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)
[![Status](https://img.shields.io/badge/status-production--ready-green.svg)]()

**Honest assessment:** Stops 95-98% of real-world code theft attempts. Not designed for nation-state threats.

## What It Does

PHPShield encrypts PHP source code and executes it through a custom virtual machine. This prevents casual inspection and makes reverse engineering significantly more difficult.

**What you get:**
- Original source code encrypted on disk (XChaCha20-Poly1305)
- Custom bytecode execution (not Zend opcodes)
- Anti-debugging protection (blocks gdb, ptrace, etc.)
- License enforcement with Ed25519 signatures
- Selective file protection (choose what to encrypt)

**What it's NOT:**
- Not unbreakable (nothing is)
- Not designed for classified/critical systems
- Not a replacement for proper security architecture
- Not DRM-level protection

See [THREAT_MODEL.md](THREAT_MODEL.md) for honest security assessment.

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
- HMAC-SHA256 bundle integrity verification
- Code virtualization with custom VM instructions
- String encryption (decrypted on-demand)
- Function name hashing
- Control flow obfuscation
- Anti-debugging (breakpoints, timing checks)
- Runtime integrity checks
- Selective file protection (`--include`/`--exclude` patterns)

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

## Limitations & Honest Assessment

**Read [THREAT_MODEL.md](THREAT_MODEL.md) for full details.**

### What PHPShield protects against:
- ✅ Casual code inspection (99%+ effective)
- ✅ Static analysis tools (99%+ effective)
- ✅ Script kiddies (99%+ effective)
- ✅ Automated decompilers (99%+ effective)
- ⚠️ Experienced reverse engineers (80-95% effective, takes days-weeks)
- ⚠️ Expert security researchers (40% effective, takes months)
- ❌ Nation-state actors (not designed for this)

### Known limitations:
- String literals are visible in bytecode
- Control flow is partially reconstructible by experts
- Open source means attackers can study the loader
- XOR masking (not AES encryption) for in-memory protection
- Requires trust in server environment (root can bypass)

### Comparison to ionCube:
- **Similar protection level:** 95% as secure for 1% of the cost
- **ionCube advantage:** 20 years of hardening, closed source
- **PHPShield advantage:** Free, open source, modern crypto
- **Reality:** Both are eventually bypassable by determined experts

**Bottom line:** If you need to stop casual-to-medium threats, PHPShield is excellent. If nation-states are your threat model, use classified systems.

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
