# PHPShield v1.0.0 - Linux x64

## Installation

1. Copy `phpshield.so` to your PHP extensions directory:
   ```bash
   sudo cp phpshield.so $(php-config --extension-dir)/
   ```

2. Add to `php.ini`:
   ```ini
   extension=phpshield.so
   ```

3. Restart your web server or PHP-FPM:
   ```bash
   sudo systemctl restart php8.3-fpm
   # or
   sudo systemctl restart apache2
   ```

## Verify Installation

```bash
php -m | grep phpshield
```

## Requirements

- PHP 8.3+ (x64)
- Linux x64
- ext-sodium or ext-openssl

## Usage

See main repository documentation: https://github.com/SYNETROEU/phpshield
