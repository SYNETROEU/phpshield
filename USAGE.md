# PHPShield User Guide

Complete documentation for protecting your PHP applications with PHPShield.

## Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Encoding Your Application](#encoding-your-application)
5. [License Management](#license-management)
6. [Deployment](#deployment)
7. [Framework-Specific Guides](#framework-specific-guides)
8. [Advanced Configuration](#advanced-configuration)
9. [Troubleshooting](#troubleshooting)

---

## Introduction

PHPShield is a modern PHP 8.4 source protection system that encrypts your PHP code into secure `.pshield` bundles. It uses authenticated encryption (XChaCha20-Poly1305) and provides opcode-level protection, bytecode optimization, and advanced anti-debugging.

### Key Features

- **Strong Encryption**: XChaCha20-Poly1305 authenticated encryption
- **Opcode Protection**: Direct bytecode execution (no source reconstruction)
- **Bytecode Optimization**: Constant folding, dead code elimination
- **JIT-Aware**: Optimized for PHP 8.4 JIT compiler
- **Anti-Debugging**: Hardware breakpoint detection, timing checks
- **License Enforcement**: Ed25519-signed licenses with hardware binding
- **Zero-Copy Loading**: Memory-mapped bundles for performance
- **Framework Support**: Laravel, Symfony, WordPress, and more

### System Requirements

- **PHP**: 8.4 or later
- **Extensions**: `ext-sodium` or `ext-openssl`
- **Platform**: Linux (x64), Windows (x64), macOS (x64/ARM64)
- **Build Tools**: CMake 3.20+ and C compiler

---

## Installation

### Step 1: Clone the Repository

```bash
git clone https://github.com/yourusername/phpshield.git
cd phpshield
```

### Step 2: Install Dependencies

```bash
composer install
```

### Step 3: Build the Loader Extension

#### Linux/macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

#### Windows

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPHP_ROOT=C:\php
cmake --build build --config Release
```

### Step 4: Verify Installation

```bash
php -d extension=build/modules/phpshield.so -m | grep phpshield
```

You should see `phpshield` in the output.

### Step 5: Install Globally (Optional)

#### Linux

```bash
sudo cp build/modules/phpshield.so $(php-config --extension-dir)/
echo "extension=phpshield.so" | sudo tee /etc/php/8.4/mods-available/phpshield.ini
sudo phpenmod phpshield
```

#### Windows

Copy `build/modules/php_phpshield.dll` to your PHP `ext/` directory and add to `php.ini`:

```ini
extension=php_phpshield
```

---

## Quick Start

### 1. Initialize Project

```bash
bin/phpshield init
```

This creates `tmp/master.key` and license keys.

### 2. Encode a Simple Project

```bash
bin/phpshield encode tests/fixtures/simple tmp/protected --key-file tmp/master.key
```

### 3. Run Protected Code

```bash
php -d extension=build/modules/phpshield.so \
    -d phpshield.bundle=tmp/protected/app.pshield \
    -d phpshield.key="$(cat tmp/master.key)" \
    tmp/protected/index.php
```

**Output:**
```
Hello, World!
```

---

## Encoding Your Application

### Basic Encoding

```bash
bin/phpshield encode /path/to/source /path/to/output \
    --key-file master.key
```

**What happens:**
1. All `.php` files are encrypted into `app.pshield` bundle
2. Original files are replaced with stub files
3. Directory structure is preserved

### With License Requirement

```bash
bin/phpshield encode ./myapp ./protected \
    --key-file master.key \
    --require-license \
    --product-id=my-product \
    --license-public-key=license.ed25519.pub
```

### With Obfuscation

```bash
bin/phpshield encode ./myapp ./protected \
    --key-file master.key \
    --obfuscate-names \
    --obfuscate-control-flow
```

**Obfuscation features:**
- `--obfuscate-names`: Renames functions/methods to `_a`, `_b`, etc.
- `--obfuscate-control-flow`: Adds dead code and opaque predicates

### Execution Strategies

PHPShield supports different execution modes:

#### 1. Transitional (Default)
```bash
bin/phpshield encode ./myapp ./protected \
    --key-file master.key \
    --execution-strategy=zend_transitional
```
- Decrypts to memory, compiles via Zend
- Good compatibility, moderate performance

#### 2. Opcode Protection (Recommended)
```bash
bin/phpshield encode ./myapp ./protected \
    --key-file master.key \
    --execution-strategy=zend_opcode
```
- Direct opcode execution
- Best security, best performance
- **Recommended for production**

#### 3. Custom IR (Experimental)
```bash
bin/phpshield encode ./myapp ./protected \
    --key-file master.key \
    --execution-strategy=custom_ir
```
- Custom bytecode VM
- Limited PHP support (simple functions only)

### Excluding Files

Create `.phpshieldignore`:

```
vendor/
tests/
*.log
cache/
storage/
```

Or use CLI:

```bash
bin/phpshield encode ./myapp ./protected \
    --key-file master.key \
    --exclude="vendor/*" \
    --exclude="tests/*"
```

---

## License Management

### Generating License Keys

```bash
# Generate Ed25519 key pair
bin/phpshield make-license-keypair \
    --private-key license.ed25519.sk \
    --public-key license.ed25519.pub
```

### Creating a License

```bash
bin/phpshield make-license \
    --private-key license.ed25519.sk \
    --out customer.pslic \
    --product-id my-product \
    --licensee "Customer Name" \
    --email customer@example.com \
    --expires 2027-12-31 \
    --domains "example.com,*.example.com" \
    --max-servers 5
```

### License Fields

| Field | Description | Required |
|-------|-------------|----------|
| `product-id` | Must match encoded bundle | Yes |
| `licensee` | Customer name | No |
| `email` | Customer email | No |
| `expires` | Expiration date (YYYY-MM-DD) | No |
| `domains` | Allowed domains (comma-separated) | No |
| `max-servers` | Maximum number of servers | No |
| `ip` | Allowed IP addresses | No |
| `mac` | Hardware binding (MAC address) | No |

### Distributing Licenses

Send `customer.pslic` to your customer. They configure it in `php.ini`:

```ini
extension=phpshield.so
phpshield.bundle=/path/to/app.pshield
phpshield.license=/path/to/customer.pslic
phpshield.key=base64-encoded-master-key
```

Or via CLI:

```bash
php -d phpshield.license=customer.pslic protected/index.php
```

---

## Deployment

### Production Configuration

#### php.ini

```ini
extension=phpshield.so

; Bundle location
phpshield.bundle=/var/www/app/app.pshield

; License file
phpshield.license=/var/www/app/license.pslic

; Master key (base64-encoded)
phpshield.key=YOUR_BASE64_ENCODED_KEY_HERE

; Enable strict mode (recommended)
phpshield.strict=1

; Disable debug mode
phpshield.debug=0
```

#### Environment Variables

```bash
export PHPSHIELD_BUNDLE=/var/www/app/app.pshield
export PHPSHIELD_LICENSE=/var/www/app/license.pslic
export PHPSHIELD_KEY=YOUR_BASE64_ENCODED_KEY_HERE
```

### Docker Deployment

**Dockerfile:**

```dockerfile
FROM php:8.4-fpm

# Install PHPShield
COPY build/modules/phpshield.so /usr/local/lib/php/extensions/no-debug-non-zts-20240731/
RUN echo "extension=phpshield.so" > /usr/local/etc/php/conf.d/phpshield.ini

# Copy protected application
COPY protected/ /var/www/html/
COPY app.pshield /var/www/html/
COPY license.pslic /var/www/html/

# Configure PHPShield
ENV PHPSHIELD_BUNDLE=/var/www/html/app.pshield
ENV PHPSHIELD_LICENSE=/var/www/html/license.pslic
ENV PHPSHIELD_KEY=YOUR_KEY_HERE
```

### CI/CD Integration

**GitHub Actions:**

```yaml
name: Build and Encode

on: [push]

jobs:
  encode:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install PHP 8.4
        uses: shivammathur/setup-php@v2
        with:
          php-version: '8.4'
          extensions: sodium
      
      - name: Install PHPShield
        run: |
          git clone https://github.com/yourusername/phpshield
          cd phpshield
          composer install
          cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build -j
      
      - name: Encode Application
        run: |
          phpshield/bin/phpshield encode ./myapp ./protected \
            --key-file ${{ secrets.PHPSHIELD_KEY }} \
            --require-license \
            --product-id my-product
      
      - name: Upload Artifact
        uses: actions/upload-artifact@v3
        with:
          name: protected-app
          path: protected/
```

---

## Framework-Specific Guides

### Laravel

#### 1. Prepare Laravel Application

```bash
composer install --no-dev -o -a
php artisan config:cache
php artisan route:cache
php artisan view:cache
php artisan event:cache
```

#### 2. Encode with Laravel Profile

```bash
bin/phpshield encode ./my-laravel-app ./protected \
    --profile=laravel \
    --key-file master.key \
    --require-license \
    --product-id my-laravel-app
```

The Laravel profile automatically excludes:
- `storage/`
- `bootstrap/cache/`
- `.env`
- Cached config/routes/views

#### 3. Deploy

```bash
php -d extension=phpshield.so \
    -d phpshield.bundle=protected/app.pshield \
    -d phpshield.license=license.pslic \
    -d phpshield.key="$(cat master.key)" \
    protected/artisan serve
```

### WordPress

```bash
# Encode WordPress plugin
bin/phpshield encode ./my-plugin ./protected-plugin \
    --key-file master.key \
    --exclude="node_modules/*" \
    --exclude="tests/*"

# Encode WordPress theme
bin/phpshield encode ./my-theme ./protected-theme \
    --key-file master.key
```

**wp-config.php:**

```php
<?php
// Load PHPShield before anything else
if (extension_loaded('phpshield')) {
    ini_set('phpshield.bundle', __DIR__ . '/wp-content/plugins/my-plugin/app.pshield');
    ini_set('phpshield.key', 'YOUR_KEY_HERE');
}
```

### Symfony

```bash
composer install --no-dev -o -a
php bin/console cache:clear --env=prod

bin/phpshield encode ./symfony-app ./protected \
    --key-file master.key \
    --exclude="var/*" \
    --exclude="public/bundles/*"
```

---

*Continued in next section...*
