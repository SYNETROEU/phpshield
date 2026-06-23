#ifndef PHPSHIELD_PATHMAP_H
#define PHPSHIELD_PATHMAP_H

#include "php.h"

zend_string *phpshield_relative_path(const char *stub_path, const char *bundle_path);

#endif
