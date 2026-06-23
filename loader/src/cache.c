#include "cache.h"
#include "crypto.h"

static HashTable phpshield_ir_cache;
static zend_bool phpshield_ir_cache_ready = 0;

void phpshield_cache_init(void)
{
  if (!phpshield_ir_cache_ready) {
    zend_hash_init(&phpshield_ir_cache, 32, NULL, ZVAL_PTR_DTOR, 0);
    phpshield_ir_cache_ready = 1;
  }
}

void phpshield_cache_shutdown(void)
{
  if (!phpshield_ir_cache_ready) return;
  zend_hash_destroy(&phpshield_ir_cache);
  phpshield_ir_cache_ready = 0;
}

zend_string *phpshield_cache_get(const char *key, size_t key_len)
{
  zval *value;
  if (!phpshield_ir_cache_ready) return NULL;
  value = zend_hash_str_find(&phpshield_ir_cache, key, key_len);
  if (!value || Z_TYPE_P(value) != IS_STRING) return NULL;
  return zend_string_init(Z_STRVAL_P(value), Z_STRLEN_P(value), 0);
}

void phpshield_cache_put(const char *key, size_t key_len, zend_string *ir)
{
  zval value;
  if (!phpshield_ir_cache_ready || !ir) return;
  ZVAL_STR_COPY(&value, ir);
  zend_hash_str_update(&phpshield_ir_cache, key, key_len, &value);
}
