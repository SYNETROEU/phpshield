# Roadmap

## Milestone 1

- PHP encoder CLI
- Authenticated encrypted `.pshield` bundles
- Native PHP loader extension
- CMake primary build and phpize fallback
- Path-preserving stubs
- Transitional in-memory Zend execution
- License model and first runtime enforcement checks
- Ed25519 public-key license verification tied to bundle metadata
- Process-memory decrypted IR cache after manifest verification

## Milestone 2

- Stronger machine fingerprint adapters
- Manifest signatures in addition to MACs
- Cache eviction policies and per-SAPI memory pressure handling

## Milestone 3

- Zend opcode capture per supported PHP version
- Protected `op_array` storage
- Loader-level execution without source text reconstruction
- Opcache integration

## Milestone 4

- macOS, Windows, FreeBSD, ARM64 build/test matrix
- PHP 8.5 compatibility branch
- Loader hardening and packaging pipeline
