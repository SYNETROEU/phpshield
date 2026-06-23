<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class Crypto
{
    public const VERSION = '0.1.0';
    private const SALT = 'phpshield-v1';

    public static function newMasterKey(): string
    {
        return rtrim(strtr(base64_encode(random_bytes(32)), '+/', '-_'), '=') . PHP_EOL;
    }

    public static function masterKeyFromFile(string $path): string
    {
        $raw = trim((string)file_get_contents($path));
        $bin = base64_decode(strtr($raw, '-_', '+/'), true);
        if ($bin === false || strlen($bin) < 32) {
            throw new \RuntimeException('key file must contain a base64url encoded 256-bit master key');
        }
        return substr($bin, 0, 32);
    }

    public static function deriveKeys(string $masterKey): array
    {
        return [
            'payload' => hash_hkdf('sha256', $masterKey, 32, 'payload-encryption', self::SALT),
            'manifest' => hash_hkdf('sha256', $masterKey, 32, 'manifest-mac', self::SALT),
            'license' => hash_hkdf('sha256', $masterKey, 32, 'license-binding', self::SALT),
            'cache' => hash_hkdf('sha256', $masterKey, 32, 'cache-key', self::SALT),
        ];
    }

    public static function deriveSegmentKey(string $payloadKey, string $path): string
    {
        return hash_hmac('sha256', "segment-key\0" . $path, $payloadKey, true);
    }

    public static function encryptSegment(string $plaintext, string $key, string $aad): array
    {
        if (function_exists('sodium_crypto_aead_xchacha20poly1305_ietf_encrypt')) {
            $nonce = random_bytes(SODIUM_CRYPTO_AEAD_XCHACHA20POLY1305_IETF_NPUBBYTES);
            $combined = sodium_crypto_aead_xchacha20poly1305_ietf_encrypt($plaintext, $aad, $nonce, $key);
            return ['alg' => 'XCHACHA20-POLY1305', 'nonce' => $nonce, 'tag' => '', 'ciphertext' => $combined];
        }
        if (function_exists('openssl_encrypt')) {
            $nonce = random_bytes(12);
            $tag = '';
            $ciphertext = openssl_encrypt($plaintext, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, $nonce, $tag, $aad, 16);
            if ($ciphertext === false || strlen($tag) !== 16) {
                throw new \RuntimeException('OpenSSL AES-256-GCM encryption failed');
            }
            return ['alg' => 'AES-256-GCM', 'nonce' => $nonce, 'tag' => $tag, 'ciphertext' => $ciphertext];
        }
        throw new \RuntimeException('no authenticated encryption backend available; install ext-sodium or ext-openssl');
    }

    public static function manifestMac(string $manifestJson, string $manifestKey): string
    {
        return hash_hmac('sha256', $manifestJson, $manifestKey, true);
    }

    public static function hash(string $bytes): string
    {
        return hash('sha256', $bytes);
    }

    public static function zero(string &$bytes): void
    {
        if (function_exists('sodium_memzero')) {
            sodium_memzero($bytes);
            return;
        }
        $bytes = str_repeat("\0", strlen($bytes));
    }
}
