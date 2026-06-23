<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class StringPoolEncryption
{
    private array $encryptedStrings = [];
    
    /**
     * Extract all string literals and encrypt them separately
     * Runtime will decrypt on-demand to prevent static analysis
     */
    public function encryptStringPool(array $opcodeData): array
    {
        $stringPool = [];
        $opcodes = $opcodeData['opcodes'];
        
        // Extract all string literals
        foreach ($opcodes as $i => $op) {
            if (isset($op['literal_id'])) {
                $litId = $op['literal_id'];
                if (isset($opcodeData['literals'][$litId]) && 
                    $opcodeData['literals'][$litId]['type'] === 'string') {
                    
                    $plaintext = $opcodeData['literals'][$litId]['value'];
                    $key = random_bytes(32);
                    $nonce = random_bytes(24);
                    
                    $encrypted = sodium_crypto_aead_xchacha20poly1305_ietf_encrypt(
                        $plaintext,
                        (string)$litId, // AD
                        $nonce,
                        $key
                    );
                    
                    $stringPool[$litId] = [
                        'encrypted' => base64_encode($encrypted),
                        'nonce' => base64_encode($nonce),
                        'key' => base64_encode($key)
                    ];
                    
                    // Remove plaintext
                    unset($opcodeData['literals'][$litId]);
                }
            }
        }
        
        return [
            ...$opcodeData,
            'string_pool' => $stringPool,
            'pool_encrypted' => true
        ];
    }
}
