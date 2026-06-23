#include "manifest.h"
#include "ext/json/php_json.h"

int phpshield_json_decode_assoc(const char *json, size_t len, zval *out)
{
  zend_string *copy;
  zend_result result;
  ZVAL_NULL(out);
  copy = zend_string_init(json, len, 0);
  result = php_json_decode_ex(out, ZSTR_VAL(copy), ZSTR_LEN(copy), PHP_JSON_OBJECT_AS_ARRAY, 512);
  zend_string_release(copy);
  return result == SUCCESS && Z_TYPE_P(out) == IS_ARRAY ? SUCCESS : FAILURE;
}

zend_string *phpshield_array_string(HashTable *ht, const char *key)
{
  zval *v = zend_hash_str_find(ht, key, strlen(key));
  if (!v) return NULL;
  if (Z_TYPE_P(v) == IS_STRING) return Z_STR_P(v);
  return NULL;
}

zend_long phpshield_array_long(HashTable *ht, const char *key)
{
  zval *v = zend_hash_str_find(ht, key, strlen(key));
  if (!v) return 0;
  if (Z_TYPE_P(v) == IS_LONG) return Z_LVAL_P(v);
  if (Z_TYPE_P(v) == IS_STRING) return zend_atol(Z_STRVAL_P(v), Z_STRLEN_P(v));
  return 0;
}

zend_bool phpshield_array_bool(HashTable *ht, const char *key)
{
  zval *v = zend_hash_str_find(ht, key, strlen(key));
  if (!v) return 0;
  return zend_is_true(v);
}
