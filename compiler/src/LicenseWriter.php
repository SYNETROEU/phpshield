<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class LicenseWriter
{
    public function __construct(private string $privateKeyPath) {}

    public function write(string $out, array $claims): void
    {
        $claims['license_version'] = 1;
        $claims['issued_at'] = $claims['issued_at'] ?? gmdate(DATE_ATOM);
        ksort($claims);
        $payload = json_encode($claims, JSON_THROW_ON_ERROR | JSON_UNESCAPED_SLASHES);
        $sigAlg = 'HMAC-SHA256-DEVELOPMENT';
        $signature = hash_hmac('sha256', $payload, $this->loadSigningMaterial(), true);
        if (function_exists('sodium_crypto_sign_detached') && is_file($this->privateKeyPath)) {
            $key = file_get_contents($this->privateKeyPath);
            if ($key !== false && strlen($key) === SODIUM_CRYPTO_SIGN_SECRETKEYBYTES) {
                $sigAlg = 'Ed25519';
                $signature = sodium_crypto_sign_detached($payload, $key);
                $public = sodium_crypto_sign_publickey_from_secretkey($key);
                if (!is_file($this->privateKeyPath . '.pub')) {
                    file_put_contents($this->privateKeyPath . '.pub', $public);
                }
            }
        }
        $license = [
            'alg' => $sigAlg,
            'payload' => base64_encode($payload),
            'signature' => base64_encode($signature),
            'public_key_sha256' => ($sigAlg === 'Ed25519' && isset($public)) ? Crypto::hash($public) : null,
        ];
        $dir = dirname($out);
        if ($dir !== '' && !is_dir($dir) && !mkdir($dir, 0775, true)) {
            throw new \RuntimeException("failed to create {$dir}");
        }
        file_put_contents($out, json_encode($license, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES) . PHP_EOL);
    }

    private function loadSigningMaterial(): string
    {
        if (is_file($this->privateKeyPath)) {
            try {
                $master = Crypto::masterKeyFromFile($this->privateKeyPath);
                return Crypto::deriveKeys($master)['license'];
            } catch (\Throwable) {
                return (string)file_get_contents($this->privateKeyPath);
            }
        }
        return $this->privateKeyPath;
    }
}
