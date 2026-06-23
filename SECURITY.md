# Security Policy

PHPShield aims to become commercial-grade, but the first milestone is not yet guaranteed equal to ionCube or SourceGuardian.

Security depends on the native loader, authenticated encryption, signed or authenticated license files, manifest MAC verification, payload AEAD verification, and never writing plaintext source to disk.

A determined attacker with root/admin access, debugger control, or the ability to instrument the PHP process can eventually extract runtime code. PHPShield's goal is to make unauthorized inspection and tampering expensive, noisy, and operationally difficult, not mathematically impossible.

Reports should include:

- Exact PHP version
- OS and architecture
- SAPI
- Loader build method and CMake options
- Crypto backend
- Bundle and license configuration
- Reproduction steps

Do not attach proprietary source bundles to public reports.
