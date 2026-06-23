#ifndef PHPSHIELD_MEMCRYPT_H
#define PHPSHIELD_MEMCRYPT_H

#include <stddef.h>

// Encrypt buffer in-place using XOR with rolling key
void phpshield_memcrypt_encrypt(unsigned char *data, size_t len, unsigned char *key, size_t key_len);

// Decrypt buffer in-place (same as encrypt due to XOR)
void phpshield_memcrypt_decrypt(unsigned char *data, size_t len, unsigned char *key, size_t key_len);

// Generate random key for runtime encryption
void phpshield_memcrypt_gen_key(unsigned char *key, size_t len);

#endif
