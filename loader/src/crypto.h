#ifndef PHPSHIELD_CRYPTO_H
#define PHPSHIELD_CRYPTO_H

#include "php.h"

typedef struct {
  unsigned char payload[32];
  unsigned char manifest[32];
  unsigned char license[32];
  unsigned char cache[32];
} phpshield_keys;

int phpshield_key_decode(const char *text, unsigned char out[32]);
int phpshield_derive_keys(const unsigned char master[32], phpshield_keys *keys);
int phpshield_derive_segment_key(const unsigned char payload_key[32], const char *path, unsigned char out[32]);
int phpshield_manifest_mac(const unsigned char key[32], const unsigned char *data, size_t data_len, unsigned char out[32]);
int phpshield_decrypt_segment(const char *alg, const unsigned char key[32], const unsigned char *nonce, size_t nonce_len, const unsigned char *tag, size_t tag_len, const unsigned char *ciphertext, size_t ciphertext_len, const char *aad, zend_string **plaintext);
void phpshield_memzero(void *p, size_t n);

#endif
