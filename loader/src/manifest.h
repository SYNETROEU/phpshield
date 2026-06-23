#ifndef PHPSHIELD_MANIFEST_H
#define PHPSHIELD_MANIFEST_H

#include "php.h"

int phpshield_json_decode_assoc(const char *json, size_t len, zval *out);
zend_string *phpshield_array_string(HashTable *ht, const char *key);
zend_long phpshield_array_long(HashTable *ht, const char *key);
zend_bool phpshield_array_bool(HashTable *ht, const char *key);

#endif
