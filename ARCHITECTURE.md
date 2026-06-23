# PHPShield Technical Architecture

This document explains how PHPShield works internally.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                   PHP Application                       │
└────────────┬────────────────────────────────────────────┘
             │
             │ 1. require stub file
             ▼
┌─────────────────────────────────────────────────────────┐
│              phpshield_load(__FILE__)                   │
│                  (PHP Extension)                        │
└────────────┬────────────────────────────────────────────┘
             │
             │ 2. Load & verify bundle
             ▼
┌─────────────────────────────────────────────────────────┐
│              Bundle Loader (bundle.c)                   │
│  • mmap bundle file (zero-copy)                        │
│  • Verify HMAC signature                               │
│  • Check license constraints                           │
└────────────┬────────────────────────────────────────────┘
             │
             │ 3. Decrypt segment
             ▼
┌─────────────────────────────────────────────────────────┐
│          Crypto Layer (crypto.c)                        │
│  • Derive segment key with HKDF                        │
│  • Decrypt with XChaCha20-Poly1305                     │
│  • Verify authenticity                                 │
└────────────┬────────────────────────────────────────────┘
             │
             │ 4. Execute opcodes
             ▼
┌─────────────────────────────────────────────────────────┐
│        Execution Engine (execute.c)                     │
│  • Strategy: transitional / opcode / custom_ir         │
│  • JIT-aware optimization                              │
│  • Runtime integrity verification                       │
└────────────┬────────────────────────────────────────────┘
             │
             │ 5. Return result
             ▼
┌─────────────────────────────────────────────────────────┐
│                 PHP Application                         │
└─────────────────────────────────────────────────────────┘
```

---

## Encoding Pipeline

### Phase 1: Source Analysis

```php
// 1. Parse PHP source with php-parser
$parser = (new ParserFactory)->createForHostVersion();
$ast = $parser->parse($source);

// 2. Traverse AST and capture opcodes
$opcodes = $opcodeCapture->capture($source);
```

### Phase 2: Optimization

```php
// 3. Bytecode optimization
$optimizer = new BytecodeOptimizer();
$opcodes = $optimizer->optimize($opcodes);
// - Constant folding: 2 + 3 → 5
// - Dead code elimination
// - Peephole optimization

// 4. JIT-aware optimization
$jitOptimizer = new JitOptimizer();
$opcodes = $jitOptimizer->optimizeForJit($opcodes);
// - Mark hot loops
// - Monomorphize call sites
```

### Phase 3: Security Hardening

```php
// 5. Add integrity checksums
$verifier = new IntegrityVerifier();
$opcodes = $verifier->addChecksums($opcodes);

// 6. Encrypt string literals
$stringMap = $this->encryptStrings($source);
```

### Phase 4: Bundle Creation

```php
// 7. Create manifest
$manifest = [
    'version' => 2,
    'php_version_min' => '8.4.0',
    'files' => [...],
    'requires_license' => true,
    'product_id' => 'my-app',
];

// 8. Encrypt segments
foreach ($files as $file) {
    $segmentKey = HKDF($masterKey, $file->path);
    $encrypted = XChaCha20_Poly1305::encrypt($file->ir, $segmentKey);
    $bundle->addSegment($encrypted);
}

// 9. Sign bundle
$mac = HMAC_SHA256($manifest . $segments, $masterKey);
$bundle->write($mac);
```

---

## Bundle File Format

```
┌──────────────────────────────────────────────────┐
│ Magic: "PSHIELD2" (8 bytes)                      │
├──────────────────────────────────────────────────┤
│ Version: uint32_t (4 bytes)                      │
├──────────────────────────────────────────────────┤
│ Manifest Length: uint32_t (4 bytes)              │
├──────────────────────────────────────────────────┤
│ Manifest (JSON, encrypted)                       │
│  • version                                       │
│  • php_version_min                               │
│  • requires_license                              │
│  • product_id                                    │
│  • files: [{path, offset, length}]              │
├──────────────────────────────────────────────────┤
│ Manifest HMAC-SHA256 (32 bytes)                  │
├──────────────────────────────────────────────────┤
│ Segment 1 (encrypted IR)                         │
│  • Nonce (24 bytes)                              │
│  • Ciphertext (variable)                         │
│  • Auth Tag (16 bytes)                           │
├──────────────────────────────────────────────────┤
│ Segment 2 (encrypted IR)                         │
│  ...                                             │
├──────────────────────────────────────────────────┤
│ Bundle HMAC-SHA256 (32 bytes)                    │
└──────────────────────────────────────────────────┘
```

---

## Cryptographic Design

### Key Derivation

```
Master Key (256-bit random)
    │
    ├─ HKDF-SHA256(info="manifest")
    │  └─> Manifest Encryption Key
    │
    ├─ HKDF-SHA256(info="segment:" || file_path)
    │  └─> Segment Key 1, 2, 3...
    │
    └─ HKDF-SHA256(info="hmac")
       └─> HMAC Key
```

### Encryption Flow

```c
// Per-segment encryption
uint8_t nonce[24];
randombytes_buf(nonce, sizeof(nonce));

uint8_t segment_key[32];
hkdf_sha256(segment_key, master_key, file_path, "segment");

uint8_t ciphertext[plaintext_len];
uint8_t tag[16];

crypto_aead_xchacha20poly1305_ietf_encrypt(
    ciphertext, &ciphertext_len,
    plaintext, plaintext_len,
    file_path, file_path_len,  // additional authenticated data
    NULL, nonce, segment_key
);
```

### Why XChaCha20-Poly1305?

1. **Authenticated Encryption**: Prevents tampering
2. **Large Nonce Space**: 192-bit nonce (vs AES-GCM's 96-bit)
3. **Performance**: Faster than AES on non-hardware-accelerated CPUs
4. **Security**: No known practical attacks
5. **Constant-Time**: Resistant to timing attacks

---

## Opcode Execution

### Traditional vs PHPShield

**Traditional PHP:**
```
Source Code → Parser → AST → Compiler → Opcodes → Executor
```

**ionCube/SourceGuardian:**
```
Encrypted → Decrypt → Source Code → Parser → AST → Compiler → Opcodes → Executor
```

**PHPShield Transitional:**
```
Bundle → Verify → Decrypt → Source Code → Compiler → Opcodes → Executor
```

**PHPShield Opcode (Recommended):**
```
Bundle → Verify → Decrypt → Opcodes → Executor
```

### Opcode Format

```json
{
  "format": "phpshield_opcodes_v3_ast",
  "opcodes": [
    {"op": "ZEND_ECHO", "pc": 0, "line": 5},
    {"op": "ZEND_INIT_FCALL", "pc": 1, "function": "strlen", "num_args": 1},
    {"op": "ZEND_SEND_VAL", "pc": 2, "literal_id": 0},
    {"op": "ZEND_DO_FCALL", "pc": 3},
    {"op": "ZEND_RETURN", "pc": 4, "has_value": false}
  ],
  "literals": [
    {"type": "string", "value": "Hello, World!"}
  ],
  "variables": {
    "name": 0,
    "age": 1
  },
  "integrity": {
    "checksums": ["a3f2c1...", "b8e4d9..."],
    "block_size": 16,
    "algorithm": "xxhash64"
  }
}
```

### Execution in C

```c
static int phpshield_execute_opcodes(zval *return_value, const char *ir_json) {
    json_object *root = json_tokener_parse(ir_json);
    json_object *opcodes_arr = json_object_object_get(root, "opcodes");
    
    // Verify integrity
    if (!phpshield_verify_integrity(ir_json)) {
        return FAILURE;
    }
    
    // Execute each opcode
    for (int i = 0; i < json_object_array_length(opcodes_arr); i++) {
        json_object *op = json_object_array_get_idx(opcodes_arr, i);
        const char *opname = json_string(op, "op");
        
        if (strcmp(opname, "ZEND_ECHO") == 0) {
            // ... execute echo
        } else if (strcmp(opname, "ZEND_RETURN") == 0) {
            // ... execute return
        }
        // ... more opcodes
    }
    
    return SUCCESS;
}
```

---

## Bytecode Optimization

### Constant Folding

**Before:**
```php
$result = 2 + 3 * 4;
```

**Opcodes (unoptimized):**
```
ZEND_INIT_LONG 2
ZEND_INIT_LONG 3
ZEND_INIT_LONG 4
ZEND_MUL
ZEND_ADD
```

**Opcodes (optimized):**
```
ZEND_INIT_LONG 14  // Computed at compile time
```

### Dead Code Elimination

**Before:**
```php
function test() {
    return 42;
    echo "unreachable";  // Dead code
}
```

**After:**
```php
function test() {
    return 42;
}
```

### Peephole Optimization

**Pattern 1: Redundant assignment**
```
ZEND_ASSIGN $a, 5
ZEND_FETCH_R $a      // Immediately fetch what we just assigned
→ Optimize to single ASSIGN
```

**Pattern 2: Jump to next instruction**
```
ZEND_JMP pc+1        // Jump to next instruction anyway
→ Remove
```

---

## JIT-Aware Optimization

### Hot Loop Detection

```php
for ($i = 0; $i < 1000000; $i++) {
    // Hot loop
}
```

PHPShield marks this loop header for PHP 8.4 JIT:

```json
{
  "op": "ZEND_JMP",
  "pc": 5,
  "jump_target": 2,
  "loop_header": true  // Hint for JIT
}
```

### Call Site Monomorphization

**Polymorphic (bad for JIT):**
```php
function process($handler) {
    $handler->handle();  // Could be any class
}
```

**Monomorphic (good for JIT):**
```php
if ($handler instanceof SpecificHandler) {
    $handler->handle();  // JIT knows exact type
}
```

PHPShield marks frequently-called functions as "hot" to help JIT:

```json
{
  "op": "ZEND_INIT_FCALL",
  "function": "strlen",
  "hot": true  // Called 100+ times
}
```

---

## Anti-Debugging

### Detection Methods

**1. Hardware Breakpoints (x86/x64)**
```c
CONTEXT ctx;
GetThreadContext(GetCurrentThread(), &ctx);
if (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3) {
    // Debugger detected
}
```

**2. Timing Attacks**
```c
uint64_t start = rdtsc();
volatile int x = 0;
for (int i = 0; i < 100; i++) x++;
uint64_t elapsed = rdtsc() - start;

if (elapsed > 50000) {
    // Suspiciously slow = debugger
}
```

**3. Process Tracing (Linux)**
```c
FILE *fp = fopen("/proc/self/status", "r");
// Check TracerPid != 0
```

**4. VM/Container Detection**
```c
// Docker
access("/.dockerenv", F_OK)

// VirtualBox
RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
    "SYSTEM\\CurrentControlSet\\Services\\VBoxGuest", ...)
```

### Response Strategy

When debugger detected:
1. Return FAILURE (execution stops)
2. Optionally log event
3. **Never** execute protected code under debugger

---

## License Enforcement

### License File Format

```json
{
  "version": 1,
  "product_id": "my-app",
  "licensee": "Customer Name",
  "email": "customer@example.com",
  "issued": "2026-01-01T00:00:00Z",
  "expires": "2027-12-31T23:59:59Z",
  "constraints": {
    "domains": ["example.com", "*.example.com"],
    "ip": ["192.168.1.0/24"],
    "mac": ["00:11:22:33:44:55"],
    "max_servers": 5
  },
  "signature": "ed25519:base64..."
}
```

### Verification Process

```c
int phpshield_verify_license(const char *license_json, const char *pubkey) {
    // 1. Parse license JSON
    json_object *lic = json_tokener_parse(license_json);
    
    // 2. Check expiration
    time_t expires = parse_iso8601(json_string(lic, "expires"));
    if (time(NULL) > expires) return FAILURE;
    
    // 3. Check domain
    const char *server_name = getenv("SERVER_NAME");
    if (!match_domain_pattern(server_name, allowed_domains)) return FAILURE;
    
    // 4. Check hardware binding
    if (!match_mac_address(current_mac, license_mac)) return FAILURE;
    
    // 5. Verify Ed25519 signature
    unsigned char sig[64];
    base64_decode(sig, json_string(lic, "signature"));
    
    unsigned char msg[4096];
    create_signing_message(msg, lic);  // Canonical JSON
    
    if (crypto_sign_verify_detached(sig, msg, strlen(msg), pubkey) != 0) {
        return FAILURE;
    }
    
    return SUCCESS;
}
```

---

## Performance Characteristics

### Memory Footprint

| Component | Memory Usage |
|-----------|--------------|
| Loader extension | ~50 KB |
| Bundle metadata | ~10 KB per 100 files |
| Cached opcodes | ~2x source size |
| Runtime overhead | ~2 MB |

### Execution Overhead

| Strategy | First Load | Cached | vs Plain PHP |
|----------|------------|--------|--------------|
| Transitional | 80-200ms | 50-100ms | 30-50% slower |
| Opcode | 20-50ms | <1ms | 10-15% slower |
| Custom IR | 10-30ms | <1ms | 5-10% slower |

### Optimization Impact

| Optimization | Performance Gain |
|--------------|------------------|
| Constant folding | 2-5% |
| Dead code elimination | 1-3% |
| Peephole | 1-2% |
| JIT-aware | 5-10% (with JIT enabled) |
| mmap loading | 30-40% memory reduction |

---

## Security Model

### Threat Model

**Protects Against:**
- ✅ Casual source code viewing
- ✅ Automated decompilation tools
- ✅ Memory dumps of running process
- ✅ License sharing/piracy
- ✅ Tampering with encrypted bundles

**Does NOT Protect Against:**
- ❌ Determined attacker with unlimited time
- ❌ Hardware-level debugging (JTAG, etc.)
- ❌ Kernel-level memory inspection
- ❌ Side-channel attacks on cryptographic keys

### Defense in Depth

1. **Encryption Layer**: XChaCha20-Poly1305 AEAD
2. **Integrity Layer**: HMAC-SHA256 signatures
3. **Anti-Debug Layer**: Multiple detection mechanisms
4. **License Layer**: Ed25519-signed constraints
5. **Runtime Layer**: Integrity checksums on executing code

---

## Comparison with Competitors

| Feature | PHPShield | ionCube | Technique |
|---------|-----------|---------|-----------|
| Encryption | XChaCha20 | AES-256-CBC | Stream cipher vs block cipher |
| Authentication | Poly1305 | Custom MAC | AEAD vs separate MAC |
| Key derivation | HKDF | Proprietary | Standard vs custom |
| Opcode execution | AST-based | Proprietary | Modern vs legacy |
| Optimization | Yes (3 passes) | No | Compile-time optimization |
| JIT-aware | Yes | No | PHP 8.4 specific |

---

## Future Improvements

### Planned Features

1. **Hardware-Assisted Encryption**
   - Use AES-NI instructions when available
   - Intel SGX enclaves for key storage

2. **Network-Based License Validation**
   - Phone-home license checks
   - Floating license support

3. **Advanced Code Obfuscation**
   - String table encryption
   - Function pointer indirection
   - Control flow flattening

4. **Multi-Version Support**
   - PHP 8.3, 8.5 compatibility
   - Backward compatibility mode

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.

## License

MIT License - see [LICENSE](LICENSE)
