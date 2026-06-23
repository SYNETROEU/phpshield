<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class CodeVirtualizer
{
    /**
     * Converts opcodes to custom VM instructions
     * Makes static analysis extremely difficult
     */
    public function virtualize(array $opcodes): array
    {
        $vmInstructions = [];
        $registerMap = [];
        
        foreach ($opcodes as $op) {
            // Map Zend opcodes to custom VM opcodes with random instruction IDs
            $vmOp = $this->mapToVmInstruction($op);
            
            // Add decoy instructions
            if (rand(0, 3) === 0) {
                $vmInstructions[] = $this->generateDecoyInstruction();
            }
            
            $vmInstructions[] = $vmOp;
        }
        
        return [
            'vm_version' => 2,
            'instructions' => $vmInstructions,
            'register_map' => $registerMap
        ];
    }
    
    private function mapToVmInstruction(array $zendOp): array
    {
        // Use random instruction IDs that change per build
        static $opMap = null;
        if ($opMap === null) {
            $opMap = $this->generateRandomOpMap();
        }
        
        return [
            'id' => $opMap[$zendOp['op']] ?? rand(0x1000, 0xFFFF),
            'args' => $this->encodeArgs($zendOp),
            'checksum' => crc32(json_encode($zendOp))
        ];
    }
    
    private function generateRandomOpMap(): array
    {
        $ops = ['ZEND_ECHO', 'ZEND_RETURN', 'ZEND_ASSIGN', 'ZEND_ADD', 'ZEND_MUL'];
        $map = [];
        foreach ($ops as $op) {
            $map[$op] = rand(0x1000, 0xFFFF);
        }
        return $map;
    }
    
    private function generateDecoyInstruction(): array
    {
        return [
            'id' => rand(0x1000, 0xFFFF),
            'args' => [rand(), rand()],
            'decoy' => true
        ];
    }
    
    private function encodeArgs(array $op): array
    {
        // XOR encode arguments with random key
        $key = rand(0, 255);
        $encoded = [];
        foreach ($op as $k => $v) {
            if (is_int($v)) {
                $encoded[$k] = $v ^ $key;
            }
        }
        $encoded['_key'] = $key;
        return $encoded;
    }
}
