#ifndef PHPSHIELD_OPCODE_H
#define PHPSHIELD_OPCODE_H

#include "php.h"
#include "Zend/zend.h"
#include "Zend/zend_compile.h"

typedef struct {
  zend_op_array *op_array;
  int encrypted_strings;
  unsigned char *string_key;
} phpshield_opcode_bundle;

int phpshield_opcode_deserialize(zend_string *data, phpshield_opcode_bundle *bundle);
int phpshield_opcode_execute(phpshield_opcode_bundle *bundle, const char *filename, zval *retval);
void phpshield_opcode_bundle_free(phpshield_opcode_bundle *bundle);
int phpshield_opcode_decrypt_strings(zend_op_array *op_array, unsigned char *key);

#endif
