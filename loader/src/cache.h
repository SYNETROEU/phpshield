#ifndef PHPSHIELD_CACHE_H
#define PHPSHIELD_CACHE_H

#include "php.h"

void phpshield_cache_init(void);
void phpshield_cache_shutdown(void);
zend_string *phpshield_cache_get(const char *key, size_t key_len);
void phpshield_cache_put(const char *key, size_t key_len, zend_string *ir);

#endif
