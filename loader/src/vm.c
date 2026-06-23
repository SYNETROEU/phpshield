#include "vm.h"
#include "anti_tamper.h"
#include "memcrypt.h"
#include <string.h>

#define MAX_STACK 256
#define MAX_VARS 128

// Decode instruction opcode
static inline phpshield_vm_opcode decode_opcode(phpshield_vm_instruction *inst, uint32_t vm_key)
{
    // XOR decode with vm_key and instruction checksum
    uint32_t decoded = inst->op_encoded ^ vm_key ^ inst->checksum;
    return (phpshield_vm_opcode)(decoded & 0xFF);
}

// Verify instruction integrity
static inline int verify_instruction(phpshield_vm_instruction *inst, size_t ip)
{
    // Simple integrity check: checksum should match position-based hash
    uint32_t expected = (uint32_t)((ip * 0x9E3779B9) ^ inst->op_encoded);
    return (inst->checksum == expected) ? SUCCESS : FAILURE;
}

int phpshield_vm_execute(phpshield_vm_bytecode *bytecode, zval *retval)
{
    zval stack[MAX_STACK];
    int stack_top = -1;
    zval vars[MAX_VARS];
    size_t ip = 0;
    uint32_t vm_key = bytecode->vm_key;
    
    // Initialize variables
    for (size_t i = 0; i < MAX_VARS; i++) {
        ZVAL_UNDEF(&vars[i]);
    }
    
    // Initial anti-debug check
    phpshield_anti_tamper_continuous_check();
    
    // Verify bytecode integrity
    uint32_t computed_hash = 0;
    for (size_t i = 0; i < bytecode->instruction_count; i++) {
        computed_hash = ((computed_hash << 5) + computed_hash) + bytecode->instructions[i].op_encoded;
    }
    if (computed_hash != bytecode->integrity_hash) {
        goto error; // Bytecode tampered
    }
    
    while (ip < bytecode->instruction_count) {
        phpshield_vm_instruction *inst = &bytecode->instructions[ip];
        
        // Verify instruction integrity (anti-tampering)
        if (verify_instruction(inst, ip) != SUCCESS) {
            goto error;
        }
        
        // Decode opcode
        phpshield_vm_opcode op = decode_opcode(inst, vm_key);
        
        // Periodic anti-debug checks
        if (ip % 50 == 0) {
            phpshield_anti_tamper_continuous_check();
        }
        
        switch (op) {
            case PS_OP_NOP:
            case PS_OP_JUNK1:
            case PS_OP_JUNK2:
            case PS_OP_JUNK3:
                // Dead code for obfuscation
                break;
                
            case PS_OP_LOAD_CONST:
                if (stack_top >= MAX_STACK - 1) goto error;
                if (inst->operand1 >= bytecode->constant_count) goto error;
                stack[++stack_top] = bytecode->constants[inst->operand1];
                Z_TRY_ADDREF(stack[stack_top]);
                break;
                
            case PS_OP_LOAD_VAR:
                if (stack_top >= MAX_STACK - 1) goto error;
                if (inst->operand1 >= MAX_VARS) goto error;
                if (Z_TYPE(vars[inst->operand1]) == IS_UNDEF) {
                    ZVAL_NULL(&stack[++stack_top]);
                } else {
                    stack[++stack_top] = vars[inst->operand1];
                    Z_TRY_ADDREF(stack[stack_top]);
                }
                break;
                
            case PS_OP_STORE_VAR:
                if (stack_top < 0) goto error;
                if (inst->operand1 >= MAX_VARS) goto error;
                if (Z_TYPE(vars[inst->operand1]) != IS_UNDEF) {
                    zval_ptr_dtor(&vars[inst->operand1]);
                }
                vars[inst->operand1] = stack[stack_top];
                Z_TRY_ADDREF(vars[inst->operand1]);
                zval_ptr_dtor(&stack[stack_top--]);
                break;
                
            case PS_OP_POP:
                if (stack_top < 0) goto error;
                zval_ptr_dtor(&stack[stack_top--]);
                break;
                
            case PS_OP_DUP:
                if (stack_top < 0 || stack_top >= MAX_STACK - 1) goto error;
                stack[stack_top + 1] = stack[stack_top];
                Z_TRY_ADDREF(stack[stack_top + 1]);
                stack_top++;
                break;
                
            case PS_OP_ADD:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    add_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_SUB:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    sub_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_MUL:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    mul_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_DIV:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    div_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_MOD:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    mod_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_CONCAT:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    concat_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_CMP_EQ:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    is_equal_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_CMP_LT:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    is_smaller_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_CMP_GT:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    is_smaller_function(&result, &stack[stack_top], &stack[stack_top - 1]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_CMP_IDENTICAL:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    is_identical_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_NOT:
                if (stack_top < 0) goto error;
                {
                    zval result;
                    boolean_not_function(&result, &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_BW_AND:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    bitwise_and_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_BW_OR:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    bitwise_or_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_BW_XOR:
                if (stack_top < 1) goto error;
                {
                    zval result;
                    bitwise_xor_function(&result, &stack[stack_top - 1], &stack[stack_top]);
                    zval_ptr_dtor(&stack[stack_top--]);
                    zval_ptr_dtor(&stack[stack_top]);
                    stack[stack_top] = result;
                }
                break;
                
            case PS_OP_ECHO:
                if (stack_top < 0) goto error;
                {
                    zend_string *str = zval_get_string(&stack[stack_top]);
                    php_write(ZSTR_VAL(str), ZSTR_LEN(str));
                    zend_string_release(str);
                    zval_ptr_dtor(&stack[stack_top--]);
                }
                break;
                
            case PS_OP_RETURN:
                ZVAL_NULL(retval);
                goto cleanup;
                
            case PS_OP_RETURN_VAL:
                if (stack_top >= 0) {
                    ZVAL_COPY(retval, &stack[stack_top]);
                } else {
                    ZVAL_NULL(retval);
                }
                goto cleanup;
                
            case PS_OP_JMP:
                if (inst->operand1 >= bytecode->instruction_count) goto error;
                ip = inst->operand1;
                continue;
                
            case PS_OP_JMP_IF:
                if (stack_top < 0) goto error;
                if (zend_is_true(&stack[stack_top])) {
                    if (inst->operand1 >= bytecode->instruction_count) goto error;
                    ip = inst->operand1;
                    zval_ptr_dtor(&stack[stack_top--]);
                    continue;
                }
                zval_ptr_dtor(&stack[stack_top--]);
                break;
                
            case PS_OP_JMP_NOT:
                if (stack_top < 0) goto error;
                if (!zend_is_true(&stack[stack_top])) {
                    if (inst->operand1 >= bytecode->instruction_count) goto error;
                    ip = inst->operand1;
                    zval_ptr_dtor(&stack[stack_top--]);
                    continue;
                }
                zval_ptr_dtor(&stack[stack_top--]);
                break;
                
            case PS_OP_CAST_STRING:
                if (stack_top < 0) goto error;
                convert_to_string(&stack[stack_top]);
                break;
                
            case PS_OP_CAST_INT:
                if (stack_top < 0) goto error;
                convert_to_long(&stack[stack_top]);
                break;
                
            case PS_OP_CAST_BOOL:
                if (stack_top < 0) goto error;
                convert_to_boolean(&stack[stack_top]);
                break;
                
            default:
                goto error;
        }
        
        ip++;
    }
    
cleanup:
    while (stack_top >= 0) {
        zval_ptr_dtor(&stack[stack_top--]);
    }
    for (size_t i = 0; i < MAX_VARS; i++) {
        if (Z_TYPE(vars[i]) != IS_UNDEF) {
            zval_ptr_dtor(&vars[i]);
        }
    }
    return SUCCESS;
    
error:
    while (stack_top >= 0) {
        zval_ptr_dtor(&stack[stack_top--]);
    }
    for (size_t i = 0; i < MAX_VARS; i++) {
        if (Z_TYPE(vars[i]) != IS_UNDEF) {
            zval_ptr_dtor(&vars[i]);
        }
    }
    return FAILURE;
}

void phpshield_vm_bytecode_free(phpshield_vm_bytecode *bytecode)
{
    if (bytecode->instructions) {
        efree(bytecode->instructions);
    }
    if (bytecode->constants) {
        for (size_t i = 0; i < bytecode->constant_count; i++) {
            zval_ptr_dtor(&bytecode->constants[i]);
        }
        efree(bytecode->constants);
    }
    if (bytecode->var_names) {
        for (size_t i = 0; i < bytecode->var_count; i++) {
            if (bytecode->var_names[i]) {
                efree(bytecode->var_names[i]);
            }
        }
        efree(bytecode->var_names);
    }
}
