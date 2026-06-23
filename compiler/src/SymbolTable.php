<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class SymbolTable
{
    public function collect(array $tokens): array
    {
        $symbols = ['classes' => [], 'functions' => [], 'methods' => []];
        $pending = null;
        $className = null;
        foreach ($tokens as $i => $token) {
            if (!is_array($token)) {
                continue;
            }
            if ($token[0] === T_CLASS || $token[0] === T_INTERFACE || $token[0] === T_TRAIT || $token[0] === T_ENUM) {
                $pending = 'classes';
            } elseif ($token[0] === T_FUNCTION) {
                $pending = $className !== null ? 'methods' : 'functions';
            } elseif ($pending && $token[0] === T_STRING) {
                if ($pending === 'classes') {
                    $className = $token[1];
                    $symbols['classes'][] = $token[1];
                } elseif ($pending === 'methods') {
                    $symbols['methods'][] = $className . '::' . $token[1];
                } else {
                    $symbols[$pending][] = $token[1];
                }
                $pending = null;
            }
        }
        return array_map(fn($v) => array_values(array_unique($v)), $symbols);
    }
}
