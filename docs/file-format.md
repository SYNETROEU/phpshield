# File Format

Bundle magic:

```text
PHPSHIELD1
```

Header:

```text
magic: 10 bytes
format_version: uint32 big-endian
manifest_length: uint32 big-endian
manifest_mac: 32 bytes HMAC-SHA256 over manifest JSON
manifest: JSON
segments: encrypted binary payloads
```

Manifest fields:

- `format_version`
- `php_target_version`
- `encoder_version`
- `created_at`
- `flags`
- `profile`
- `license`
- `segments`

License fields:

- `required`
- `product_id`
- `signature_algorithms`
- `ed25519_public_key`
- `ed25519_public_key_sha256`

Segment fields:

- `path`
- `strategy`
- `alg`
- `offset`
- `length`
- `nonce`
- `tag`
- `ciphertext_sha256`
- `stub_sha256`
- `source_sha256`

The manifest MAC covers paths, offsets, nonces, tags, hashes, flags, and license requirements. The AEAD tag covers each encrypted segment and uses the relative path as additional authenticated data.
