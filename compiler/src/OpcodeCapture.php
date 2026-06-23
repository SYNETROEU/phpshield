<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

use PhpParser\{Node, NodeTraverser, NodeVisitorAbstract, ParserFactory};

final class OpcodeCapture
{
    private int $pc = 0;
    private array $literals = [];
    private array $variables = [];
    private array $opcodes = [];
    
    public function capture(string $source): array
    {
        $parser = (new ParserFactory)->createForHostVersion();
        $ast = $parser->parse($source);
        
        $this->pc = 0;
        $this->literals = [];
        $this->variables = [];
        $this->opcodes = [];
        
        $traverser = new NodeTraverser;
        $traverser->addVisitor(new class($this) extends NodeVisitorAbstract {
            public function __construct(private OpcodeCapture $capture) {}
            
            public function enterNode(Node $node) {
                $this->capture->visitNode($node);
                return null;
            }
        });
        
        $traverser->traverse($ast);
        
        return [
            'format' => 'phpshield_opcodes_v3_ast',
            'opcodes' => $this->opcodes,
            'literals' => $this->literals,
            'variables' => $this->variables,
            'metadata' => [
                'num_opcodes' => count($this->opcodes),
                'num_literals' => count($this->literals),
                'num_variables' => count($this->variables),
                'php_version' => PHP_VERSION
            ]
        ];
    }
    
    public function visitNode(Node $node): void
    {
        $line = $node->getStartLine();
        
        match (true) {
            $node instanceof Node\Stmt\Echo_ => $this->emitEcho($node, $line),
            $node instanceof Node\Stmt\Return_ => $this->emitReturn($node, $line),
            $node instanceof Node\Expr\Variable => $this->emitFetchVar($node, $line),
            $node instanceof Node\Scalar\String_ => $this->emitLiteral('string', $node->value, $line),
            $node instanceof Node\Scalar\Int_ => $this->emitLiteral('long', $node->value, $line),
            $node instanceof Node\Scalar\Float_ => $this->emitLiteral('double', $node->value, $line),
            $node instanceof Node\Expr\FuncCall => $this->emitFuncCall($node, $line),
            $node instanceof Node\Expr\MethodCall => $this->emitMethodCall($node, $line),
            $node instanceof Node\Stmt\Function_ => $this->emitFuncDecl($node, $line),
            $node instanceof Node\Stmt\Class_ => $this->emitClassDecl($node, $line),
            $node instanceof Node\Stmt\If_ => $this->emitIf($node, $line),
            $node instanceof Node\Stmt\While_ => $this->emitWhile($node, $line),
            $node instanceof Node\Stmt\For_ => $this->emitFor($node, $line),
            $node instanceof Node\Stmt\Foreach_ => $this->emitForeach($node, $line),
            $node instanceof Node\Expr\Assign => $this->emitAssign($node, $line),
            $node instanceof Node\Expr\BinaryOp => $this->emitBinaryOp($node, $line),
            default => null
        };
    }
    
    private function emitEcho(Node\Stmt\Echo_ $node, int $line): void
    {
        foreach ($node->exprs as $expr) {
            $this->opcodes[] = ['op' => 'ZEND_ECHO', 'pc' => $this->pc++, 'line' => $line];
        }
    }
    
    private function emitReturn(Node\Stmt\Return_ $node, int $line): void
    {
        $this->opcodes[] = [
            'op' => 'ZEND_RETURN',
            'pc' => $this->pc++,
            'line' => $line,
            'has_value' => $node->expr !== null
        ];
    }
    
    private function emitFetchVar(Node\Expr\Variable $node, int $line): void
    {
        $varName = is_string($node->name) ? $node->name : null;
        if (!$varName) return;
        
        if (!isset($this->variables[$varName])) {
            $this->variables[$varName] = count($this->variables);
        }
        
        $this->opcodes[] = [
            'op' => 'ZEND_FETCH_R',
            'pc' => $this->pc++,
            'var_id' => $this->variables[$varName],
            'line' => $line
        ];
    }
    
    private function emitLiteral(string $type, mixed $value, int $line): void
    {
        $litId = count($this->literals);
        $this->literals[$litId] = ['type' => $type, 'value' => $value];
        
        $opName = match($type) {
            'string' => 'ZEND_INIT_STRING',
            'long' => 'ZEND_INIT_LONG',
            'double' => 'ZEND_INIT_DOUBLE',
            default => 'ZEND_NOP'
        };
        
        $this->opcodes[] = ['op' => $opName, 'pc' => $this->pc++, 'literal_id' => $litId, 'line' => $line];
    }
    
    private function emitFuncCall(Node\Expr\FuncCall $node, int $line): void
    {
        $name = $node->name instanceof Node\Name ? $node->name->toString() : null;
        if (!$name) return;
        
        $this->opcodes[] = ['op' => 'ZEND_INIT_FCALL', 'pc' => $this->pc++, 'function' => $name, 'line' => $line, 'num_args' => count($node->args)];
        $this->opcodes[] = ['op' => 'ZEND_DO_FCALL', 'pc' => $this->pc++, 'line' => $line];
    }
    
    private function emitMethodCall(Node\Expr\MethodCall $node, int $line): void
    {
        $method = $node->name instanceof Node\Identifier ? $node->name->name : null;
        if (!$method) return;
        
        $this->opcodes[] = ['op' => 'ZEND_INIT_METHOD_CALL', 'pc' => $this->pc++, 'method' => $method, 'line' => $line, 'num_args' => count($node->args)];
        $this->opcodes[] = ['op' => 'ZEND_DO_FCALL', 'pc' => $this->pc++, 'line' => $line];
    }
    
    private function emitFuncDecl(Node\Stmt\Function_ $node, int $line): void
    {
        $this->opcodes[] = ['op' => 'ZEND_DECLARE_FUNCTION', 'pc' => $this->pc++, 'name' => $node->name->name, 'line' => $line];
    }
    
    private function emitClassDecl(Node\Stmt\Class_ $node, int $line): void
    {
        $this->opcodes[] = ['op' => 'ZEND_DECLARE_CLASS', 'pc' => $this->pc++, 'name' => $node->name?->name, 'line' => $line];
    }
    
    private function emitIf(Node\Stmt\If_ $node, int $line): void
    {
        $this->opcodes[] = ['op' => 'ZEND_JMPZ', 'pc' => $this->pc++, 'line' => $line, 'jump_target' => null];
    }
    
    private function emitWhile(Node\Stmt\While_ $node, int $line): void
    {
        $loopStart = $this->pc;
        $this->opcodes[] = ['op' => 'ZEND_JMP_SET', 'pc' => $this->pc++, 'line' => $line, 'loop_start' => $loopStart];
    }
    
    private function emitFor(Node\Stmt\For_ $node, int $line): void
    {
        $this->opcodes[] = ['op' => 'ZEND_JMP', 'pc' => $this->pc++, 'line' => $line];
    }
    
    private function emitForeach(Node\Stmt\Foreach_ $node, int $line): void
    {
        $this->opcodes[] = ['op' => 'ZEND_FE_RESET_R', 'pc' => $this->pc++, 'line' => $line];
        $this->opcodes[] = ['op' => 'ZEND_FE_FETCH_R', 'pc' => $this->pc++, 'line' => $line];
    }
    
    private function emitAssign(Node\Expr\Assign $node, int $line): void
    {
        $this->opcodes[] = ['op' => 'ZEND_ASSIGN', 'pc' => $this->pc++, 'line' => $line];
    }
    
    private function emitBinaryOp(Node\Expr\BinaryOp $node, int $line): void
    {
        $opMap = [
            Node\Expr\BinaryOp\Plus::class => 'ZEND_ADD',
            Node\Expr\BinaryOp\Minus::class => 'ZEND_SUB',
            Node\Expr\BinaryOp\Mul::class => 'ZEND_MUL',
            Node\Expr\BinaryOp\Div::class => 'ZEND_DIV',
            Node\Expr\BinaryOp\Concat::class => 'ZEND_CONCAT',
            Node\Expr\BinaryOp\Equal::class => 'ZEND_IS_EQUAL',
            Node\Expr\BinaryOp\NotEqual::class => 'ZEND_IS_NOT_EQUAL',
            Node\Expr\BinaryOp\Identical::class => 'ZEND_IS_IDENTICAL',
            Node\Expr\BinaryOp\Greater::class => 'ZEND_IS_SMALLER',
            Node\Expr\BinaryOp\GreaterOrEqual::class => 'ZEND_IS_SMALLER_OR_EQUAL',
        ];
        
        $opName = $opMap[get_class($node)] ?? 'ZEND_NOP';
        $this->opcodes[] = ['op' => $opName, 'pc' => $this->pc++, 'line' => $line];
    }
    
    public function serializeOpcodes(array $opcodeData): string
    {
        return function_exists('msgpack_pack') 
            ? msgpack_pack($opcodeData)
            : json_encode($opcodeData, JSON_THROW_ON_ERROR);
    }
}
