# Hardening

Release CMake builds feature-detect:

- `-fvisibility=hidden`
- `-fstack-protector-strong`
- `_FORTIFY_SOURCE=2`
- `-O2`
- `-Wl,-z,relro`
- `-Wl,-z,now`

Symbols are stripped after build when `strip` is available. Unsupported flags do not hard-fail unless strict hardening needs a production crypto backend and none is available.

Loader hardening TODOs:

- Build ID and self-hash checks
- Anti-debugging probes
- Loader control-flow hardening
- Obfuscated internal symbols
- Signed release packages
- Per-platform notarization or code signing

Do not ship debug builds to customers. Keep master keys outside stubs and outside bundles.
