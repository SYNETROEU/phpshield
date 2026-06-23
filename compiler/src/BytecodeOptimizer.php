<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class BytecodeOptimizer
{
    public function optimize(array $opcodeData): array
    {
        $opcodes = $opcodeData['opcodes'];
        $literals = $opcodeData['literals'];
        
        // Pass 1: Constant folding
        [$opcodes, $literals] = $this->foldConstants($opcodes, $literals);
        
        // Pass 2: Dead code elimination
        $opcodes = $this->eliminateDeadCode($opcodes);
        
        // Pass 3: Peephole optimization
        $opcodes = $this->peepholeOptimize($opcodes);
        
        return [
            ...$opcodeData,
            'opcodes' => $opcodes,
            'literals' => $literals,
            'metadata' => [
                ...($opcodeData['metadata'] ?? []),
                'optimized' => true
            ]
        ];
    }
    
    private function foldConstants(array $opcodes, array $literals): array
    {
        $optimized = [];
        $newLiterals = $literals;
        
        for ($i = 0; $i < count($opcodes); $i++) {
            $op = $opcodes[$i];
            
            // Detect constant binary operations like 2 + 3
            if ($this->isBinaryOp($op) && $i >= 2) {
                $left = $opcodes[$i - 2] ?? null;
                $right = $opcodes[$i - 1] ?? null;
                
                if ($this->isLiteralLoad($left) && $this->isLiteralLoad($right)) {
                    $leftVal = $literals[$left['literal_id']] ?? null;
                    $rightVal = $literals[$right['literal_id']] ?? null;
                    
                    if ($leftVal && $rightVal && $this->isNumeric($leftVal) && $this->isNumeric($rightVal)) {
                        $result = $this->evaluateBinaryOp($op['op'], $leftVal['value'], $rightVal['value']);
                        
                        if ($result !== null) {
                            // Remove last 2 opcodes, replace with folded constant
                            array_pop($optimized);
                            array_pop($optimized);
                            
                            $litId = count($newLiterals);
                            $newLiterals[$litId] = ['type' => is_float($result) ? 'double' : 'long', 'value' => $result];
                            
                            $optimized[] = [
                                'op' => is_float($result) ? 'ZEND_INIT_DOUBLE' : 'ZEND_INIT_LONG',
                                'pc' => $op['pc'],
                                'literal_id' => $litId,
                                'line' => $op['line']
                            ];
                            continue;
                        }
                    }
                }
            }
            
            $optimized[] = $op;
        }
        
        return [$optimized, $newLiterals];
    }
    
    private function eliminateDeadCode(array $opcodes): array
    {
        $reachable = array_fill(0, count($opcodes), false);
        $worklist = [0];
        
        while (!empty($worklist)) {
            $pc = array_pop($worklist);
            if ($pc >= count($opcodes) || $reachable[$pc]) continue;
            
            $reachable[$pc] = true;
            $op = $opcodes[$pc];
            
            // Mark sequential instruction
            if ($pc + 1 < count($opcodes) && !$this->isTerminator($op)) {
                $worklist[] = $pc + 1;
            }
            
            // Mark jump targets
            if (isset($op['jump_target']) && $op['jump_target'] !== null) {
                $worklist[] = $op['jump_target'];
            }
        }
        
        return array_values(array_filter($opcodes, fn($op, $i) => $reachable[$i], ARRAY_FILTER_USE_BOTH));
    }
    
    private function peepholeOptimize(array $opcodes): array
    {
        $optimized = [];
        
        for ($i = 0; $i < count($opcodes); $i++) {
            $curr = $opcodes[$i];
            $next = $opcodes[$i + 1] ?? null;
            
            // NOP elimination
            if ($curr['op'] === 'ZEND_NOP') continue;
            
            // JMP to next instruction elimination
            if ($curr['op'] === 'ZEND_JMP' && isset($curr['jump_target']) && $curr['jump_target'] === $curr['pc'] + 1) {
                continue;
            }
            
            // ASSIGN followed by FETCH of same variable → optimize to direct use
            if ($curr['op'] === 'ZEND_ASSIGN' && $next && $next['op'] === 'ZEND_FETCH_R') {
                if (isset($curr['var_id']) && isset($next['var_id']) && $curr['var_id'] === $next['var_id']) {
                    $optimized[] = $curr;
                    $i++; // Skip next
                    continue;
                }
            }
            
            $optimized[] = $curr;
        }
        
        return $optimized;
    }
    
    private function isBinaryOp(array $op): bool
    {
        return in_array($op['op'], ['ZEND_ADD', 'ZEND_SUB', 'ZEND_MUL', 'ZEND_DIV']);
    }
    
    private function isLiteralLoad(array $op): bool
    {
        return isset($op['literal_id']) && in_array($op['op'], ['ZEND_INIT_LONG', 'ZEND_INIT_DOUBLE', 'ZEND_INIT_STRING']);
    }
    
    private function isNumeric(array $literal): bool
    {
        return in_array($literal['type'], ['long', 'double']);
    }
    
    private function evaluateBinaryOp(string $op, mixed $left, mixed $right): int|float|null
    {
        return match($op) {
            'ZEND_ADD' => $left + $right,
            'ZEND_SUB' => $left - $right,
            'ZEND_MUL' => $left * $right,
            'ZEND_DIV' => $right != 0 ? $left / $right : null,
            default => null
        };
    }
    
    private function isTerminator(array $op): bool
    {
        return in_array($op['op'], ['ZEND_RETURN', 'ZEND_JMP', 'ZEND_EXIT']);
    }
}
