#include "ir.h"
#include "manifest.h"
#include "ext/standard/base64.h"
#include <string.h>

int phpshield_ir_decode(zend_string *ir, zval *doc)
{
  const char *json;
  size_t json_len;

  ZVAL_NULL(doc);
  if (ZSTR_LEN(ir) < 8 || memcmp(ZSTR_VAL(ir), "PSHIR1\n", 7) != 0) {
    return FAILURE;
  }
  json = ZSTR_VAL(ir) + 7;
  json_len = ZSTR_LEN(ir) - 7;
  return phpshield_json_decode_assoc(json, json_len, doc);
}

int phpshield_ir_extract_php(zend_string *ir, zend_string **php_code)
{
  zval doc;
  zend_string *b64, *strategy_name = NULL;
  zval *strategy;

  *php_code = NULL;
  if (phpshield_ir_decode(ir, &doc) != SUCCESS) {
    return FAILURE;
  }
  strategy = zend_hash_str_find(Z_ARRVAL(doc), "strategy", strlen("strategy"));
  if (strategy && Z_TYPE_P(strategy) == IS_ARRAY) {
    strategy_name = phpshield_array_string(Z_ARRVAL_P(strategy), "name");
  } else if (strategy && Z_TYPE_P(strategy) == IS_STRING) {
    strategy_name = Z_STR_P(strategy);
  }
  if (!strategy_name || strcmp(ZSTR_VAL(strategy_name), "zend_transitional_compile_string") != 0) {
    zval_ptr_dtor(&doc);
    return FAILURE;
  }
  b64 = phpshield_array_string(Z_ARRVAL(doc), "php");
  if (!b64) {
    zval_ptr_dtor(&doc);
    return FAILURE;
  }
  *php_code = php_base64_decode((unsigned char *)ZSTR_VAL(b64), ZSTR_LEN(b64));
  zval_ptr_dtor(&doc);
  return *php_code ? SUCCESS : FAILURE;
}
