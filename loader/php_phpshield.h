#ifndef PHP_PHPSHIELD_H
#define PHP_PHPSHIELD_H

extern zend_module_entry phpshield_module_entry;
#define phpext_phpshield_ptr &phpshield_module_entry

#ifndef PHP_SHIELD_VERSION
#define PHP_SHIELD_VERSION "0.1.0"
#endif

ZEND_BEGIN_MODULE_GLOBALS(phpshield)
  char *bundle;
  char *license;
  char *key;
  zend_bool debug;
  zend_bool allow_dev_eval;
  zend_bool cache;
  zend_bool strict;
ZEND_END_MODULE_GLOBALS(phpshield)

ZEND_EXTERN_MODULE_GLOBALS(phpshield)
#define PHPSHIELD_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(phpshield, v)

#endif
