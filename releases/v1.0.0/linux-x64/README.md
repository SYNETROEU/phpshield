# PHPShield Linux x64 Installation

## Requirements
- Linux x86_64
- PHP 8.3+ with development headers
- libsodium or OpenSSL

## Installation

1. Copy `phpshield.so` to your PHP extensions directory:
```bash
PHP_VERSION=$(php -r 'echo PHP_MAJOR_VERSION.".".PHP_MINOR_VERSION;')
sudo cp phpshield.so /usr/lib/php/$PHP_VERSION/
```

2. Add to your `php.ini`:
```bash
echo "extension=phpshield.so" | sudo tee /etc/php/$PHP_VERSION/mods-available/phpshield.ini
sudo phpenmod phpshield
```

Or for CLI only:
```bash
echo "extension=phpshield.so" | sudo tee -a /etc/php/$PHP_VERSION/cli/php.ini
```

3. Verify installation:
```bash
php -m | grep phpshield
```

Should output: `phpshield`

## Usage

```bash
php -d phpshield.bundle=/var/www/app.pshield \
    -d phpshield.key="YOUR_BASE64_KEY" \
    index.php
```

## Web Server Configuration

### Apache + PHP-FPM

Add to pool config (`/etc/php/8.3/fpm/pool.d/www.conf`):
```ini
php_admin_value[extension] = phpshield.so
php_admin_value[phpshield.bundle] = /var/www/myapp/app.pshield
php_admin_value[phpshield.key] = "YOUR_BASE64_KEY"
```

Restart PHP-FPM:
```bash
sudo systemctl restart php8.3-fpm
```

### Nginx + PHP-FPM

```nginx
location ~ \.php$ {
    fastcgi_param PHP_VALUE "phpshield.bundle=/var/www/myapp/app.pshield\nphpshield.key=YOUR_BASE64_KEY";
    # ... rest of fastcgi config
}
```

## Troubleshooting

### "undefined symbol: crypto_aead_xchacha20poly1305_ietf_encrypt"

Install libsodium:
```bash
sudo apt-get install libsodium-dev
```

Then rebuild from source.

### Permission denied

Ensure the bundle file is readable by the web server user:
```bash
sudo chown www-data:www-data /var/www/app.pshield
sudo chmod 644 /var/www/app.pshield
```
