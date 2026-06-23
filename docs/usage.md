# PHPShield Usage Guide

This guide covers the hardened Windows-first workflow for building the loader,
encoding a PHP project, issuing a license, and running protected code.

## 1. Build the Windows Loader

Open PowerShell in the project root and build through the Visual Studio
developer environment:

```powershell
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"" -arch=x64 -host_arch=x64 && cmake -S . -B .\build -DCMAKE_BUILD_TYPE=Release -DPHP_ROOT=C:\PHP\php-8.5.7-devel-vs17-x64-nts -DSODIUM_ROOT=C:\PHP\libsodium-1.0.22-RELEASE"
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"" -arch=x64 -host_arch=x64 && cmake --build .\build --config Release"
```

The loader DLL is written to:

```text
build\modules\php_phpshield.dll
```

For shipped customer builds, enable production mode:

```powershell
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"" -arch=x64 -host_arch=x64 && cmake -S . -B .\build-production -DCMAKE_BUILD_TYPE=Release -DPHPSHIELD_PRODUCTION=ON -DPHP_ROOT=C:\PHP\php-8.5.7-devel-vs17-x64-nts -DSODIUM_ROOT=C:\PHP\libsodium-1.0.22-RELEASE"
cmd /c "call ""C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"" -arch=x64 -host_arch=x64 && cmake --build .\build-production --config Release"
```

Production loader builds reject development HMAC licenses and require Ed25519
licenses for protected products.

## 2. Initialize Keys

```powershell
php .\bin\phpshield init
```

This creates:

```text
tmp\master.key
tmp\license.ed25519.sk
tmp\license.ed25519.pub
```

Keep `tmp\master.key` and `tmp\license.ed25519.sk` private. Do not ship them
with protected applications.

## 3. Encode a Project

Development bundle:

```powershell
php .\bin\phpshield encode .\tests\fixtures\simple .\tmp\protected --key-file .\tmp\master.key
```

Production bundle with license enforcement:

```powershell
php .\bin\phpshield encode .\tests\fixtures\simple .\tmp\protected `
  --key-file .\tmp\master.key `
  --require-license `
  --production `
  --product-id my-product `
  --license-public-key .\tmp\license.ed25519.pub
```

The encoder writes protected PHP stubs and a bundle:

```text
tmp\protected\app.pshield
```

Each protected segment uses a per-file key derived from the bundle payload key
and the manifest path. The manifest authenticates segment offsets, algorithms,
hashes, license policy, and key-derivation metadata.

## 4. Make a License

```powershell
php .\bin\phpshield make-license `
  --private-key .\tmp\license.ed25519.sk `
  --out .\tmp\license.pslic `
  --product-id my-product `
  --customer-id customer-001 `
  --expires-at 2027-01-01T00:00:00Z
```

Optional binding fields:

```powershell
--domains example.com,www.example.com
--ips 203.0.113.10
--mac-addresses 00:11:22:33:44:55
--machine-fingerprints MACHINE-GUID-OR-MACHINE-ID
--allowed-sapis cli,fpm-fcgi,apache2handler
--min-php-version 8.4.0
--max-php-version 8.5.99
```

Windows machine fingerprints use:

```text
HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid
```

Linux machine fingerprints use `/etc/machine-id` or
`/var/lib/dbus/machine-id`.

## 5. Run Protected Code

Development or unlicensed bundle:

```powershell
$key = Get-Content .\tmp\master.key
$bundle = (Resolve-Path .\tmp\protected\app.pshield).Path
php -d extension=.\build\modules\php_phpshield.dll `
  -d phpshield.bundle=$bundle `
  -d phpshield.key=$key `
  .\tmp\protected\index.php
```

Licensed production bundle:

```powershell
$key = Get-Content .\tmp\master.key
$bundle = (Resolve-Path .\tmp\protected\app.pshield).Path
php -d extension=.\build-production\modules\php_phpshield.dll `
  -d phpshield.bundle=$bundle `
  -d phpshield.license=.\tmp\license.pslic `
  -d phpshield.key=$key `
  .\tmp\protected\index.php
```

Recommended runtime settings:

```ini
extension=php_phpshield.dll
phpshield.bundle=C:\path\to\app.pshield
phpshield.license=C:\path\to\license.pslic
phpshield.key=base64url-master-key
phpshield.debug=0
phpshield.strict=1
phpshield.cache=1
```

## 6. Windows GUI

Start the GUI from PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\windows-gui\phpshield-gui.ps1
```

The GUI wraps the same CLI commands:

- `Encode` selects source, output, master key, product ID, public license key,
  and production mode.
- `License` creates Ed25519 licenses with customer, expiry, domain, and machine
  bindings.
- `Tools` initializes keys and inspects bundles.

Use the GUI for operator convenience, but keep automated builds on the CLI so
release commands are repeatable.

## 7. Security Notes

- Production bundles should always use `--require-license --production` and a
  raw Ed25519 public key.
- Production loader builds should use `-DPHPSHIELD_PRODUCTION=ON`.
- Do not ship `master.key` or Ed25519 secret keys with customer artifacts.
- Keep `phpshield.debug=0` in production.
- Sign the final `php_phpshield.dll` before distributing it.
- Match PHP minor version, thread-safety, architecture, and MSVC toolset between
  the loader and the PHP binary that loads it.
- This milestone still uses transitional Zend compilation after native
  verification and decryption. The next major hardening step is replacing
  recoverable PHP-source execution with protected opcode or custom IR execution.
