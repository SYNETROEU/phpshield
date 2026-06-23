<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

use RecursiveDirectoryIterator;
use RecursiveIteratorIterator;

final class BundleWriter
{
    private const MAGIC = 'PHPSHIELD1';
    private const HEADER_LEN = 10 + 4 + 4 + 32;

    public function __construct(private string $masterKey) {}

    public function encodeProject(string $sourceDir, string $outputDir, string $bundleName, PathPolicy $policy, array $options): array
    {
        $sourceDir = rtrim(realpath($sourceDir) ?: $sourceDir, DIRECTORY_SEPARATOR);
        if (!is_dir($sourceDir)) {
            throw new \RuntimeException("source directory not found: {$sourceDir}");
        }
        self::removeTree($outputDir);
        if (!mkdir($outputDir, 0775, true) && !is_dir($outputDir)) {
            throw new \RuntimeException("failed to create output directory: {$outputDir}");
        }

        $keys = Crypto::deriveKeys($this->masterKey);
        $parser = new Parser();
        $analyzer = new Analyzer();
        $symbols = new SymbolTable();
        $obfuscator = new Obfuscator();
        $nameObfuscator = new NameObfuscator();
        $cfObfuscator = new ControlFlowObfuscator();
        $ir = new IrBuilder((string)($options['execution_strategy'] ?? 'zend_transitional_compile_string'));
        $warnings = [];
        $segments = [];
        $payloadBlob = '';
        $protected = 0;

        $it = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($sourceDir, \FilesystemIterator::SKIP_DOTS));
        foreach ($it as $file) {
            if (!$file->isFile()) {
                continue;
            }
            $abs = $file->getPathname();
            $rel = str_replace('\\', '/', substr($abs, strlen($sourceDir) + 1));
            if ($policy->shouldExclude($rel)) {
                continue;
            }
            $dest = rtrim($outputDir, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . str_replace('/', DIRECTORY_SEPARATOR, $rel);
            $dir = dirname($dest);
            if (!is_dir($dir) && !mkdir($dir, 0775, true)) {
                throw new \RuntimeException("failed to create {$dir}");
            }
            if (strtolower(pathinfo($rel, PATHINFO_EXTENSION)) !== 'php') {
                copy($abs, $dest);
                continue;
            }
            $source = (string)file_get_contents($abs);
            $parsed = $parser->parse($rel, $source);
            $analysis = $analyzer->analyze($rel, $source);
            array_push($warnings, ...$analysis['warnings']);
            
            // Apply obfuscation layers
            $normalized = $obfuscator->renameSafePrivateLocals($source);
            
            if (!empty($options['obfuscate_names'])) {
                $normalized = $nameObfuscator->obfuscate($normalized, $symbols->collect($parsed['tokens']));
            }
            
            if (!empty($options['obfuscate_control_flow'])) {
                $normalized = $cfObfuscator->obfuscate($normalized);
            }
            
            $irPayload = $ir->build($rel, $normalized, $symbols->collect($parsed['tokens']));
            $segmentKey = Crypto::deriveSegmentKey($keys['payload'], $rel);
            $enc = Crypto::encryptSegment($irPayload, $segmentKey, $rel);
            Crypto::zero($segmentKey);
            $offset = strlen($payloadBlob);
            $payloadBlob .= $enc['ciphertext'];
            $segments[] = [
                'path' => $rel,
                'strategy' => 'zend_transitional_compile_string',
                'alg' => $enc['alg'],
                'key_derivation' => 'payload-hmac-sha256:path:v2',
                'offset' => $offset,
                'length' => strlen($enc['ciphertext']),
                'nonce' => base64_encode($enc['nonce']),
                'tag' => base64_encode($enc['tag']),
                'ciphertext_sha256' => Crypto::hash($enc['ciphertext']),
                'stub_sha256' => Crypto::hash("<?php\nreturn phpshield_load(__FILE__);\n"),
                'source_sha256' => Crypto::hash($normalized),
            ];
            file_put_contents($dest, "<?php\nreturn phpshield_load(__FILE__);\n");
            $protected++;
        }

        $license = [
            'required' => (bool)$options['require_license'],
            'product_id' => $options['product_id'],
            'signature_algorithms' => ['Ed25519'],
        ];
        if (!empty($options['license_public_key'])) {
            $pub = (string)file_get_contents((string)$options['license_public_key']);
            if (strlen($pub) !== (defined('SODIUM_CRYPTO_SIGN_PUBLICKEYBYTES') ? SODIUM_CRYPTO_SIGN_PUBLICKEYBYTES : 32)) {
                throw new \RuntimeException('license public key must be a raw Ed25519 public key');
            }
            $license['ed25519_public_key'] = base64_encode($pub);
            $license['ed25519_public_key_sha256'] = Crypto::hash($pub);
        } elseif ((bool)$options['require_license']) {
            if (!empty($options['production'])) {
                throw new \RuntimeException('production bundles require --license-public-key with a raw Ed25519 public key');
            }
            $license['signature_algorithms'][] = 'HMAC-SHA256-DEVELOPMENT';
        }

        $manifest = [
            'format_version' => 1,
            'php_target_version' => '8.4',
            'encoder_version' => Crypto::VERSION,
            'created_at' => gmdate(DATE_ATOM),
            'flags' => ['strict' => true, 'cache' => true],
            'security' => [
                'profile' => !empty($options['production']) ? 'production' : 'development',
                'segment_key_derivation' => 'payload-hmac-sha256:path:v2',
                'license_dev_hmac_allowed' => !((bool)$options['require_license']) || empty($options['license_public_key']),
            ],
            'profile' => $options['profile'],
            'license' => $license,
            'segments' => $segments,
        ];
        $manifestJson = json_encode($manifest, JSON_THROW_ON_ERROR | JSON_UNESCAPED_SLASHES);
        $mac = Crypto::manifestMac($manifestJson, $keys['manifest']);
        $bundle = self::MAGIC . pack('N', 1) . pack('N', strlen($manifestJson)) . $mac . $manifestJson . $payloadBlob;
        $bundlePath = rtrim($outputDir, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $bundleName;
        file_put_contents($bundlePath, $bundle);
        Crypto::zero($keys['payload']);
        Crypto::zero($keys['manifest']);
        return ['bundle' => $bundlePath, 'protected' => $protected, 'warnings' => $warnings];
    }

    public static function inspect(string $bundlePath, bool $verifyShape = false): array
    {
        $bytes = (string)file_get_contents($bundlePath);
        if (strlen($bytes) < self::HEADER_LEN || substr($bytes, 0, 10) !== self::MAGIC) {
            throw new \RuntimeException('not a phpshield bundle');
        }
        $version = unpack('N', substr($bytes, 10, 4))[1];
        $manifestLength = unpack('N', substr($bytes, 14, 4))[1];
        $manifestJson = substr($bytes, self::HEADER_LEN, $manifestLength);
        $manifest = json_decode($manifestJson, true, 512, JSON_THROW_ON_ERROR);
        return [
            'ok' => true,
            'verified_shape' => $verifyShape,
            'format_version' => $version,
            'manifest_length' => $manifestLength,
            'segments' => count($manifest['segments'] ?? []),
            'license_required' => $manifest['license']['required'] ?? null,
            'php_target_version' => $manifest['php_target_version'] ?? null,
        ];
    }

    public static function removeTree(string $path): void
    {
        if (!file_exists($path)) {
            return;
        }
        if (is_file($path) || is_link($path)) {
            unlink($path);
            return;
        }
        $it = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($path, \FilesystemIterator::SKIP_DOTS), RecursiveIteratorIterator::CHILD_FIRST);
        foreach ($it as $file) {
            $file->isDir() ? rmdir($file->getPathname()) : unlink($file->getPathname());
        }
        rmdir($path);
    }
}
