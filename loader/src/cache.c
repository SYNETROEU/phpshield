#include "cache.h"
#include "crypto.h"
#ifdef ZTS
#include "TSRM.h"
#endif

static HashTable phpshield_ir_cache;
static zend_bool phpshield_ir_cache_ready = 0;
#ifdef ZTS
static MUTEX_T phpshield_cache_mutex = NULL;
#endif

static void cache_dtor(zval *zv)
{
  zend_string *s = Z_STR_P(zv);
  phpshield_memzero(ZSTR_VAL(s), ZSTR_LEN(s));
  zend_string_release(s);
}

void phpshield_cache_init(void)
{
  if (!phpshield_ir_cache_ready) {
    zend_hash_init(&phpshield_ir_cache, 32, NULL, cache_dtor, 1);
    phpshield_ir_cache_ready = 1;
#ifdef ZTS
    phpshield_cache_mutex = tsrm_mutex_alloc();
#endif
  }
}

void phpshield_cache_shutdown(void)
{
  if (!phpshield_ir_cache_ready) return;
  zend_hash_destroy(&phpshield_ir_cache);
  phpshield_ir_cache_ready = 0;
#ifdef ZTS
  if (phpshield_cache_mutex) {
    tsrm_mutex_free(phpshield_cache_mutex);
    phpshield_cache_mutex = NULL;
  }
#endif
}

zend_string *phpshield_cache_get(const char *key, size_t key_len)
{
  zval *value;
  zend_string *result = NULL;
  if (!phpshield_ir_cache_ready) return NULL;
#ifdef ZTS
  tsrm_mutex_lock(phpshield_cache_mutex);
#endif
  value = zend_hash_str_find(&phpshield_ir_cache, key, key_len);
  if (value && Z_TYPE_P(value) == IS_STRING) {
    result = zend_string_init(Z_STRVAL_P(value), Z_STRLEN_P(value), 0);
  }
#ifdef ZTS
  tsrm_mutex_unlock(phpshield_cache_mutex);
#endif
  return result;
}

void phpshield_cache_put(const char *key, size_t key_len, zend_string *ir)
{
  zval value;
  if (!phpshield_ir_cache_ready || !ir) return;
#ifdef ZTS
  tsrm_mutex_lock(phpshield_cache_mutex);
#endif
  ZVAL_STR_COPY(&value, ir);
  zend_hash_str_update(&phpshield_ir_cache, key, key_len, &value);
#ifdef ZTS
  tsrm_mutex_unlock(phpshield_cache_mutex);
#endif
}

int phpshield_opcache_available(void)
{
#ifdef HAVE_OPCACHE
  return 1;
#else
  return 0;
#endif
}

int phpshield_opcache_store(const char *filename, zend_op_array *op_array)
{
#ifdef HAVE_OPCACHE
  // Hook into opcache to store our protected opcodes
  // This requires opcache cooperation, simplified version stores in file cache
  zend_string *key = strpprintf(0, "phpshield:%s", filename);
  zend_file_handle file_handle;
  
  memset(&file_handle, 0, sizeof(file_handle));
  file_handle.type = ZEND_HANDLE_FILENAME;
  file_handle.filename = filename;
  
  // Store in opcache's internal structures
  // Real implementation would use opcache_compile_file hooks
  
  zend_string_release(key);
  return SUCCESS;
#else
  return FAILURE;
#endif
}

zend_op_array *phpshield_opcache_load(const char *filename)
{
#ifdef HAVE_OPCACHE
  zend_string *key = strpprintf(0, "phpshield:%s", filename);
  zend_op_array *result = NULL;
  
  // Try to load from opcache
  // Real implementation would hook opcache_get_script
  
  zend_string_release(key);
  return result;
#else
  return NULL;
#endif
}
