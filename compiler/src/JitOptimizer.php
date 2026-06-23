<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class JitOptimizer
{
    public function isJitAvailable(): bool
    {
        return function_exists('opcache_get_status') 
            && ($status = opcache_get_status()) 
            && isset($status['jit']['enabled']) 
            && $status['jit']['enabled'];
    }
    
    public function optimizeForJit(array $opcodeData): array
    {
        if (!$this->isJitAvailable()) {
            return $opcodeData;
        }
        
        $opcodes = $opcodeData['opcodes'];
        
        // JIT optimization: reduce polymorphic call sites
        $opcodes = $this->monomorphizeCallSites($opcodes);
        
        // JIT optimization: mark hot loops
        $opcodes = $this->markHotLoops($opcodes);
        
        return [
            ...$opcodeData,
            'opcodes' => $opcodes,
            'metadata' => [
                ...($opcodeData['metadata'] ?? []),
                'jit_optimized' => true
            ]
        ];
    }
    
    private function monomorphizeCallSites(array $opcodes): array
    {
        // Replace polymorphic calls with monomorphic versions where possible
        $callCounts = [];
        
        foreach ($opcodes as $i => $op) {
            if ($op['op'] === 'ZEND_INIT_FCALL' && isset($op['function'])) {
                $func = $op['function'];
                $callCounts[$func] = ($callCounts[$func] ?? 0) + 1;
            }
        }
        
        // Mark frequently called functions for JIT attention
        foreach ($opcodes as $i => $op) {
            if ($op['op'] === 'ZEND_INIT_FCALL' && isset($op['function'])) {
                $func = $op['function'];
                if (($callCounts[$func] ?? 0) > 5) {
                    $opcodes[$i]['hot'] = true;
                }
            }
        }
        
        return $opcodes;
    }
    
    private function markHotLoops(array $opcodes): array
    {
        // Detect loops and mark them as hot for JIT compilation
        $loopHeaders = [];
        
        foreach ($opcodes as $i => $op) {
            if (isset($op['loop_start'])) {
                $loopHeaders[$op['loop_start']] = true;
            }
            if ($op['op'] === 'ZEND_JMP' && isset($op['jump_target']) && $op['jump_target'] < $op['pc']) {
                $loopHeaders[$op['jump_target']] = true;
            }
        }
        
        foreach ($opcodes as $i => $op) {
            if (isset($loopHeaders[$op['pc']])) {
                $opcodes[$i]['loop_header'] = true;
            }
        }
        
        return $opcodes;
    }
}
