#ifndef PHPSHIELD_LICENSE_H
#define PHPSHIELD_LICENSE_H

#include "php.h"
#include "bundle.h"

int phpshield_license_verify(phpshield_segment *seg, const char *license_path, zend_bool debug);

#endif
