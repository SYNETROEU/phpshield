#ifndef PHPSHIELD_BUNDLE_H
#define PHPSHIELD_BUNDLE_H

#include "php.h"

typedef struct {
  char *path;
  zend_string *ir;
  zend_bool license_required;
  char *product_id;
  unsigned char license_key[32];
  unsigned char license_public_key[32];
  zend_bool has_license_public_key;
} phpshield_segment;

int phpshield_bundle_load(const char *stub_path, const char *bundle_path, const char *key_text, zend_bool strict, zend_bool debug, zend_bool cache, phpshield_segment *out);
void phpshield_segment_free(phpshield_segment *seg);

#endif
