<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class PathPolicy
{
    /** 
     * @param list<string> $excludes 
     * @param list<string> $includes 
     */
    public function __construct(private array $excludes, private array $includes = []) {}

    public static function defaultExcludes(): array
    {
        return ['.env', 'storage/', 'bootstrap/cache/', 'vendor/', 'node_modules/', '.git/', 'logs/', 'cache/', 'sessions/', 'public/uploads/', '*.log'];
    }

    public function shouldExclude(string $relativePath): bool
    {
        $path = str_replace('\\', '/', ltrim($relativePath, '/'));
        
        // If includes are specified, path must match at least one include pattern
        if (!empty($this->includes)) {
            $included = false;
            foreach ($this->includes as $pattern) {
                $pattern = str_replace('\\', '/', $pattern);
                if (str_ends_with($pattern, '/')) {
                    if ($path === rtrim($pattern, '/') || str_starts_with($path, $pattern)) {
                        $included = true;
                        break;
                    }
                } elseif (fnmatch($pattern, $path) || $path === $pattern) {
                    $included = true;
                    break;
                }
            }
            if (!$included) {
                return true;
            }
        }
        
        // Then check excludes
        foreach ($this->excludes as $pattern) {
            $pattern = str_replace('\\', '/', $pattern);
            if (str_ends_with($pattern, '/')) {
                if ($path === rtrim($pattern, '/') || str_starts_with($path, $pattern)) {
                    return true;
                }
                continue;
            }
            if (fnmatch($pattern, $path) || $path === $pattern) {
                return true;
            }
        }
        return false;
    }
}
