#include "opcode.h"
#include "crypto.h"
#include "manifest.h"
#include <string.h>

int phpshield_opcode_deserialize(zend_string *data, phpshield_opcode_bundle *bundle)
{
  zval decoded, *opcodes_data, *literals_data, *variables_data;
  zend_string *format;
  
  memset(bundle, 0, sizeof(*bundle));
  
  if (phpshield_json_decode_assoc(ZSTR_VAL(data), ZSTR_LEN(data), &decoded) != SUCCESS) {
    return FAILURE;
  }
  
  format = phpshield_array_string(Z_ARRVAL(decoded), "format");
  if (!format || strncmp(ZSTR_VAL(format), "phpshield_opcodes_v", 19) != 0) {
    zval_ptr_dtor(&decoded);
    return FAILURE;
  }
  
  opcodes_data = zend_hash_str_find(Z_ARRVAL(decoded), "opcodes", strlen("opcodes"));
  literals_data = zend_hash_str_find(Z_ARRVAL(decoded), "literals", strlen("literals"));
  variables_data = zend_hash_str_find(Z_ARRVAL(decoded), "variables", strlen("variables"));
  
  if (!opcodes_data || Z_TYPE_P(opcodes_data) != IS_ARRAY) {
    zval_ptr_dtor(&decoded);
    return FAILURE;
  }
  
  // Allocate op_array
  bundle->op_array = emalloc(sizeof(zend_op_array));
  memset(bundle->op_array, 0, sizeof(zend_op_array));
  
  bundle->op_array->type = ZEND_USER_FUNCTION;
  bundle->op_array->function_name = NULL;
  bundle->op_array->last = zend_hash_num_elements(Z_ARRVAL_P(opcodes_data));
  bundle->op_array->opcodes = emalloc(sizeof(zend_op) * bundle->op_array->last);
  
  // Deserialize opcodes
  uint32_t idx = 0;
  zval *op_data;
  ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(opcodes_data), op_data) {
    if (Z_TYPE_P(op_data) != IS_ARRAY || idx >= bundle->op_array->last) continue;
    
    zend_op *op = &bundle->op_array->opcodes[idx];
    memset(op, 0, sizeof(zend_op));
    
    zend_string *opcode_name = phpshield_array_string(Z_ARRVAL_P(op_data), "op");
    if (opcode_name) {
      // Map opcode names to Zend opcodes
      if (strcmp(ZSTR_VAL(opcode_name), "ZEND_RETURN") == 0) op->opcode = ZEND_RETURN;
      else if (strcmp(ZSTR_VAL(opcode_name), "ZEND_ECHO") == 0) op->opcode = ZEND_ECHO;
      else if (strcmp(ZSTR_VAL(opcode_name), "ZEND_NOP") == 0) op->opcode = ZEND_NOP;
      else op->opcode = ZEND_NOP;
    }
    
    op->lineno = (uint32_t)phpshield_array_long(Z_ARRVAL_P(op_data), "line");
    idx++;
  } ZEND_HASH_FOREACH_END();
  
  // Deserialize literals if present
  if (literals_data && Z_TYPE_P(literals_data) == IS_ARRAY) {
    bundle->op_array->last_literal = zend_hash_num_elements(Z_ARRVAL_P(literals_data));
    if (bundle->op_array->last_literal > 0) {
      bundle->op_array->literals = emalloc(sizeof(zval) * bundle->op_array->last_literal);
      // Populate literals...
    }
  }
  
  bundle->op_array->filename = NULL;
  bundle->op_array->line_start = 1;
  bundle->op_array->line_end = 1;
  bundle->encrypted_strings = 0;
  
  zval_ptr_dtor(&decoded);
  return SUCCESS;
}

int phpshield_opcode_decrypt_strings(zend_op_array *op_array, unsigned char *key)
{
  if (!op_array || !key) return FAILURE;
  
  // Decrypt all string literals in the opcode array
  for (uint32_t i = 0; i < op_array->last_literal; i++) {
    zval *lit = &op_array->literals[i];
    if (Z_TYPE_P(lit) == IS_STRING) {
      zend_string *encrypted = Z_STR_P(lit);
      zend_string *decrypted = NULL;
      
      // Simplified decryption - real impl would use proper AEAD
      if (ZSTR_LEN(encrypted) > 16) {
        unsigned char mac[32];
        phpshield_manifest_mac(key, (const unsigned char*)ZSTR_VAL(encrypted), ZSTR_LEN(encrypted), mac);
        
        decrypted = zend_string_init(ZSTR_VAL(encrypted), ZSTR_LEN(encrypted), 0);
        
        // XOR decrypt (placeholder for real crypto)
        for (size_t j = 0; j < ZSTR_LEN(decrypted); j++) {
          ZSTR_VAL(decrypted)[j] ^= key[j % 32];
        }
        
        zend_string_release(encrypted);
        ZVAL_STR(lit, decrypted);
      }
    }
  }
  
  return SUCCESS;
}

int phpshield_opcode_execute(phpshield_opcode_bundle *bundle, const char *filename, zval *retval)
{
  if (!bundle || !bundle->op_array) return FAILURE;
  
  // Set filename for stack traces
  if (filename) {
    bundle->op_array->filename = zend_string_init(filename, strlen(filename), 0);
  }
  
  // Decrypt strings if needed
  if (bundle->encrypted_strings && bundle->string_key) {
    phpshield_opcode_decrypt_strings(bundle->op_array, bundle->string_key);
    bundle->encrypted_strings = 0;
  }
  
  // Execute the op_array
  ZVAL_NULL(retval);
  zend_try {
    zend_execute(bundle->op_array, retval);
  } zend_catch {
    return FAILURE;
  } zend_end_try();
  
  return EG(exception) ? FAILURE : SUCCESS;
}

void phpshield_opcode_bundle_free(phpshield_opcode_bundle *bundle)
{
  if (!bundle) return;
  
  if (bundle->op_array) {
    if (bundle->op_array->opcodes) {
      efree(bundle->op_array->opcodes);
    }
    if (bundle->op_array->literals) {
      for (uint32_t i = 0; i < bundle->op_array->last_literal; i++) {
        zval_ptr_dtor(&bundle->op_array->literals[i]);
      }
      efree(bundle->op_array->literals);
    }
    if (bundle->op_array->vars) {
      efree(bundle->op_array->vars);
    }
    if (bundle->op_array->filename) {
      zend_string_release(bundle->op_array->filename);
    }
    destroy_op_array(bundle->op_array);
    efree(bundle->op_array);
  }
  
  if (bundle->string_key) {
    phpshield_memzero(bundle->string_key, 32);
    efree(bundle->string_key);
  }
  
  memset(bundle, 0, sizeof(*bundle));
}
