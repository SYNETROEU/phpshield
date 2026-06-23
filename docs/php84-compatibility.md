# PHP 8.4 Compatibility

PHPShield targets PHP 8.4 first. The native loader is a PHP extension and assumes PHP headers and `php-config` from the same PHP build used at runtime.

Supported SAPIs in the first design are CLI, FPM/FastCGI, and Apache module. CLI is the first test target. Windows support targets PHP 8.4 NTS/TS builds through CMake-produced `php_phpshield.dll`; the loader includes Windows MAC address, machine GUID, hostname, and debugger checks.

The compiler uses PHP tokenization to avoid blocking on a third-party parser in the first milestone. Full syntax-aware obfuscation should move to an AST parser with explicit PHP 8.4 grammar support.

PHP 8.5 work requires a separate compatibility pass for extension API changes, opcode changes, optimizer behavior, and opcache integration.
