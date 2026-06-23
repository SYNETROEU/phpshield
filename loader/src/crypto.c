#include "crypto.h"
#include "ext/standard/base64.h"
#include <string.h>

#ifdef PHPSHIELD_HAVE_SODIUM
#include <sodium.h>
#endif
#ifdef PHPSHIELD_HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/buffer.h>
#endif

static const unsigned char hkdf_salt[] = "PHPShield-v1-HKDF-2024";

// White-box crypto: Hide key derivation through lookup tables
static unsigned char wbc_sbox[256];
static int wbc_initialized = 0;

static void phpshield_init_whitebox(void) {
    if (wbc_initialized) return;
    
    // Initialize S-box with randomized permutation
    for (int i = 0; i < 256; i++) {
        wbc_sbox[i] = (unsigned char)i;
    }
    
    // Fisher-Yates shuffle with fixed seed for reproducibility
    unsigned int seed = 0x12345678;
    for (int i = 255; i > 0; i--) {
        seed = seed * 1103515245 + 12345;
        int j = seed % (i + 1);
        unsigned char tmp = wbc_sbox[i];
        wbc_sbox[i] = wbc_sbox[j];
        wbc_sbox[j] = tmp;
    }
    
    wbc_initialized = 1;
}

static void phpshield_whitebox_transform(unsigned char *key, size_t len) {
    phpshield_init_whitebox();
    
    // Apply S-box transformation to hide key in memory
    for (size_t i = 0; i < len; i++) {
        key[i] = wbc_sbox[key[i]];
        key[i] ^= (unsigned char)(i & 0xFF); // Position-dependent mixing
    }
}

void phpshield_memzero(void *p, size_t n)
{
#ifdef PHPSHIELD_HAVE_SODIUM
  sodium_memzero(p, n);
#else
  volatile unsigned char *v = (volatile unsigned char *)p;
  while (n--) *v++ = 0;
#endif
}

int phpshield_secure_memcmp(const unsigned char *a, const unsigned char *b, size_t n)
{
#ifdef PHPSHIELD_HAVE_SODIUM
  return sodium_memcmp(a, b, n);
#else
  unsigned char diff = 0;
  for (size_t i = 0; i < n; i++) {
    diff |= a[i] ^ b[i];
  }
  return diff == 0 ? 0 : -1;
#endif
}

int phpshield_key_decode(const char *text, unsigned char out[32])
{
  zend_string *decoded;
  char *copy = estrdup(text);
  size_t len = strlen(copy);
  size_t clean_len;
  char *padded;
  for (size_t i = 0; i < len; i++) {
    if (copy[i] == '-') copy[i] = '+';
    if (copy[i] == '_') copy[i] = '/';
    if (copy[i] == '\r' || copy[i] == '\n' || copy[i] == ' ') copy[i] = '\0';
  }
  clean_len = strlen(copy);
  padded = emalloc(clean_len + 5);
  memcpy(padded, copy, clean_len);
  while (clean_len % 4 != 0) {
    padded[clean_len++] = '=';
  }
  padded[clean_len] = '\0';
  decoded = php_base64_decode((unsigned char *)padded, clean_len);
  efree(padded);
  efree(copy);
  if (!decoded || ZSTR_LEN(decoded) != 32) {
    if (decoded) zend_string_release(decoded);
    return FAILURE;
  }
  memcpy(out, ZSTR_VAL(decoded), 32);
  zend_string_release(decoded);
  return SUCCESS;
}

static int hkdf_expand_one(const unsigned char master[32], const char *info, unsigned char out[32])
{
#ifdef PHPSHIELD_HAVE_OPENSSL
  unsigned char prk[EVP_MAX_MD_SIZE];
  unsigned int prk_len = 0;
  unsigned char buf[128];
  unsigned int out_len = 0;
  // HKDF-Extract: PRK = HMAC(salt, IKM) where IKM is master key
  HMAC(EVP_sha256(), hkdf_salt, (int)strlen((const char *)hkdf_salt), master, 32, prk, &prk_len);
  // HKDF-Expand: OKM = HMAC(PRK, info || 0x01)
  size_t info_len = strlen(info);
  memcpy(buf, info, info_len);
  buf[info_len] = 1;
  HMAC(EVP_sha256(), prk, prk_len, buf, info_len + 1, out, &out_len);
  phpshield_memzero(prk, sizeof(prk));
  return out_len >= 32 ? SUCCESS : FAILURE;
#elif defined(PHPSHIELD_HAVE_SODIUM)
  crypto_auth_hmacsha256_state st;
  unsigned char prk[32];
  // HKDF-Extract: PRK = HMAC(salt, IKM) 
  crypto_auth_hmacsha256_init(&st, hkdf_salt, strlen((const char *)hkdf_salt));
  crypto_auth_hmacsha256_update(&st, master, 32);
  crypto_auth_hmacsha256_final(&st, prk);
  // HKDF-Expand: OKM = HMAC(PRK, info || 0x01)
  crypto_auth_hmacsha256_init(&st, prk, 32);
  crypto_auth_hmacsha256_update(&st, (const unsigned char *)info, strlen(info));
  unsigned char one = 1;
  crypto_auth_hmacsha256_update(&st, &one, 1);
  crypto_auth_hmacsha256_final(&st, out);
  phpshield_memzero(prk, sizeof(prk));
  return SUCCESS;
#else
  return FAILURE;
#endif
}

int phpshield_derive_keys(const unsigned char master[32], phpshield_keys *keys)
{
  return hkdf_expand_one(master, "payload-encryption", keys->payload) == SUCCESS &&
         hkdf_expand_one(master, "manifest-mac", keys->manifest) == SUCCESS &&
         hkdf_expand_one(master, "license-binding", keys->license) == SUCCESS &&
         hkdf_expand_one(master, "cache-key", keys->cache) == SUCCESS ? SUCCESS : FAILURE;
}

int phpshield_derive_segment_key(const unsigned char payload_key[32], const char *path, unsigned char out[32])
{
  unsigned char *buf;
  size_t prefix_len = strlen("segment-key");
  size_t path_len = strlen(path);
  size_t data_len = prefix_len + 1 + path_len;
  int rc;
  buf = emalloc(data_len);
  memcpy(buf, "segment-key", prefix_len);
  buf[prefix_len] = '\0';
  memcpy(buf + prefix_len + 1, path, path_len);
  rc = phpshield_manifest_mac(payload_key, buf, data_len, out);
  phpshield_memzero(buf, data_len);
  efree(buf);
  return rc;
}

int phpshield_manifest_mac(const unsigned char key[32], const unsigned char *data, size_t data_len, unsigned char out[32])
{
#ifdef PHPSHIELD_HAVE_OPENSSL
  unsigned int len = 0;
  HMAC(EVP_sha256(), key, 32, data, data_len, out, &len);
  return len == 32 ? SUCCESS : FAILURE;
#elif defined(PHPSHIELD_HAVE_SODIUM)
  crypto_auth_hmacsha256(out, data, data_len, key);
  return SUCCESS;
#else
  return FAILURE;
#endif
}

int phpshield_decrypt_segment(const char *alg, const unsigned char key[32], const unsigned char *nonce, size_t nonce_len, const unsigned char *tag, size_t tag_len, const unsigned char *ciphertext, size_t ciphertext_len, const char *aad, zend_string **plaintext)
{
  *plaintext = NULL;
#ifdef PHPSHIELD_HAVE_SODIUM
  if (strcmp(alg, "XCHACHA20-POLY1305") == 0) {
    unsigned long long plain_len = 0;
    zend_string *out = zend_string_alloc(ciphertext_len, 0);
    if (nonce_len != crypto_aead_xchacha20poly1305_ietf_NPUBBYTES) {
      zend_string_release(out);
      return FAILURE;
    }
    if (crypto_aead_xchacha20poly1305_ietf_decrypt((unsigned char *)ZSTR_VAL(out), &plain_len, NULL, ciphertext, ciphertext_len, (const unsigned char *)aad, strlen(aad), nonce, key) != 0) {
      zend_string_release(out);
      return FAILURE;
    }
    ZSTR_VAL(out)[plain_len] = '\0';
    ZSTR_LEN(out) = (size_t)plain_len;
    *plaintext = out;
    return SUCCESS;
  }
#endif
#ifdef PHPSHIELD_HAVE_OPENSSL
  if (strcmp(alg, "AES-256-GCM") == 0) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    int len = 0, plain_len = 0, ok = 0;
    zend_string *out;
    if (!ctx || nonce_len != 12 || tag_len != 16) {
      if (ctx) EVP_CIPHER_CTX_free(ctx);
      return FAILURE;
    }
    out = zend_string_alloc(ciphertext_len, 0);
    ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) &&
         EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)nonce_len, NULL) &&
         EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce);
    if (ok && aad && *aad) ok = EVP_DecryptUpdate(ctx, NULL, &len, (const unsigned char *)aad, (int)strlen(aad));
    if (ok) ok = EVP_DecryptUpdate(ctx, (unsigned char *)ZSTR_VAL(out), &len, ciphertext, (int)ciphertext_len);
    plain_len = len;
    if (ok) ok = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, (int)tag_len, (void *)tag);
    if (ok) ok = EVP_DecryptFinal_ex(ctx, (unsigned char *)ZSTR_VAL(out) + plain_len, &len) > 0;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok) {
      zend_string_release(out);
      return FAILURE;
    }
    plain_len += len;
    ZSTR_VAL(out)[plain_len] = '\0';
    ZSTR_LEN(out) = (size_t)plain_len;
    *plaintext = out;
    return SUCCESS;
  }
#endif
  return FAILURE;
}
