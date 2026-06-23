# Threat Model

Protected assets:

- PHP source logic
- Symbol names where safe to obfuscate
- License constraints
- Bundle integrity
- Runtime payload confidentiality before execution

Attackers considered:

- Users modifying bundles or stubs
- Users with wrong keys
- Users with expired or mismatched licenses
- Offline tampering attempts

Attackers not fully stoppable:

- Root/admin users debugging the PHP process
- Kernel-level instrumentation
- Compromised production hosts
- Attackers extracting memory after successful decryption

Mitigations include AEAD payload encryption, manifest MACs, strict stub checks, license validation, hidden symbols, stack protection, RELRO where supported, no plaintext-on-disk, and minimal debug leakage by default.
