<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class LaravelProfile
{
    public static function excludes(): array
    {
        return ['.env', 'storage/', 'bootstrap/cache/', 'vendor/', 'node_modules/', '.git/', 'public/uploads/', 'logs/', 'cache/', 'sessions/', '*.log'];
    }

    public static function preservedSymbols(): array
    {
        return ['controllers', 'models', 'migrations', 'routes', 'config keys', 'env keys', 'blade paths', 'container bindings'];
    }
}
