# PHPShield Compiler

The compiler is a PHP 8.4 command-line encoder. It walks a source tree, analyzes PHP files for dynamic constructs, preserves paths, writes PHP stubs, and stores protected per-file IR segments in an authenticated encrypted `.pshield` bundle.

The first milestone uses a protected internal representation with a transitional Zend compile-string execution strategy. That is not marketed as equivalent to ionCube or SourceGuardian bytecode. The code is structured so the `IrBuilder` and native `execute` layer can later switch to captured Zend opcodes or a custom IR interpreter without changing bundle, license, or stub contracts.
