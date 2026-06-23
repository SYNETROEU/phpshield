<?php
declare(strict_types=1);

namespace Fixture;

final class Greeter
{
    private string $secret = 'secret fixture phrase';

    public function hello(): string
    {
        return 'class ok';
    }
}
