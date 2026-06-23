<?php
declare(strict_types=1);

namespace PhpShield\Compiler;

final class IrBuilder
{
    public function __construct(private string $strategy = ExecutionStrategy::ZEND_TRANSITIONAL) {}

    public function build(string $relativePath, string $source, array $symbols): string
    {
        $executable = preg_replace('/^\s*<\?(php)?/i', '', $source, 1);
        $executable = preg_replace('/\?>\s*$/', '', (string)$executable, 1);
        $executable = preg_replace('/^\s*declare\s*\(\s*strict_types\s*=\s*1\s*\)\s*;\s*/i', '', (string)$executable, 1);
        
        // Encrypt string literals if using opcode strategy
        $encryptedSource = $executable;
        $stringMap = [];
        if ($this->strategy === ExecutionStrategy::ZEND_OPCODE) {
            [$encryptedSource, $stringMap] = $this->encryptStrings($executable);
        }
        
        $plan = (new OpcodePlan())->describe($relativePath, $symbols);
        $payload = [
            'ir_version' => 1,
            'strategy' => ExecutionStrategy::describe($this->strategy),
            'path' => $relativePath,
            'module' => $plan['module'],
            'metadata' => $plan['metadata_tables'],
            'php' => $this->strategy === ExecutionStrategy::ZEND_TRANSITIONAL ? base64_encode((string)$executable) : null,
            'opcodes' => $this->strategy === ExecutionStrategy::ZEND_OPCODE ? $this->captureOpcodes($encryptedSource, $stringMap) : null,
            'custom_ir' => $this->strategy === ExecutionStrategy::CUSTOM_IR ? $this->buildCustomIr($relativePath, (string)$executable) : null,
            'future_opcode_work' => $plan['future_opcode_work'],
        ];
        return "PSHIR1\n" . json_encode($payload, JSON_THROW_ON_ERROR | JSON_UNESCAPED_SLASHES);
    }
    
    private function encryptStrings(string $source): array
    {
        $stringMap = [];
        $tokens = token_get_all('<?php ' . $source);
        $output = '';
        $stringId = 0;
        
        foreach ($tokens as $token) {
            if (is_array($token) && $token[0] === T_CONSTANT_ENCAPSED_STRING) {
                $key = 'PHPSHIELD_STR_' . $stringId++;
                $stringMap[$key] = $token[1];
                $output .= $key;
            } else {
                $output .= is_array($token) ? $token[1] : $token;
            }
        }
        
        return [substr($output, 6), $stringMap]; // Remove <?php 
    }
    
    private function captureOpcodes(string $source, array $stringMap): array
    {
        // Use php-parser for complete opcode capture
        $capture = new OpcodeCapture();
        $opcodeData = $capture->capture('<?php ' . $source);
        
        // Apply bytecode optimizations
        $optimizer = new BytecodeOptimizer();
        $opcodeData = $optimizer->optimize($opcodeData);
        
        // Apply JIT optimizations if available
        $jitOptimizer = new JitOptimizer();
        $opcodeData = $jitOptimizer->optimizeForJit($opcodeData);
        
        // Add integrity verification
        $verifier = new IntegrityVerifier();
        $opcodeData = $verifier->addChecksums($opcodeData);
        
        // Add string decryption map
        $opcodeData['string_map'] = base64_encode(json_encode($stringMap));
        
        return $opcodeData;
    }

    private function buildCustomIr(string $relativePath, string $source): array
    {
        $source = trim($source);
        $ops = [];
        $namespace = '';
        
        ini_set('pcre.backtrack_limit', '100000');
        ini_set('pcre.recursion_limit', '100');
        
        if (preg_match('/^namespace\s+([^;]+);/m', $source, $m)) {
            $namespace = trim($m[1]);
        }

        if (preg_match_all('/function\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]{0,200}\)\s*(?::\s*[A-Za-z_\\\\][A-Za-z0-9_\\\\]{0,100})?\s*\{\s*return\s+([\'"])((?:(?!\2).){0,1000})\2\s*;\s*\}/s', $source, $matches, PREG_SET_ORDER)) {
            foreach ($matches as $match) {
                $name = $namespace !== '' ? $namespace . '\\' . $match[1] : $match[1];
                $ops[] = ['op' => 'REGISTER_FUNCTION', 'name' => $name, 'return' => $match[3]];
            }
        }

        if (preg_match_all('/(?:final\s+)?class\s+([A-Za-z_][A-Za-z0-9_]*)\b[^{]{0,100}\{((?:[^{}]|\{[^{}]*\}){0,50000})\}\s*$/s', $source, $classMatches, PREG_SET_ORDER)) {
            foreach ($classMatches as $classMatch) {
                $class = $namespace !== '' ? $namespace . '\\' . $classMatch[1] : $classMatch[1];
                if (preg_match_all('/public\s+function\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^)]{0,200}\)\s*(?::\s*[A-Za-z_\\\\][A-Za-z0-9_\\\\]{0,100})?\s*\{\s*return\s+([\'"])((?:(?!\2).){0,1000})\2\s*;\s*\}/s', $classMatch[2], $methodMatches, PREG_SET_ORDER)) {
                    foreach ($methodMatches as $methodMatch) {
                        $ops[] = ['op' => 'REGISTER_METHOD', 'class' => $class, 'method' => $methodMatch[1], 'return' => $methodMatch[3]];
                    }
                }
            }
        }

        if (preg_match_all('/\$([A-Za-z_][A-Za-z0-9_]*)\s*=\s*require\s+__DIR__\s*\.\s*([\'"])\/([^\'"]{1,200})\2\s*;/', $source, $requires, PREG_SET_ORDER)) {
            foreach ($requires as $require) {
                $ops[] = ['op' => 'REQUIRE', 'path' => $require[3], 'assign' => $require[1]];
            }
        }
        if (preg_match_all('/(?<![=\$])require\s+__DIR__\s*\.\s*([\'"])\/([^\'"]{1,200})\1\s*;/', $source, $requires, PREG_SET_ORDER)) {
            foreach ($requires as $require) {
                $ops[] = ['op' => 'REQUIRE', 'path' => $require[2]];
            }
        }
        if (preg_match_all('/echo\s+([A-Za-z_\\\\][A-Za-z0-9_\\\\]{0,100})\s*\(\s*\)\s*\.\s*PHP_EOL\s*;/', $source, $echoes, PREG_SET_ORDER)) {
            foreach ($echoes as $echo) {
                $ops[] = ['op' => 'ECHO_FUNCTION', 'name' => $echo[1], 'newline' => true];
            }
        }
        if (preg_match_all('/echo\s+\(new\s+([A-Za-z_\\\\][A-Za-z0-9_\\\\]{0,100})\s*\(\s*\)\)->([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*\)\s*\.\s*PHP_EOL\s*;/', $source, $echoes, PREG_SET_ORDER)) {
            foreach ($echoes as $echo) {
                $ops[] = ['op' => 'ECHO_METHOD', 'class' => $echo[1], 'method' => $echo[2], 'newline' => true];
            }
        }
        if (preg_match_all('/echo\s+\$([A-Za-z_][A-Za-z0-9_]*)\s*\.\s*PHP_EOL\s*;/', $source, $echoes, PREG_SET_ORDER)) {
            foreach ($echoes as $echo) {
                $ops[] = ['op' => 'ECHO_VAR', 'name' => $echo[1], 'newline' => true];
            }
        }
        if (preg_match('/(?:^|;)\s*return\s+([\'"])((?:(?!\1).){0,1000})\1\s*;?\s*$/s', $source, $return)) {
            $ops[] = ['op' => 'RETURN_STRING', 'value' => $return[2]];
        }

        if ($ops === []) {
            throw new \RuntimeException("{$relativePath}: custom IR does not support this file yet");
        }

        return [
            'version' => 1,
            'encoding' => 'pshield-vm-v1',
            'ops' => $ops,
        ];
    }
}
