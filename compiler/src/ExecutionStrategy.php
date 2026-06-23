<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class ExecutionStrategy
{
    public const ZEND_TRANSITIONAL = 'zend_transitional_compile_string';
    public const ZEND_OP_ARRAY_FUTURE = 'zend_op_array_protected';
    public const CUSTOM_IR = 'phpshield_custom_ir';

    public static function firstMilestone(): array
    {
        return self::describe(self::ZEND_TRANSITIONAL);
    }

    public static function describe(string $strategy): array
    {
        if (!in_array($strategy, [self::ZEND_TRANSITIONAL, self::ZEND_OP_ARRAY_FUTURE, self::CUSTOM_IR], true)) {
            throw new \InvalidArgumentException("unsupported execution strategy: {$strategy}");
        }
        return [
            'name' => $strategy,
            'production_equivalent_to_opcode_loader' => $strategy !== self::ZEND_TRANSITIONAL,
            'implemented_in_native_loader' => in_array($strategy, [self::ZEND_TRANSITIONAL, self::CUSTOM_IR], true),
            'plaintext_disk_write_allowed' => false,
            'replaceable_by' => [self::ZEND_OP_ARRAY_FUTURE, self::CUSTOM_IR],
        ];
    }
}
