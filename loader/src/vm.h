#ifndef PHPSHIELD_VM_H
#define PHPSHIELD_VM_H

#include "php.h"

// Custom VM opcodes (not Zend opcodes) - Full coverage
typedef enum {
    // Stack operations
    PS_OP_NOP = 0,
    PS_OP_LOAD_CONST,    // Load constant to stack
    PS_OP_LOAD_VAR,      // Load variable to stack
    PS_OP_STORE_VAR,     // Store stack top to variable
    PS_OP_POP,           // Pop stack
    PS_OP_DUP,           // Duplicate stack top
    
    // Arithmetic
    PS_OP_ADD,
    PS_OP_SUB,
    PS_OP_MUL,
    PS_OP_DIV,
    PS_OP_MOD,
    PS_OP_POW,
    PS_OP_NEG,           // Negate
    
    // String operations
    PS_OP_CONCAT,
    
    // Comparison
    PS_OP_CMP_EQ,
    PS_OP_CMP_NE,
    PS_OP_CMP_LT,
    PS_OP_CMP_LE,
    PS_OP_CMP_GT,
    PS_OP_CMP_GE,
    PS_OP_CMP_IDENTICAL,
    PS_OP_CMP_NOT_IDENTICAL,
    
    // Logical
    PS_OP_AND,
    PS_OP_OR,
    PS_OP_NOT,
    PS_OP_XOR,
    
    // Bitwise
    PS_OP_BW_AND,
    PS_OP_BW_OR,
    PS_OP_BW_XOR,
    PS_OP_BW_NOT,
    PS_OP_SHL,
    PS_OP_SHR,
    
    // Control flow
    PS_OP_JMP,
    PS_OP_JMP_IF,
    PS_OP_JMP_NOT,
    PS_OP_JMP_Z,         // Jump if zero
    PS_OP_JMP_NZ,        // Jump if not zero
    
    // Functions
    PS_OP_CALL,          // Call function
    PS_OP_CALL_METHOD,   // Call method
    PS_OP_RETURN,
    PS_OP_RETURN_VAL,
    
    // Arrays
    PS_OP_ARRAY_NEW,
    PS_OP_ARRAY_PUSH,
    PS_OP_ARRAY_FETCH,
    PS_OP_ARRAY_SET,
    
    // Objects
    PS_OP_OBJ_NEW,
    PS_OP_OBJ_FETCH,
    PS_OP_OBJ_SET,
    
    // Output
    PS_OP_ECHO,
    PS_OP_PRINT,
    
    // Type operations
    PS_OP_CAST_BOOL,
    PS_OP_CAST_INT,
    PS_OP_CAST_FLOAT,
    PS_OP_CAST_STRING,
    PS_OP_CAST_ARRAY,
    
    // Control flow obfuscation (fake opcodes)
    PS_OP_JUNK1,         // Dead code
    PS_OP_JUNK2,
    PS_OP_JUNK3,
} phpshield_vm_opcode;

// VM instruction with obfuscation
typedef struct {
    uint32_t op_encoded;     // Encoded opcode (XOR with key)
    uint32_t operand1;
    uint32_t operand2;
    uint32_t checksum;       // Anti-tamper checksum
} phpshield_vm_instruction;

// VM bytecode with anti-tampering
typedef struct {
    phpshield_vm_instruction *instructions;
    size_t instruction_count;
    zval *constants;
    size_t constant_count;
    char **var_names;
    size_t var_count;
    uint32_t vm_key;         // Key to decode instructions
    uint32_t integrity_hash; // Bytecode integrity hash
} phpshield_vm_bytecode;

// Execute custom bytecode
int phpshield_vm_execute(phpshield_vm_bytecode *bytecode, zval *retval);

// Free bytecode
void phpshield_vm_bytecode_free(phpshield_vm_bytecode *bytecode);

#endif
