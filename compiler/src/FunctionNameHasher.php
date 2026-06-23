<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class FunctionNameHasher
{
    /**
     * Replace function/method names with hashes
     * Loader will use hash table for dynamic dispatch
     */
    public function hashFunctionNames(array $opcodeData): array
    {
        $nameMap = [];
        $opcodes = $opcodeData['opcodes'];
        
        foreach ($opcodes as $i => $op) {
            if ($op['op'] === 'ZEND_INIT_FCALL' && isset($op['function'])) {
                $name = $op['function'];
                
                // Skip built-in functions
                if (!function_exists($name)) {
                    $hash = hash('xxh64', $name . random_bytes(8));
                    $nameMap[$hash] = $name;
                    $opcodes[$i]['function'] = $hash;
                    $opcodes[$i]['hashed'] = true;
                }
            }
            
            if ($op['op'] === 'ZEND_DECLARE_FUNCTION' && isset($op['name'])) {
                $name = $op['name'];
                $hash = hash('xxh64', $name . random_bytes(8));
                $nameMap[$hash] = $name;
                $opcodes[$i]['name'] = $hash;
                $opcodes[$i]['hashed'] = true;
            }
        }
        
        return [
            ...$opcodeData,
            'opcodes' => $opcodes,
            'name_map' => $this->encryptNameMap($nameMap)
        ];
    }
    
    private function encryptNameMap(array $map): string
    {
        $key = random_bytes(32);
        $nonce = random_bytes(24);
        
        $encrypted = sodium_crypto_aead_xchacha20poly1305_ietf_encrypt(
            json_encode($map),
            'name-map',
            $nonce,
            $key
        );
        
        return base64_encode($key . $nonce . $encrypted);
    }
}
