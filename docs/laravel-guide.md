# Laravel Protection Guide

## Quick Start

### 1. Protect only your application code (recommended)

This approach protects your business logic while leaving framework files unencrypted:

```bash
php bin/phpshield encode ./laravel-app ./protected \
  --key-file master.key \
  --profile=laravel \
  --include="app/" \
  --include="routes/" \
  --include="database/"
```

**What gets protected:**
- `app/` - Your models, controllers, services, commands
- `routes/` - Your route definitions
- `database/` - Migrations and seeders

**What stays unencrypted:**
- `config/` - Avoids issues with `env()` calls
- `bootstrap/` - Framework bootstrap files
- `vendor/` - Composer dependencies

### 2. Copy necessary files

```bash
# Copy unprotected files to the protected directory
cp -r ./laravel-app/vendor ./protected/
cp -r ./laravel-app/config ./protected/
cp -r ./laravel-app/bootstrap ./protected/
cp -r ./laravel-app/storage ./protected/
cp -r ./laravel-app/public ./protected/
cp ./laravel-app/.env ./protected/
cp ./laravel-app/artisan ./protected/
cp ./laravel-app/composer.json ./protected/
```

### 3. Run your protected Laravel app

```bash
php -d phpshield.bundle=./protected/app.pshield \
    -d phpshield.key="$(cat master.key)" \
    ./protected/artisan about
```

## Custom Includes/Excludes

### Include specific files or patterns

```bash
--include="app/Http/Controllers/"     # Only controllers
--include="app/Models/"                # Only models
--include="routes/api.php"             # Single file
--include="app/Services/*.php"         # Wildcard pattern
```

### Exclude specific patterns

```bash
--exclude="app/Console/Commands/DevCommand.php"  # Exclude dev commands
--exclude="tests/"                                # Exclude tests
--exclude="*.md"                                  # Exclude markdown
```

### Multiple includes work as OR

If you specify:
```bash
--include="app/" --include="routes/"
```

Any file matching either `app/` OR `routes/` will be included.

### Excludes apply after includes

```bash
--include="app/" --exclude="app/Console/"
```

This protects everything in `app/` EXCEPT `app/Console/`.

## Production Deployment

### Option 1: Runtime key (development/staging)

```bash
php -d phpshield.bundle=/var/www/app.pshield \
    -d phpshield.key="YOUR_MASTER_KEY" \
    artisan serve
```

### Option 2: License file (production)

1. Generate license:
```bash
bin/phpshield make-license \
  --private-key license.ed25519.sk \
  --out customer.pslic \
  --product-id my-app \
  --expires 2027-12-31 \
  --domains "example.com"
```

2. Encode with license requirement:
```bash
bin/phpshield encode ./app ./protected \
  --key-file master.key \
  --require-license \
  --product-id my-app \
  --license-public-key="$(cat license.ed25519.pub)"
```

3. Run with license:
```bash
php -d phpshield.bundle=/var/www/app.pshield \
    -d phpshield.license=/var/www/customer.pslic \
    -d phpshield.key="YOUR_MASTER_KEY" \
    artisan serve
```

## Web Server Configuration

### Apache + PHP-FPM

Add to your pool config (`/etc/php/8.5/fpm/pool.d/www.conf`):

```ini
php_admin_value[extension] = phpshield.so
php_admin_value[phpshield.bundle] = /var/www/myapp/app.pshield
php_admin_value[phpshield.key] = "YOUR_BASE64_KEY"
```

### Nginx + PHP-FPM

```nginx
location ~ \.php$ {
    fastcgi_param PHP_VALUE "phpshield.bundle=/var/www/myapp/app.pshield\nphpshield.key=YOUR_BASE64_KEY";
    # ... rest of fastcgi config
}
```

## Troubleshooting

### Error: "Call to undefined function env()"

**Cause:** Config files are protected and reference Laravel's `env()` helper.

**Solution:** Use `--include` to only protect `app/`, `routes/`, `database/`:
```bash
--include="app/" --include="routes/" --include="database/"
```

### Error: "PHPShield execution failed"

**Debug mode:**
```bash
php -d phpshield.debug=1 \
    -d phpshield.bundle=./protected/app.pshield \
    -d phpshield.key="$(cat master.key)" \
    artisan about
```

Look for specific error messages in the output.

### Performance

PHPShield uses zero-copy memory mapping for bundles. First load is ~10ms, subsequent loads are cached. For production, ensure:

1. OPcache is enabled
2. Bundle file has proper permissions (readable by www-data)
3. Consider using `--execution-strategy=zend_protected_opcode` for better performance

## File Counts

Full Laravel 13 app protection:
- All files: ~342 files
- With `--include="app/" --include="routes/" --include="database/"`: ~276 files
- Recommended: Protect only business logic, save ~20% overhead

## Security Notes

- Master key should be stored securely (environment variable, secrets manager)
- License files are signed with Ed25519, cannot be tampered
- Bundle integrity verified with HMAC-SHA256 on every load
- Anti-tamper checks detect debuggers and VM modifications
