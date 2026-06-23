<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class NameObfuscator
{
    private array $nameMap = [];
    private int $counter = 0;
    
    public function obfuscate(string $source, array $symbols): string
    {
        // Build rename map for safe-to-rename identifiers
        $this->buildNameMap($symbols);
        
        // Apply renames
        $tokens = token_get_all($source);
        $output = '';
        
        foreach ($tokens as $token) {
            if (is_array($token)) {
                if ($token[0] === T_STRING && isset($this->nameMap[$token[1]])) {
                    $output .= $this->nameMap[$token[1]];
                } else {
                    $output .= $token[1];
                }
            } else {
                $output .= $token;
            }
        }
        
        return $output;
    }
    
    private function buildNameMap(array $symbols): void
    {
        foreach ($symbols['functions'] ?? [] as $func) {
            if ($this->isSafeToRename($func)) {
                $this->nameMap[$func] = $this->generateName();
            }
        }
        
        foreach ($symbols['classes'] ?? [] as $class) {
            if ($this->isSafeToRename($class)) {
                $this->nameMap[$class] = $this->generateName();
            }
        }
    }
    
    private function isSafeToRename(string $name): bool
    {
        // Don't rename magic methods, built-ins, or public APIs
        $unsafe = ['__construct', '__destruct', '__call', '__get', '__set', 
                   '__isset', '__unset', '__toString', '__invoke', '__clone'];
        
        return !in_array(strtolower($name), $unsafe, true)
            && !function_exists($name)
            && !class_exists($name);
    }
    
    private function generateName(): string
    {
        // Generate short obfuscated names
        $chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
        $name = '_' . $chars[$this->counter % 52];
        $num = (int)($this->counter / 52);
        if ($num > 0) {
            $name .= $num;
        }
        $this->counter++;
        return $name;
    }
    
    public function getNameMap(): array
    {
        return $this->nameMap;
    }
}
