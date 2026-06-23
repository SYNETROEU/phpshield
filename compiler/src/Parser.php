<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class Parser
{
    public function parse(string $path, string $source): array
    {
        $tokens = token_get_all($source, TOKEN_PARSE);
        return ['path' => $path, 'tokens' => $tokens, 'source_sha256' => Crypto::hash($source)];
    }
}
