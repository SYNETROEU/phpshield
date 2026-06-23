<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class OpcodePlan
{
    public function describe(string $relativePath, array $symbols): array
    {
        return [
            'module' => [
                'name' => $relativePath,
                'format' => 'pshir',
                'file_segments' => [
                    ['id' => 0, 'kind' => 'php_source_bridge', 'path' => $relativePath],
                ],
            ],
            'metadata_tables' => [
                'symbols' => $symbols,
                'constants' => [],
                'relocations' => [],
                'op_arrays' => [],
            ],
            'future_opcode_work' => [
                'capture Zend op_array after compile',
                'serialize protected op_array per PHP API version',
                'bind literals and names through encrypted tables',
                'execute through loader-owned dispatch path',
                'integrate with opcache validation and invalidation',
            ],
        ];
    }
}
