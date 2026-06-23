# Laravel

The Laravel profile keeps filesystem paths and PHP stubs so Composer, reflection, `__FILE__`, `__DIR__`, and path helpers continue to work as far as possible.

Recommended build flow:

```bash
composer install --no-dev -o -a
php artisan config:cache
php artisan route:cache
php artisan view:cache
php artisan event:cache
bin/phpshield encode ./my-laravel-app ./build --profile=laravel --key-file ./master.key
```

Default Laravel excludes:

```text
.env
storage/
bootstrap/cache/
vendor/
node_modules/
.git/
public/uploads/
logs/
cache/
sessions/
*.log
```

Composer should be optimized with authoritative classmaps before encoding. Dynamic container resolution, reflection-heavy packages, generated proxy classes, migrations, Blade paths, public controller names, config keys, and env keys should be preserved unless an analyzer can prove a rename is safe.

Known issues: reflection, dynamic method calls, variable class names, and non-literal includes can prevent safe symbol obfuscation.
