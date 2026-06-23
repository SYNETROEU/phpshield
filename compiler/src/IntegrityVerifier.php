<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class IntegrityVerifier
{
    public function addChecksums(array $opcodeData): array
    {
        $opcodes = $opcodeData['opcodes'];
        $checksums = [];
        
        // Compute checksums for opcode blocks
        $blockSize = 16;
        for ($i = 0; $i < count($opcodes); $i += $blockSize) {
            $block = array_slice($opcodes, $i, $blockSize);
            $checksums[] = $this->computeBlockChecksum($block);
        }
        
        return [
            ...$opcodeData,
            'integrity' => [
                'checksums' => $checksums,
                'block_size' => $blockSize,
                'algorithm' => 'xxhash64'
            ]
        ];
    }
    
    private function computeBlockChecksum(array $block): string
    {
        $data = json_encode($block);
        return hash('xxh64', $data);
    }
    
    public function generateVerificationCode(): string
    {
        // C code to verify integrity at runtime
        return <<<'C'
static int phpshield_verify_integrity(const char *opcodes, size_t len, const char **checksums, size_t num_checksums) {
    size_t block_size = len / num_checksums;
    for (size_t i = 0; i < num_checksums; i++) {
        uint64_t hash = xxhash64(opcodes + (i * block_size), block_size);
        char computed[17];
        snprintf(computed, sizeof(computed), "%016llx", (unsigned long long)hash);
        if (memcmp(computed, checksums[i], 16) != 0) return 0;
    }
    return 1;
}
C;
    }
}
