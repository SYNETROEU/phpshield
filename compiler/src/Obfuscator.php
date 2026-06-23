<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class Obfuscator
{
    public function normalize(string $source): string
    {
        $tokens = token_get_all($source);
        $out = '';
        foreach ($tokens as $token) {
            if (is_string($token)) {
                $out .= $token;
                continue;
            }
            [$id, $text] = $token;
            if ($id === T_COMMENT || $id === T_DOC_COMMENT) {
                continue;
            }
            if ($id === T_WHITESPACE) {
                $out .= ' ';
                continue;
            }
            $out .= $text;
        }
        return trim($out) . PHP_EOL;
    }

    public function renameSafePrivateLocals(string $source): string
    {
        // First milestone deliberately preserves symbols. Full PHP-aware local/private renaming
        // needs AST scope analysis to avoid changing Laravel/reflection-sensitive behavior.
        return $this->normalize($source);
    }
}
