#ifndef PHPSHIELD_IR_H
#define PHPSHIELD_IR_H

#include "php.h"

int phpshield_ir_extract_php(zend_string *ir, zend_string **php_code);
int phpshield_ir_decode(zend_string *ir, zval *doc);

#endif
