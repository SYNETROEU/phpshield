# Windows Support

Windows support targets PHP 8.4 x64 builds through CMake. PHP extensions on Windows are DLLs, so the loader output is:

```text
build/modules/php_phpshield.dll
```

Example:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPHP_ROOT=C:\php
cmake --build build --config Release
php -d extension=build/modules/php_phpshield.dll -m | findstr phpshield
```

`PHP_ROOT` should point to a PHP SDK or install tree containing headers and the matching import library. For NTS PHP, use `php8.lib`; for TS PHP, use `php8ts.lib`. Match thread-safety, architecture, compiler runtime, and PHP minor version with the PHP binary that will load the extension. CMake rejects an obvious TS/NTS import-library mismatch.

The MSVC minor toolset must also match the PHP binary. If PHP reports a startup error such as `linked with 14.50, but the core is linked with 14.44`, install the matching Visual Studio 2022 Build Tools MSVC `14.44` toolset and regenerate the build with that compiler. A successfully compiled DLL can still be rejected by PHP when this runtime ABI does not match.

The loader calls libsodium or OpenSSL directly. PHP's `php_sodium.lib` import library is not enough by itself; install standalone libsodium development headers/libraries or OpenSSL development headers/libraries and pass `-DSODIUM_ROOT=...` or `-DOPENSSL_ROOT_DIR=...` when needed.

Runtime checks implemented on Windows:

- `IsDebuggerPresent` in strict mode
- Adapter MAC address checks through `GetAdaptersAddresses`
- Machine fingerprint checks through `HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid`
- Hostname/domain checks through Winsock `gethostname`

Recommended production packaging:

- Ship separate NTS and TS builds when needed.
- Sign `php_phpshield.dll`.
- Keep `phpshield.debug=0`.
- Do not place master keys in stubs, bundles, or source control.
