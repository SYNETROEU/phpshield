<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class Analyzer
{
    public function analyze(string $relativePath, string $source): array
    {
        $warnings = [];
        $checks = [
            '/\beval\s*\(/i' => 'eval use requires careful review',
            '/\$\$[A-Za-z_]/' => 'variable variables limit safe obfuscation',
            '/->\s*\{?\s*\$/' => 'dynamic method/property access limits safe obfuscation',
            '/\bReflection[A-Za-z_\\\\]*/' => 'reflection may depend on original symbols',
            '/\b(include|include_once|require|require_once)\s*\(?\s*(?![\'"])/i' => 'non-literal include/require path',
            '/app\s*\(\s*\$|make\s*\(\s*\$/i' => 'dynamic Laravel container resolution',
            '/function\s+__(call|callStatic|get|set|isset|unset|invoke|toString|debugInfo|serialize|unserialize|sleep|wakeup)\s*\(/i' => 'magic method present',
        ];
        foreach ($checks as $pattern => $message) {
            if (preg_match($pattern, $source)) {
                $warnings[] = "{$relativePath}: {$message}";
            }
        }
        return ['warnings' => $warnings];
    }
}
