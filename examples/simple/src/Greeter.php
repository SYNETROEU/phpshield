<?php
declare(strict_types=1);

namespace Example;

final class Greeter
{
    public function hello(string $name): string
    {
        return "Hello {$name}";
    }
}
