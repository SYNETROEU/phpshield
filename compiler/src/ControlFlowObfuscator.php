<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class ControlFlowObfuscator
{
    public function obfuscate(string $source): string
    {
        $tokens = token_get_all($source);
        $output = '';
        $braceDepth = 0;
        
        foreach ($tokens as $i => $token) {
            if (is_array($token)) {
                $output .= $token[1];
                
                // Insert dead code after function declarations
                if ($token[0] === T_FUNCTION && $braceDepth === 0) {
                    $nextBrace = $this->findNextBrace($tokens, $i);
                    if ($nextBrace !== -1) {
                        // Will insert dead code after opening brace
                    }
                }
            } else {
                $output .= $token;
                
                // Track brace depth
                if ($token === '{') {
                    $braceDepth++;
                    // Insert opaque predicate
                    if (rand(0, 3) === 0) {
                        $output .= $this->generateOpaqueCode();
                    }
                } elseif ($token === '}') {
                    $braceDepth--;
                }
            }
        }
        
        return $output;
    }
    
    private function findNextBrace(array $tokens, int $start): int
    {
        for ($i = $start; $i < count($tokens); $i++) {
            if (!is_array($tokens[$i]) && $tokens[$i] === '{') {
                return $i;
            }
        }
        return -1;
    }
    
    private function generateOpaqueCode(): string
    {
        $predicates = [
            'if(1===1){}',
            'if(2>1){}',
            '$_=0;',
            'if(true){}',
        ];
        
        return $predicates[array_rand($predicates)];
    }
    
    public function addDeadCodeBlocks(string $source): string
    {
        // Add unreachable code blocks
        $deadCode = [
            'if(false){$_="' . bin2hex(random_bytes(8)) . '";}',
            'while(false){break;}',
            'for($i=0;false;$i++){}',
        ];
        
        $lines = explode("\n", $source);
        $insertPoints = array_keys(array_filter($lines, fn($l) => str_contains($l, '{')));
        
        foreach (array_slice($insertPoints, 0, min(5, count($insertPoints))) as $line) {
            $lines[$line] .= $deadCode[array_rand($deadCode)];
        }
        
        return implode("\n", $lines);
    }
}
