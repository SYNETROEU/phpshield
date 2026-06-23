#ifndef PHPSHIELD_EXECUTE_H
#define PHPSHIELD_EXECUTE_H

#include "php.h"
#include "bundle.h"

void phpshield_custom_ir_init(void);
void phpshield_custom_ir_shutdown(void);
int phpshield_execute_ir(phpshield_segment *seg, zend_string **php_code);
int phpshield_execute_php(zend_string *php_code, const char *display_path, zval *retval);
int phpshield_execute_segment(phpshield_segment *seg, const char *stub_path, zend_bool debug, zval *retval);

#endif
