# PHPShield v1.0.0

First release of PHPShield - open-source PHP 8.4 code protection.

## Features

- XChaCha20-Poly1305 authenticated encryption
- Direct opcode execution
- Bytecode optimization (constant folding, dead code elimination)
- PHP 8.4 JIT compatible
- Anti-debugging (breakpoints, timing, VM detection)
- Ed25519-signed licenses with hardware binding
- Memory-mapped bundles (zero-copy)
- Framework support: Laravel, Symfony, WordPress

## What's Included

### Windows x64
- Pre-built `php_phpshield.dll` for PHP 8.4+
- Compatible with both NTS and TS builds

### Linux x64
- Pre-built `phpshield.so` for PHP 8.3+
- Compatible with most distributions

## Installation

See README.md in each platform folder.

## Quick Start

```bash
# Initialize keys
bin/phpshield init

# Encode project
bin/phpshield encode ./myapp ./protected --key-file tmp/master.key

# Run protected code
php -d extension=phpshield \
    -d phpshield.bundle=protected/app.pshield \
    -d phpshield.key="$(cat tmp/master.key)" \
    protected/index.php
```

## Documentation

- [Main README](https://github.com/SYNETROEU/phpshield/blob/main/README.md)
- [Usage Guide](https://github.com/SYNETROEU/phpshield/blob/main/USAGE.md)
- [Windows Setup](https://github.com/SYNETROEU/phpshield/blob/main/docs/windows.md)

## License

Apache 2.0
