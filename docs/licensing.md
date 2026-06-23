# Licensing

Licenses are JSON envelopes with base64 payload and signature fields. The payload contains:

- `license_version`
- `customer_id`
- `product_id`
- `not_before`
- `expires_at`
- `features`
- `min_php_version`
- `max_php_version`
- `allowed_sapis`
- `domains`
- `ips`
- `offline`

The loader checks product ID, dates, PHP version, and SAPI. Domain, IP, MAC address, and machine fingerprint checks are modeled and reserved for stronger platform adapters.

When libsodium signing keys are available, license files use Ed25519. The trusted raw Ed25519 public key is embedded in the authenticated bundle manifest with `--license-public-key`, and the loader verifies the license signature against that key. The license file may expose a public-key hash for diagnostics, but the loader does not trust a public key supplied by the license itself.

Development HMAC licenses are only accepted for bundles that do not embed an Ed25519 license public key. They are useful for local smoke tests and are not a commercial signing model.

Machine binding fields:

- `mac_addresses`
- `machine_fingerprints`

On Linux, MAC addresses are read from `/sys/class/net/*/address` and machine fingerprints from `/etc/machine-id` or `/var/lib/dbus/machine-id`. On Windows, MAC addresses are read with `GetAdaptersAddresses` and the machine fingerprint is `HKLM\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid`.

Online activation and revocation are intentionally not called in v1. They should be added as explicit opt-in features.
