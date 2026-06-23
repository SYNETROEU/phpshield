# PHPShield Windows x64 Installation

## Requirements
- Windows 10/11
- PHP 8.5.2 NTS (Non-Thread-Safe)
- MSVC 14.44 runtime (included with PHP 8.5.2)

## Installation

1. Copy `php_phpshield.dll` to your PHP extensions directory:
```powershell
Copy-Item php_phpshield.dll C:\php\ext\
```

2. Add to your `php.ini`:
```ini
extension=php_phpshield
```

3. Verify installation:
```powershell
php -m | findstr phpshield
```

Should output: `phpshield`

## Usage

```powershell
php -d phpshield.bundle=C:\path\to\app.pshield `
    -d phpshield.key="YOUR_BASE64_KEY" `
    index.php
```

## Troubleshooting

### "Unable to load dynamic library 'php_phpshield'"

Check PHP version:
```powershell
php -v
```

This DLL is compiled for PHP 8.5.2 NTS. If you have a different version, rebuild from source.

### "Can't load module as it's linked with X, but the core is linked with Y"

MSVC version mismatch. This DLL requires PHP compiled with MSVC 14.44.
