# PHPShield v1.0.0 - Windows x64

## Installation

1. Copy `php_phpshield.dll` to your PHP extensions directory
2. Add to `php.ini`:
   ```ini
   extension=php_phpshield
   ```
3. Restart your web server or PHP-FPM

## Verify Installation

```cmd
php -m | findstr phpshield
```

## Requirements

- PHP 8.4+ (x64, NTS or TS)
- Windows x64
- ext-sodium or ext-openssl

## Usage

See main repository documentation: https://github.com/SYNETROEU/phpshield
