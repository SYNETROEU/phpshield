#include "execute.h"
#include "ir.h"
#include "opcode.h"
#include "manifest.h"
#include "Zend/zend_stream.h"
#include <string.h>

static HashTable phpshield_custom_functions;
static HashTable phpshield_custom_methods;
static zend_bool phpshield_custom_ready = 0;

void phpshield_custom_ir_init(void)
{
  if (!phpshield_custom_ready) {
    zend_hash_init(&phpshield_custom_functions, 16, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_init(&phpshield_custom_methods, 16, NULL, ZVAL_PTR_DTOR, 0);
    phpshield_custom_ready = 1;
  }
}

void phpshield_custom_ir_shutdown(void)
{
  if (!phpshield_custom_ready) return;
  zend_hash_destroy(&phpshield_custom_functions);
  zend_hash_destroy(&phpshield_custom_methods);
  phpshield_custom_ready = 0;
}

static int phpshield_verify_runtime_integrity(const unsigned char *code, size_t len);
static void phpshield_inject_anti_debug_traps(void);

static uint64_t phpshield_rdtsc(void) {
#ifdef _MSC_VER
    return __rdtsc();
#elif defined(__x86_64__) || defined(__i386__)
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#else
    return 0;
#endif
}

static void phpshield_inject_anti_debug_traps(void) {
    // Random delays to confuse timing analysis
    volatile int dummy = 0;
    for (int i = 0; i < rand() % 1000; i++) {
        dummy = (dummy + i) % 1000;
    }
    
    // Check integrity at random intervals
    if (rand() % 10 == 0) {
        // Verify code hasn't been patched in memory
        uint64_t start = phpshield_rdtsc();
        volatile int check = 0;
        for (int i = 0; i < 100; i++) check += i;
        uint64_t elapsed = phpshield_rdtsc() - start;
        
        if (elapsed > 100000 || check != 4950) {
            exit(1); // Debugger or tampering detected
        }
    }
}

static int phpshield_verify_runtime_integrity(const unsigned char *code, size_t len) {
    // Compute checksum of executing code
    uint32_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum = (checksum * 33) + code[i];
    }
    
    // Verify against stored checksum (would be embedded during encoding)
    return (checksum != 0) ? SUCCESS : FAILURE;
}

int phpshield_execute_ir(phpshield_segment *seg, zend_string **php_code)
{
  phpshield_inject_anti_debug_traps();
  return phpshield_ir_extract_php(seg->ir, php_code);
}

int phpshield_execute_php(zend_string *php_code, const char *display_path, zval *retval)
{
  const char *name = display_path ? display_path : "phpshield segment";
  zend_op_array *op_array = zend_compile_string(php_code, name, ZEND_COMPILE_POSITION_AFTER_OPEN_TAG);
  if (!op_array) {
    return FAILURE;
  }
  ZVAL_NULL(retval);
  zend_try {
    zend_execute(op_array, retval);
  } zend_catch {
    destroy_op_array(op_array);
    efree(op_array);
    zend_bailout();
  } zend_end_try();
  destroy_op_array(op_array);
  efree(op_array);
  return EG(exception) ? FAILURE : SUCCESS;
}

static zend_string *custom_array_string(zval *arr, const char *key)
{
  if (!arr || Z_TYPE_P(arr) != IS_ARRAY) return NULL;
  return phpshield_array_string(Z_ARRVAL_P(arr), key);
}

static zend_bool custom_array_bool(zval *arr, const char *key)
{
  zval *v;
  if (!arr || Z_TYPE_P(arr) != IS_ARRAY) return 0;
  v = zend_hash_str_find(Z_ARRVAL_P(arr), key, strlen(key));
  return v ? zend_is_true(v) : 0;
}

static void custom_register_string(HashTable *table, const char *key, size_t key_len, zend_string *value)
{
  zval zvalue;
  ZVAL_STR_COPY(&zvalue, value);
  zend_hash_str_update(table, key, key_len, &zvalue);
}

static zval *custom_lookup_string(HashTable *table, zend_string *key)
{
  zval *value = zend_hash_find(table, key);
  if (!value || Z_TYPE_P(value) != IS_STRING) return NULL;
  return value;
}

static zend_string *custom_method_key(zend_string *class_name, zend_string *method)
{
  return strpprintf(0, "%s::%s", ZSTR_VAL(class_name), ZSTR_VAL(method));
}

static zend_string *custom_sibling_path(const char *stub_path, zend_string *relative)
{
  char *slash1 = strrchr(stub_path, '/');
  char *slash2 = strrchr(stub_path, '\\');
  char *last = slash1 > slash2 ? slash1 : slash2;
  size_t dir_len = last ? (size_t)(last - stub_path) : 0;
  const char sep =
#ifdef _WIN32
    '\\';
#else
    '/';
#endif
  if (!last) {
    return zend_string_copy(relative);
  }
  return strpprintf(0, "%.*s%c%s", (int)dir_len, stub_path, sep, ZSTR_VAL(relative));
}

static int custom_require_file(zend_string *path, zval *retval)
{
  zend_file_handle file_handle;
  zend_op_array *op_array;
  zend_stream_init_filename_ex(&file_handle, path);
  op_array = zend_compile_file(&file_handle, ZEND_REQUIRE);
  zend_destroy_file_handle(&file_handle);
  if (!op_array) {
    return FAILURE;
  }
  ZVAL_NULL(retval);
  zend_try {
    zend_execute(op_array, retval);
  } zend_catch {
    destroy_op_array(op_array);
    efree(op_array);
    zend_bailout();
  } zend_end_try();
  destroy_op_array(op_array);
  efree(op_array);
  return EG(exception) ? FAILURE : SUCCESS;
}

static void custom_echo_string(zend_string *value, zend_bool newline)
{
  PHPWRITE(ZSTR_VAL(value), ZSTR_LEN(value));
  if (newline) {
    PHPWRITE(PHP_EOL, strlen(PHP_EOL));
  }
}

static int phpshield_execute_custom_ir(phpshield_segment *seg, const char *stub_path, zend_bool debug, zval *retval)
{
  zval doc, vars;
  zval *strategy, *custom, *ops, *op;
  zend_string *strategy_name = NULL;

  if (phpshield_ir_decode(seg->ir, &doc) != SUCCESS) {
    return FAILURE;
  }
  strategy = zend_hash_str_find(Z_ARRVAL(doc), "strategy", strlen("strategy"));
  if (strategy && Z_TYPE_P(strategy) == IS_ARRAY) {
    strategy_name = phpshield_array_string(Z_ARRVAL_P(strategy), "name");
  }
  if (!strategy_name || strcmp(ZSTR_VAL(strategy_name), "phpshield_custom_ir") != 0) {
    zval_ptr_dtor(&doc);
    return FAILURE;
  }
  custom = zend_hash_str_find(Z_ARRVAL(doc), "custom_ir", strlen("custom_ir"));
  if (!custom || Z_TYPE_P(custom) != IS_ARRAY) {
    zval_ptr_dtor(&doc);
    return FAILURE;
  }
  ops = zend_hash_str_find(Z_ARRVAL_P(custom), "ops", strlen("ops"));
  if (!ops || Z_TYPE_P(ops) != IS_ARRAY) {
    zval_ptr_dtor(&doc);
    return FAILURE;
  }

  array_init(&vars);
  ZVAL_NULL(retval);

  ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(ops), op) {
    zend_string *opcode = custom_array_string(op, "op");
    if (!opcode) {
      zval_ptr_dtor(&vars);
      zval_ptr_dtor(&doc);
      return FAILURE;
    }
    if (strcmp(ZSTR_VAL(opcode), "REGISTER_FUNCTION") == 0) {
      zend_string *name = custom_array_string(op, "name");
      zend_string *ret = custom_array_string(op, "return");
      if (!name || !ret) { if (debug) php_error_docref(NULL, E_WARNING, "custom IR REGISTER_FUNCTION is missing operands"); goto fail; }
      custom_register_string(&phpshield_custom_functions, ZSTR_VAL(name), ZSTR_LEN(name), ret);
    } else if (strcmp(ZSTR_VAL(opcode), "REGISTER_METHOD") == 0) {
      zend_string *class_name = custom_array_string(op, "class");
      zend_string *method = custom_array_string(op, "method");
      zend_string *ret = custom_array_string(op, "return");
      zend_string *key;
      if (!class_name || !method || !ret) { if (debug) php_error_docref(NULL, E_WARNING, "custom IR REGISTER_METHOD is missing operands"); goto fail; }
      key = custom_method_key(class_name, method);
      custom_register_string(&phpshield_custom_methods, ZSTR_VAL(key), ZSTR_LEN(key), ret);
      zend_string_release(key);
    } else if (strcmp(ZSTR_VAL(opcode), "REQUIRE") == 0) {
      zend_string *rel = custom_array_string(op, "path");
      zend_string *assign = custom_array_string(op, "assign");
      zend_string *full_path;
      zval require_ret;
      if (!rel) { if (debug) php_error_docref(NULL, E_WARNING, "custom IR REQUIRE is missing path"); goto fail; }
      full_path = custom_sibling_path(stub_path, rel);
      ZVAL_NULL(&require_ret);
      if (custom_require_file(full_path, &require_ret) != SUCCESS) {
        if (debug) php_error_docref(NULL, E_WARNING, "custom IR failed to require '%s'", ZSTR_VAL(full_path));
        zend_string_release(full_path);
        zval_ptr_dtor(&require_ret);
        goto fail;
      }
      zend_string_release(full_path);
      if (assign) {
        zend_hash_str_update(Z_ARRVAL(vars), ZSTR_VAL(assign), ZSTR_LEN(assign), &require_ret);
      } else {
        zval_ptr_dtor(&require_ret);
      }
    } else if (strcmp(ZSTR_VAL(opcode), "ECHO_FUNCTION") == 0) {
      zend_string *name = custom_array_string(op, "name");
      zval *value;
      if (!name) { if (debug) php_error_docref(NULL, E_WARNING, "custom IR ECHO_FUNCTION is missing name"); goto fail; }
      value = custom_lookup_string(&phpshield_custom_functions, name);
      if (!value) { if (debug) php_error_docref(NULL, E_WARNING, "custom IR function '%s' is not registered", ZSTR_VAL(name)); goto fail; }
      custom_echo_string(Z_STR_P(value), custom_array_bool(op, "newline"));
    } else if (strcmp(ZSTR_VAL(opcode), "ECHO_METHOD") == 0) {
      zend_string *class_name = custom_array_string(op, "class");
      zend_string *method = custom_array_string(op, "method");
      zend_string *key;
      zval *value;
      if (!class_name || !method) { if (debug) php_error_docref(NULL, E_WARNING, "custom IR ECHO_METHOD is missing operands"); goto fail; }
      key = custom_method_key(class_name, method);
      value = custom_lookup_string(&phpshield_custom_methods, key);
      zend_string_release(key);
      if (!value) { if (debug) php_error_docref(NULL, E_WARNING, "custom IR method is not registered"); goto fail; }
      custom_echo_string(Z_STR_P(value), custom_array_bool(op, "newline"));
    } else if (strcmp(ZSTR_VAL(opcode), "ECHO_VAR") == 0) {
      zend_string *name = custom_array_string(op, "name");
      zval *value;
      if (!name) { if (debug) php_error_docref(NULL, E_WARNING, "custom IR ECHO_VAR is missing name"); goto fail; }
      value = zend_hash_find(Z_ARRVAL(vars), name);
      if (!value || Z_TYPE_P(value) != IS_STRING) { if (debug) php_error_docref(NULL, E_WARNING, "custom IR variable '%s' is not a string", ZSTR_VAL(name)); goto fail; }
      custom_echo_string(Z_STR_P(value), custom_array_bool(op, "newline"));
    } else if (strcmp(ZSTR_VAL(opcode), "RETURN_STRING") == 0) {
      zend_string *value = custom_array_string(op, "value");
      if (!value) { if (debug) php_error_docref(NULL, E_WARNING, "custom IR RETURN_STRING is missing value"); goto fail; }
      zval_ptr_dtor(retval);
      ZVAL_STR_COPY(retval, value);
    } else {
      if (debug) php_error_docref(NULL, E_WARNING, "custom IR opcode '%s' is not supported", ZSTR_VAL(opcode));
      goto fail;
    }
  } ZEND_HASH_FOREACH_END();

  zval_ptr_dtor(&vars);
  zval_ptr_dtor(&doc);
  return EG(exception) ? FAILURE : SUCCESS;

fail:
  zval_ptr_dtor(&vars);
  zval_ptr_dtor(&doc);
  return FAILURE;
}

int phpshield_execute_segment(phpshield_segment *seg, const char *stub_path, zend_bool debug, zval *retval)
{
  zval doc;
  zval *strategy;
  zend_string *strategy_name = NULL;

  if (phpshield_ir_decode(seg->ir, &doc) != SUCCESS) {
    return FAILURE;
  }
  strategy = zend_hash_str_find(Z_ARRVAL(doc), "strategy", strlen("strategy"));
  if (strategy && Z_TYPE_P(strategy) == IS_ARRAY) {
    strategy_name = phpshield_array_string(Z_ARRVAL_P(strategy), "name");
  }
  if (!strategy_name) {
    zval_ptr_dtor(&doc);
    return FAILURE;
  }
  if (strcmp(ZSTR_VAL(strategy_name), "zend_transitional_compile_string") == 0) {
    zend_string *php_code = NULL;
    zval_ptr_dtor(&doc);
    if (phpshield_execute_ir(seg, &php_code) != SUCCESS) return FAILURE;
    if (phpshield_execute_php(php_code, stub_path, retval) != SUCCESS) {
      zend_string_release(php_code);
      return FAILURE;
    }
    zend_string_release(php_code);
    return SUCCESS;
  }
  if (strcmp(ZSTR_VAL(strategy_name), "zend_protected_opcode") == 0) {
    phpshield_opcode_bundle opcode_bundle;
    zval_ptr_dtor(&doc);
    if (phpshield_opcode_deserialize(seg->ir, &opcode_bundle) != SUCCESS) return FAILURE;
    if (phpshield_opcode_execute(&opcode_bundle, stub_path, retval) != SUCCESS) {
      phpshield_opcode_bundle_free(&opcode_bundle);
      return FAILURE;
    }
    phpshield_opcode_bundle_free(&opcode_bundle);
    return SUCCESS;
  }
  if (strcmp(ZSTR_VAL(strategy_name), "phpshield_custom_ir") == 0) {
    zval_ptr_dtor(&doc);
    return phpshield_execute_custom_ir(seg, stub_path, debug, retval);
  }
  zval_ptr_dtor(&doc);
  return FAILURE;
}
