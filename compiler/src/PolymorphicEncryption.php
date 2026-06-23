<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class PolymorphicEncryption
{
    /**
     * Encrypts opcodes with randomized per-segment keys and nonces
     * Makes each segment unique even with same source
     */
    public function polymorphicEncrypt(string $data, string $masterKey): array
    {
        // Generate random per-segment seed
        $seed = random_bytes(32);
        
        // Derive unique key from master + seed
        $segmentKey = hash_hkdf('sha256', $masterKey, 32, 'polymorphic-' . bin2hex($seed));
        
        // Add random padding to change ciphertext size
        $padding = random_bytes(rand(16, 128));
        $paddedData = $data . pack('C', strlen($padding)) . $padding;
        
        // Encrypt with XChaCha20-Poly1305
        $nonce = random_bytes(SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NPUBBYTES);
        $ciphertext = sodium_crypto_aead_xchacha20poly1305_ietf_encrypt(
            $paddedData,
            $seed, // Additional authenticated data
            $nonce,
            $segmentKey
        );
        
        return [
            'seed' => $seed,
            'nonce' => $nonce,
            'ciphertext' => $ciphertext
        ];
    }
}
