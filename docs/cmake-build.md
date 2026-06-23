# CMake Build

PHPShield uses CMake 3.20 or newer as the primary native loader build system.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
php -d extension=build/modules/phpshield.so -m | grep phpshield
```

Windows:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPHP_ROOT=C:\php
cmake --build build --config Release
php -d extension=build/modules/php_phpshield.dll -m | findstr phpshield
```

Windows builds do not require `php-config`. Pass `-DPHP_ROOT=C:\path\to\php-sdk-or-install`; CMake looks for PHP headers and `php8.lib` or `php8ts.lib`. Windows builds link `iphlpapi`, `advapi32`, and `ws2_32` for adapter enumeration, registry machine GUID checks, and hostname support.

If sodium or OpenSSL headers are installed outside default search paths, pass:

```powershell
-DSODIUM_ROOT=C:\path\to\libsodium
-DOPENSSL_ROOT_DIR=C:\path\to\openssl
```

Options:

```bash
-DPHPSHIELD_ENABLE_SODIUM=ON
-DPHPSHIELD_ENABLE_OPENSSL=ON
-DPHPSHIELD_STRICT_HARDENING=ON
-DPHPSHIELD_BUILD_TESTS=ON
```

PHP detection uses `php` and `php-config`. If `php-config` is missing, install the PHP development package matching the runtime PHP version.

If libsodium headers are missing, CMake prints a warning and XChaCha20-Poly1305 bundles will not load. If OpenSSL headers are also missing and strict hardening is enabled, configuration fails. PHPShield never silently downgrades to weak crypto.
