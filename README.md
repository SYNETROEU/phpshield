# PHPShield

PHPShield is a PHP 8.4 source protection system aimed at becoming a commercial-grade alternative to SourceGuardian and ionCube over multiple milestones. The first commit establishes the architecture and a working vertical slice: PHP encoder, authenticated encrypted `.pshield` bundles, path-preserving stubs, a native PHP loader extension, license checks, and CMake/phpize build paths.

## What This Is Not

This is not a base64 obfuscator, XOR encoder, ROT transform, encrypted-source-plus-wrapper product, or a claim of current parity with ionCube/SourceGuardian. The first milestone still uses a transitional Zend compile-string execution strategy after native verification and in-memory decryption. It is designed so later milestones can replace that strategy with protected Zend opcode or custom IR execution.

## Requirements

- PHP 8.4 with `php-config`
- CMake 3.20+
- C compiler toolchain
- libsodium development headers preferred, or OpenSSL development headers for AES-256-GCM fallback
- PHP `ext-sodium` or `ext-openssl` for the encoder

On Windows, use `-DPHP_ROOT=C:\path\to\php` instead of `php-config`; see [docs/windows.md](docs/windows.md).
For the end-to-end hardened workflow, production build flags, licensing, and the Windows GUI, see [docs/usage.md](docs/usage.md).

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

## Security Limitations

PHPShield uses authenticated encryption and does not write plaintext to disk. Modified manifests, payloads, wrong keys, and failed licenses are rejected. A determined attacker with full machine control and debugger access can eventually extract runtime code; the goal is to make cracking expensive, not impossible.

## Roadmap

The next major work is Zend opcode capture, protected `op_array` execution, opcache integration, stronger license signatures by default, platform loader builds, and PHP 8.5 compatibility.
