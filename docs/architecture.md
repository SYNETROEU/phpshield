# Architecture

Source flows through parser, analyzer, symbol table, safe obfuscator, IR builder, bundle writer, and native loader.

The compiler preserves filesystem paths by copying the project into an output directory and replacing protected PHP files with stubs. The bundle stores encrypted per-file segments. Each segment contains a protected IR envelope with metadata tables, constants, symbols, relocation placeholders, and an execution strategy.

Loader flow:

1. `phpshield_load(__FILE__)` receives the stub path.
2. The loader maps the stub path to a relative bundle path.
3. The bundle header and manifest MAC are verified.
4. The matching segment is selected.
5. The payload AEAD tag is verified and decrypted in memory.
6. License constraints are checked when required.
7. The execution strategy runs the protected segment.

When `phpshield.cache=1`, the loader may cache decrypted IR in process memory. The cache is keyed by bundle path, protected relative path, and authenticated ciphertext hash. Manifest verification still runs before cached IR is used.

The implemented execution strategy is `zend_transitional_compile_string`. It is a milestone bridge, not final bytecode protection. The IR format also names future strategies, including `zend_op_array_protected` and `phpshield_custom_ir`; the native loader rejects those modes until a real opcode/custom-IR executor is implemented. This avoids silently falling back to weaker execution.
