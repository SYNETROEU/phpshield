#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_phpshield.h"
#include "src/bundle.h"
#include "src/license.h"
#include "src/execute.h"
#include "src/anti_tamper.h"
#include "src/cache.h"
#include <string.h>

ZEND_DECLARE_MODULE_GLOBALS(phpshield)

PHP_INI_BEGIN()
  STD_PHP_INI_ENTRY("phpshield.bundle", "", PHP_INI_ALL, OnUpdateString, bundle, zend_phpshield_globals, phpshield_globals)
  STD_PHP_INI_ENTRY("phpshield.license", "", PHP_INI_ALL, OnUpdateString, license, zend_phpshield_globals, phpshield_globals)
  STD_PHP_INI_ENTRY("phpshield.key", "", PHP_INI_ALL, OnUpdateString, key, zend_phpshield_globals, phpshield_globals)
  STD_PHP_INI_BOOLEAN("phpshield.debug", "0", PHP_INI_ALL, OnUpdateBool, debug, zend_phpshield_globals, phpshield_globals)
  STD_PHP_INI_BOOLEAN("phpshield.allow_dev_eval", "0", PHP_INI_ALL, OnUpdateBool, allow_dev_eval, zend_phpshield_globals, phpshield_globals)
  STD_PHP_INI_BOOLEAN("phpshield.cache", "1", PHP_INI_ALL, OnUpdateBool, cache, zend_phpshield_globals, phpshield_globals)
  STD_PHP_INI_BOOLEAN("phpshield.strict", "1", PHP_INI_ALL, OnUpdateBool, strict, zend_phpshield_globals, phpshield_globals)
PHP_INI_END()

static void php_phpshield_init_globals(zend_phpshield_globals *g)
{
  g->bundle = NULL;
  g->license = NULL;
  g->key = NULL;
  g->debug = 0;
  g->allow_dev_eval = 0;
  g->cache = 1;
  g->strict = 1;
}

PHP_FUNCTION(phpshield_version)
{
  ZEND_PARSE_PARAMETERS_NONE();
  RETURN_STRING(PHP_SHIELD_VERSION);
}

PHP_FUNCTION(phpshield_license_info)
{
  ZEND_PARSE_PARAMETERS_NONE();
  array_init(return_value);
  add_assoc_bool(return_value, "configured", PHPSHIELD_G(license) && strlen(PHPSHIELD_G(license)) > 0);
  add_assoc_string(return_value, "status", PHPSHIELD_G(license) && strlen(PHPSHIELD_G(license)) > 0 ? "configured" : "not_configured");
}

PHP_FUNCTION(phpshield_load)
{
  char *path = NULL;
  size_t path_len = 0;
  phpshield_segment seg;
  zval retval;

  ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(path, path_len)
  ZEND_PARSE_PARAMETERS_END();

  if (!PHPSHIELD_G(bundle) || strlen(PHPSHIELD_G(bundle)) == 0) {
    zend_throw_error(NULL, "PHPShield bundle is not configured");
    RETURN_THROWS();
  }
  if (!PHPSHIELD_G(key) || strlen(PHPSHIELD_G(key)) == 0) {
    zend_throw_error(NULL, "PHPShield key is not configured");
    RETURN_THROWS();
  }
  if (PHPSHIELD_G(strict) && phpshield_anti_tamper_check(PHPSHIELD_G(debug)) != SUCCESS) {
    zend_throw_error(NULL, PHPSHIELD_G(debug) ? "PHPShield anti-tamper check failed" : "PHPShield load failed");
    RETURN_THROWS();
  }

  memset(&seg, 0, sizeof(seg));
  if (phpshield_bundle_load(path, PHPSHIELD_G(bundle), PHPSHIELD_G(key), PHPSHIELD_G(strict), PHPSHIELD_G(debug), PHPSHIELD_G(cache), &seg) != SUCCESS) {
    zend_throw_error(NULL, PHPSHIELD_G(debug) ? "PHPShield failed to verify or decrypt bundle segment" : "PHPShield load failed");
    RETURN_THROWS();
  }

  if (phpshield_license_verify(&seg, PHPSHIELD_G(license), PHPSHIELD_G(debug)) != SUCCESS) {
    phpshield_segment_free(&seg);
    zend_throw_error(NULL, PHPSHIELD_G(debug) ? "PHPShield license verification failed" : "PHPShield license rejected");
    RETURN_THROWS();
  }

  ZVAL_UNDEF(&retval);
  if (phpshield_execute_segment(&seg, path, PHPSHIELD_G(debug), &retval) != SUCCESS) {
    phpshield_segment_free(&seg);
    zend_throw_error(NULL, PHPSHIELD_G(debug) ? "PHPShield protected execution failed" : "PHPShield execution failed");
    RETURN_THROWS();
  }
  phpshield_segment_free(&seg);
  RETURN_ZVAL(&retval, 1, 1);
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_phpshield_load, 0, 1, IS_MIXED, 0)
  ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_phpshield_version, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_phpshield_license_info, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry phpshield_functions[] = {
  PHP_FE(phpshield_load, arginfo_phpshield_load)
  PHP_FE(phpshield_version, arginfo_phpshield_version)
  PHP_FE(phpshield_license_info, arginfo_phpshield_license_info)
  PHP_FE_END
};

PHP_MINIT_FUNCTION(phpshield)
{
  REGISTER_INI_ENTRIES();
  phpshield_anti_tamper_init();
  return SUCCESS;
}

PHP_RINIT_FUNCTION(phpshield)
{
  phpshield_cache_init();
  phpshield_custom_ir_init();
  return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(phpshield)
{
  phpshield_custom_ir_shutdown();
  phpshield_cache_shutdown();
  return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(phpshield)
{
  UNREGISTER_INI_ENTRIES();
  return SUCCESS;
}

PHP_MINFO_FUNCTION(phpshield)
{
  php_info_print_table_start();
  php_info_print_table_header(2, "PHPShield", "enabled");
  php_info_print_table_row(2, "Version", PHP_SHIELD_VERSION);
#ifdef PHPSHIELD_HAVE_SODIUM
  php_info_print_table_row(2, "libsodium", "enabled");
#else
  php_info_print_table_row(2, "libsodium", "not compiled");
#endif
#ifdef PHPSHIELD_HAVE_OPENSSL
  php_info_print_table_row(2, "OpenSSL", "enabled");
#else
  php_info_print_table_row(2, "OpenSSL", "not compiled");
#endif
  php_info_print_table_end();
  DISPLAY_INI_ENTRIES();
}

zend_module_entry phpshield_module_entry = {
  STANDARD_MODULE_HEADER,
  "phpshield",
  phpshield_functions,
  PHP_MINIT(phpshield),
  PHP_MSHUTDOWN(phpshield),
  PHP_RINIT(phpshield),
  PHP_RSHUTDOWN(phpshield),
  PHP_MINFO(phpshield),
  PHP_SHIELD_VERSION,
  STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_PHPSHIELD
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(phpshield)
#endif
