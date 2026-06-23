<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

use PhpParser\Node;
use PhpParser\NodeTraverser;
use PhpParser\NodeVisitorAbstract;

/**
 * Compiles PHP AST to custom VM bytecode
 * This creates ionCube-style protection by using custom opcodes
 */
final class VMCompiler
{
    private array $instructions = [];
    private array $constants = [];
    private array $varNames = [];
    private int $nextVarId = 0;
    private int $nextLabel = 0;
    private array $varMap = [];
    
    public function compile(array $ast): array
    {
        foreach ($ast as $stmt) {
            $this->compileStmt($stmt);
        }
        
        // Add return at end
        $this->emit('RETURN_VAL', 0, 0);
        
        return $this->generateBytecode();
    }
    
    private function compileStmt(Node $node): void
    {
        if ($node instanceof Node\Stmt\Echo_) {
            foreach ($node->exprs as $expr) {
                $this->compileExpr($expr);
                $this->emit('ECHO', 0, 0);
            }
        } elseif ($node instanceof Node\Stmt\Expression) {
            $this->compileExpr($node->expr);
            $this->emit('POP', 0, 0); // Discard result
        } elseif ($node instanceof Node\Stmt\Return_) {
            if ($node->expr) {
                $this->compileExpr($node->expr);
                $this->emit('RETURN_VAL', 0, 0);
            } else {
                $this->emit('RETURN', 0, 0);
            }
        } elseif ($node instanceof Node\Stmt\If_) {
            $this->compileIf($node);
        } elseif ($node instanceof Node\Stmt\While_) {
            $this->compileWhile($node);
        } elseif ($node instanceof Node\Stmt\Foreach_) {
            // Simplified: compile foreach as while
            $this->compileStmts($node->stmts);
        } else {
            // Other statements
            $this->compileStmts($node->stmts ?? []);
        }
    }
    
    private function compileExpr(Node\Expr $node): void
    {
        if ($node instanceof Node\Scalar\String_) {
            $constId = $this->addConstant($node->value);
            $this->emit('LOAD_CONST', $constId, 0);
        } elseif ($node instanceof Node\Scalar\Int_) {
            $constId = $this->addConstant($node->value);
            $this->emit('LOAD_CONST', $constId, 0);
        } elseif ($node instanceof Node\Expr\Variable) {
            $varId = $this->getVarId($node->name);
            $this->emit('LOAD_VAR', $varId, 0);
        } elseif ($node instanceof Node\Expr\Assign) {
            $this->compileExpr($node->expr);
            if ($node->var instanceof Node\Expr\Variable) {
                $varId = $this->getVarId($node->var->name);
                $this->emit('DUP', 0, 0);
                $this->emit('STORE_VAR', $varId, 0);
            }
        } elseif ($node instanceof Node\Expr\BinaryOp\Plus) {
            $this->compileExpr($node->left);
            $this->compileExpr($node->right);
            $this->emit('ADD', 0, 0);
        } elseif ($node instanceof Node\Expr\BinaryOp\Minus) {
            $this->compileExpr($node->left);
            $this->compileExpr($node->right);
            $this->emit('SUB', 0, 0);
        } elseif ($node instanceof Node\Expr\BinaryOp\Mul) {
            $this->compileExpr($node->left);
            $this->compileExpr($node->right);
            $this->emit('MUL', 0, 0);
        } elseif ($node instanceof Node\Expr\BinaryOp\Div) {
            $this->compileExpr($node->left);
            $this->compileExpr($node->right);
            $this->emit('DIV', 0, 0);
        } elseif ($node instanceof Node\Expr\BinaryOp\Concat) {
            $this->compileExpr($node->left);
            $this->compileExpr($node->right);
            $this->emit('CONCAT', 0, 0);
        } elseif ($node instanceof Node\Expr\BinaryOp\Equal) {
            $this->compileExpr($node->left);
            $this->compileExpr($node->right);
            $this->emit('CMP_EQ', 0, 0);
        } elseif ($node instanceof Node\Expr\BinaryOp\Smaller) {
            $this->compileExpr($node->left);
            $this->compileExpr($node->right);
            $this->emit('CMP_LT', 0, 0);
        } elseif ($node instanceof Node\Expr\BinaryOp\Greater) {
            $this->compileExpr($node->left);
            $this->compileExpr($node->right);
            $this->emit('CMP_GT', 0, 0);
        } elseif ($node instanceof Node\Expr\BinaryOp\Identical) {
            $this->compileExpr($node->left);
            $this->compileExpr($node->right);
            $this->emit('CMP_IDENTICAL', 0, 0);
        } elseif ($node instanceof Node\Expr\BooleanNot) {
            $this->compileExpr($node->expr);
            $this->emit('NOT', 0, 0);
        } else {
            // Fallback: load null
            $constId = $this->addConstant(null);
            $this->emit('LOAD_CONST', $constId, 0);
        }
    }
    
    private function compileIf(Node\Stmt\If_ $node): void
    {
        $this->compileExpr($node->cond);
        
        $elseLabel = $this->nextLabel++;
        $endLabel = $this->nextLabel++;
        
        $this->emit('JMP_NOT', $elseLabel, 0);
        $this->compileStmts($node->stmts);
        $this->emit('JMP', $endLabel, 0);
        
        $this->instructions[$elseLabel] = ['op' => 'NOP', 'op1' => 0, 'op2' => 0];
        
        if ($node->else) {
            $this->compileStmts($node->else->stmts ?? []);
        }
        
        $this->instructions[$endLabel] = ['op' => 'NOP', 'op1' => 0, 'op2' => 0];
    }
    
    private function compileWhile(Node\Stmt\While_ $node): void
    {
        $startLabel = count($this->instructions);
        $endLabel = $this->nextLabel++;
        
        $this->compileExpr($node->cond);
        $this->emit('JMP_NOT', $endLabel, 0);
        $this->compileStmts($node->stmts);
        $this->emit('JMP', $startLabel, 0);
        
        $this->instructions[$endLabel] = ['op' => 'NOP', 'op1' => 0, 'op2' => 0];
    }
    
    private function compileStmts(array $stmts): void
    {
        foreach ($stmts as $stmt) {
            $this->compileStmt($stmt);
        }
    }
    
    private function emit(string $op, int $op1, int $op2): void
    {
        $this->instructions[] = ['op' => $op, 'op1' => $op1, 'op2' => $op2];
        
        // Add dead code for obfuscation (every 10 instructions)
        if (count($this->instructions) % 10 == 0) {
            $junk = ['JUNK1', 'JUNK2', 'JUNK3'][random_int(0, 2)];
            $this->instructions[] = ['op' => $junk, 'op1' => 0, 'op2' => 0];
        }
    }
    
    private function addConstant($value): int
    {
        $this->constants[] = $value;
        return count($this->constants) - 1;
    }
    
    private function getVarId(string $name): int
    {
        if (!isset($this->varMap[$name])) {
            $this->varMap[$name] = $this->nextVarId++;
            $this->varNames[] = $name;
        }
        return $this->varMap[$name];
    }
    
    private function generateBytecode(): array
    {
        // Generate VM key
        $vmKey = random_int(0, 0xFFFFFFFF);
        
        // Encode instructions
        $encoded = [];
        foreach ($this->instructions as $i => $inst) {
            $opcode = $this->opcodeToInt($inst['op']);
            
            // Calculate checksum for anti-tampering
            $checksum = ($i * 0x9E3779B9) ^ $opcode;
            
            // Encode opcode
            $encodedOp = $opcode ^ $vmKey ^ $checksum;
            
            $encoded[] = [
                'op' => $encodedOp,
                'op1' => $inst['op1'],
                'op2' => $inst['op2'],
                'checksum' => $checksum,
            ];
        }
        
        // Calculate integrity hash
        $integrityHash = 0;
        foreach ($encoded as $inst) {
            $integrityHash = (($integrityHash << 5) + $integrityHash) + $inst['op'];
        }
        
        return [
            'instructions' => $encoded,
            'constants' => $this->constants,
            'var_names' => $this->varNames,
            'vm_key' => $vmKey,
            'integrity_hash' => $integrityHash & 0xFFFFFFFF,
        ];
    }
    
    private function opcodeToInt(string $op): int
    {
        $opcodes = [
            'NOP' => 0, 'LOAD_CONST' => 1, 'LOAD_VAR' => 2, 'STORE_VAR' => 3,
            'POP' => 4, 'DUP' => 5, 'ADD' => 6, 'SUB' => 7, 'MUL' => 8,
            'DIV' => 9, 'MOD' => 10, 'POW' => 11, 'NEG' => 12, 'CONCAT' => 13,
            'CMP_EQ' => 14, 'CMP_NE' => 15, 'CMP_LT' => 16, 'CMP_LE' => 17,
            'CMP_GT' => 18, 'CMP_GE' => 19, 'CMP_IDENTICAL' => 20, 'CMP_NOT_IDENTICAL' => 21,
            'AND' => 22, 'OR' => 23, 'NOT' => 24, 'XOR' => 25,
            'BW_AND' => 26, 'BW_OR' => 27, 'BW_XOR' => 28, 'BW_NOT' => 29,
            'SHL' => 30, 'SHR' => 31, 'JMP' => 32, 'JMP_IF' => 33,
            'JMP_NOT' => 34, 'JMP_Z' => 35, 'JMP_NZ' => 36, 'CALL' => 37,
            'CALL_METHOD' => 38, 'RETURN' => 39, 'RETURN_VAL' => 40,
            'ECHO' => 45, 'PRINT' => 46,
            'CAST_BOOL' => 47, 'CAST_INT' => 48, 'CAST_STRING' => 49,
            'JUNK1' => 60, 'JUNK2' => 61, 'JUNK3' => 62,
        ];
        return $opcodes[$op] ?? 0;
    }
}
