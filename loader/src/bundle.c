#include "bundle.h"
#include "crypto.h"
#include "manifest.h"
#include "pathmap.h"
#include "cache.h"
#include "ext/standard/base64.h"
#include "ext/hash/php_hash.h"
#include <string.h>
#include <stdint.h>

#define PHPSHIELD_MAGIC "PHPSHIELD1"
#define PHPSHIELD_HEADER_LEN 50

static int secure_memcmp32(const unsigned char a[32], const unsigned char b[32])
{
  unsigned char diff = 0;
  for (int i = 0; i < 32; i++) diff |= a[i] ^ b[i];
  return diff == 0;
}

static int stub_is_expected(const char *path)
{
  php_stream *stream = php_stream_open_wrapper((char *)path, "rb", 0, NULL);
  zend_string *contents;
  int ok;
  if (!stream) return 0;
  contents = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, 0);
  php_stream_close(stream);
  if (!contents) return 0;
  ok = strcmp(ZSTR_VAL(contents), "<?php\nreturn phpshield_load(__FILE__);\n") == 0 ||
       strcmp(ZSTR_VAL(contents), "<?php\r\nreturn phpshield_load(__FILE__);\r\n") == 0;
  zend_string_release(contents);
  return ok;
}

static zval *find_segment(zval *segments, zend_string *rel)
{
  zval *seg;
  if (!segments || Z_TYPE_P(segments) != IS_ARRAY) return NULL;
  ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(segments), seg) {
    zend_string *path;
    if (Z_TYPE_P(seg) != IS_ARRAY) continue;
    path = phpshield_array_string(Z_ARRVAL_P(seg), "path");
    if (path && zend_string_equals(path, rel)) return seg;
  } ZEND_HASH_FOREACH_END();
  return NULL;
}

static zend_bool manifest_license_required(zval *manifest)
{
  zval *license = zend_hash_str_find(Z_ARRVAL_P(manifest), "license", strlen("license"));
  if (!license || Z_TYPE_P(license) != IS_ARRAY) return 0;
  return phpshield_array_bool(Z_ARRVAL_P(license), "required");
}

static char *manifest_product_id(zval *manifest)
{
  zval *license = zend_hash_str_find(Z_ARRVAL_P(manifest), "license", strlen("license"));
  zend_string *product;
  if (!license || Z_TYPE_P(license) != IS_ARRAY) return NULL;
  product = phpshield_array_string(Z_ARRVAL_P(license), "product_id");
  return product ? estrndup(ZSTR_VAL(product), ZSTR_LEN(product)) : NULL;
}

static int manifest_license_public_key(zval *manifest, unsigned char out[32])
{
  zval *license = zend_hash_str_find(Z_ARRVAL_P(manifest), "license", strlen("license"));
  zend_string *key_b64;
  zend_string *decoded;
  if (!license || Z_TYPE_P(license) != IS_ARRAY) return FAILURE;
  key_b64 = phpshield_array_string(Z_ARRVAL_P(license), "ed25519_public_key");
  if (!key_b64) return FAILURE;
  decoded = php_base64_decode((unsigned char *)ZSTR_VAL(key_b64), ZSTR_LEN(key_b64));
  if (!decoded || ZSTR_LEN(decoded) != 32) {
    if (decoded) zend_string_release(decoded);
    return FAILURE;
  }
  memcpy(out, ZSTR_VAL(decoded), 32);
  zend_string_release(decoded);
  return SUCCESS;
}

int phpshield_bundle_load(const char *stub_path, const char *bundle_path, const char *key_text, zend_bool strict, zend_bool debug, zend_bool cache, phpshield_segment *out)
{
  php_stream *stream;
  zend_string *bundle = NULL, *rel = NULL, *nonce = NULL, *tag = NULL, *plain = NULL;
  zval manifest, *segments, *seg;
  unsigned char master[32], expected_mac[32], actual_mac[32];
  phpshield_keys keys;
  uint32_t manifest_len;
  const unsigned char *manifest_json, *payload;
  size_t payload_len;
  zend_long offset, length;
  zend_string *alg, *key_derivation, *nonce_b64, *tag_b64, *cipher_hash;
  zend_string *cache_key = NULL;
  unsigned char segment_key[32];

  if (debug) {
    php_printf("DEBUG: bundle_load starting\n");
    php_printf("DEBUG: stub=%s\n", stub_path);
    php_printf("DEBUG: bundle=%s\n", bundle_path);
  }

  if (strict && !stub_is_expected(stub_path)) {
    if (debug) php_error_docref(NULL, E_WARNING, "stub contents did not match expected PHPShield stub");
    return FAILURE;
  }
  memset(segment_key, 0, sizeof(segment_key));
  if (phpshield_key_decode(key_text, master) != SUCCESS) {
    if (debug) php_error_docref(NULL, E_WARNING, "configured key could not be decoded");
    return FAILURE;
  }
  if (phpshield_derive_keys(master, &keys) != SUCCESS) {
    if (debug) php_error_docref(NULL, E_WARNING, "derived bundle keys could not be generated");
    phpshield_memzero(master, sizeof(master));
    return FAILURE;
  }
  phpshield_memzero(master, sizeof(master));

  stream = php_stream_open_wrapper((char *)bundle_path, "rb", 0, NULL);
  if (!stream) {
    if (debug) php_error_docref(NULL, E_WARNING, "bundle file could not be opened: '%s'", bundle_path);
    phpshield_memzero(&keys, sizeof(keys));
    return FAILURE;
  }
  bundle = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, 0);
  php_stream_close(stream);
  if (!bundle || ZSTR_LEN(bundle) < PHPSHIELD_HEADER_LEN || memcmp(ZSTR_VAL(bundle), PHPSHIELD_MAGIC, 10) != 0) {
    if (debug) php_error_docref(NULL, E_WARNING, "bundle header is invalid");
    goto fail;
  }

  manifest_len = ((unsigned char)ZSTR_VAL(bundle)[14] << 24) | ((unsigned char)ZSTR_VAL(bundle)[15] << 16) | ((unsigned char)ZSTR_VAL(bundle)[16] << 8) | (unsigned char)ZSTR_VAL(bundle)[17];
  if (manifest_len > ZSTR_LEN(bundle) - PHPSHIELD_HEADER_LEN) {
    if (debug) php_error_docref(NULL, E_WARNING, "bundle manifest length exceeds bundle size");
    goto fail;
  }
  memcpy(expected_mac, ZSTR_VAL(bundle) + 18, 32);
  manifest_json = (const unsigned char *)ZSTR_VAL(bundle) + PHPSHIELD_HEADER_LEN;
  
  if (debug) {
    char first_bytes[41];
    snprintf(first_bytes, sizeof(first_bytes), "%.40s", manifest_json);
    php_error_docref(NULL, E_WARNING, "C: Manifest first bytes: %s", first_bytes);
    php_error_docref(NULL, E_WARNING, "C: Manifest len: %u", manifest_len);
    php_error_docref(NULL, E_WARNING, "C: Key bytes: %02x%02x%02x%02x",
      keys.manifest[0], keys.manifest[1], keys.manifest[2], keys.manifest[3]);
  }
  
  if (phpshield_manifest_mac(keys.manifest, manifest_json, manifest_len, actual_mac) != SUCCESS) {
    if (debug) php_error_docref(NULL, E_WARNING, "bundle manifest MAC could not be calculated");
    goto fail;
  }
  
  if (debug) {
    php_error_docref(NULL, E_WARNING, "Expected MAC: %02x%02x%02x%02x%02x%02x%02x%02x...",
      expected_mac[0], expected_mac[1], expected_mac[2], expected_mac[3],
      expected_mac[4], expected_mac[5], expected_mac[6], expected_mac[7]);
    php_error_docref(NULL, E_WARNING, "Actual MAC: %02x%02x%02x%02x%02x%02x%02x%02x...",
      actual_mac[0], actual_mac[1], actual_mac[2], actual_mac[3],
      actual_mac[4], actual_mac[5], actual_mac[6], actual_mac[7]);
  }
  
  if (!secure_memcmp32(expected_mac, actual_mac)) {
    if (debug) php_error_docref(NULL, E_WARNING, "bundle manifest MAC mismatch");
    goto fail;
  }
  
  if (phpshield_json_decode_assoc((const char *)manifest_json, manifest_len, &manifest) != SUCCESS) {
    if (debug) php_error_docref(NULL, E_WARNING, "bundle manifest JSON could not be decoded");
    goto fail;
  }

  rel = phpshield_relative_path(stub_path, bundle_path);
  segments = zend_hash_str_find(Z_ARRVAL(manifest), "segments", strlen("segments"));
  seg = find_segment(segments, rel);
  if (!seg) {
    if (debug) php_error_docref(NULL, E_WARNING, "bundle segment not found for path '%s'", ZSTR_VAL(rel));
    zval_ptr_dtor(&manifest);
    goto fail;
  }
  alg = phpshield_array_string(Z_ARRVAL_P(seg), "alg");
  key_derivation = phpshield_array_string(Z_ARRVAL_P(seg), "key_derivation");
  offset = phpshield_array_long(Z_ARRVAL_P(seg), "offset");
  length = phpshield_array_long(Z_ARRVAL_P(seg), "length");
  nonce_b64 = phpshield_array_string(Z_ARRVAL_P(seg), "nonce");
  tag_b64 = phpshield_array_string(Z_ARRVAL_P(seg), "tag");
  if (!nonce_b64 || !tag_b64) {
    if (debug) php_error_docref(NULL, E_WARNING, "bundle segment is missing nonce or tag metadata");
    zval_ptr_dtor(&manifest);
    goto fail;
  }
  nonce = php_base64_decode((unsigned char *)ZSTR_VAL(nonce_b64), ZSTR_LEN(nonce_b64));
  tag = ZSTR_LEN(tag_b64) == 0
    ? zend_string_init("", 0, 0)
    : php_base64_decode((unsigned char *)ZSTR_VAL(tag_b64), ZSTR_LEN(tag_b64));
  payload = (const unsigned char *)ZSTR_VAL(bundle) + PHPSHIELD_HEADER_LEN + manifest_len;
  payload_len = ZSTR_LEN(bundle) - PHPSHIELD_HEADER_LEN - manifest_len;
  cipher_hash = phpshield_array_string(Z_ARRVAL_P(seg), "ciphertext_sha256");
  if (!alg || !key_derivation || strcmp(ZSTR_VAL(key_derivation), "payload-hmac-sha256:path:v2") != 0 || !cipher_hash || !nonce || !tag || offset < 0 || length < 0 || (size_t)offset > payload_len || (size_t)length > payload_len - (size_t)offset) {
    if (debug) php_error_docref(NULL, E_WARNING, "bundle segment metadata is invalid");
    zval_ptr_dtor(&manifest);
    goto fail;
  }
  if (phpshield_derive_segment_key(keys.payload, ZSTR_VAL(rel), segment_key) != SUCCESS) {
    if (debug) php_error_docref(NULL, E_WARNING, "bundle segment key derivation failed for path '%s'", ZSTR_VAL(rel));
    zval_ptr_dtor(&manifest);
    goto fail;
  }

  if (cache) {
    unsigned char key_hash[32];
    phpshield_manifest_mac(keys.manifest, (const unsigned char*)bundle_path, strlen(bundle_path), key_hash);
    cache_key = strpprintf(0, "%.16s:%s:%s", key_hash, ZSTR_VAL(rel), ZSTR_VAL(cipher_hash));
    plain = phpshield_cache_get(ZSTR_VAL(cache_key), ZSTR_LEN(cache_key));
  }
  if (!plain) {
    if (phpshield_decrypt_segment(ZSTR_VAL(alg), segment_key, (const unsigned char *)ZSTR_VAL(nonce), ZSTR_LEN(nonce), (const unsigned char *)ZSTR_VAL(tag), ZSTR_LEN(tag), payload + offset, (size_t)length, ZSTR_VAL(rel), &plain) != SUCCESS) {
      if (debug) php_error_docref(NULL, E_WARNING, "bundle segment decrypt failed for path '%s' using '%s'", ZSTR_VAL(rel), ZSTR_VAL(alg));
      zval_ptr_dtor(&manifest);
      goto fail;
    }
    if (cache && cache_key) {
      phpshield_cache_put(ZSTR_VAL(cache_key), ZSTR_LEN(cache_key), plain);
    }
  }

  out->path = estrndup(ZSTR_VAL(rel), ZSTR_LEN(rel));
  out->ir = plain;
  out->license_required = manifest_license_required(&manifest);
  out->product_id = manifest_product_id(&manifest);
  memcpy(out->license_key, keys.license, 32);
  out->has_license_public_key = manifest_license_public_key(&manifest, out->license_public_key) == SUCCESS;
  zval_ptr_dtor(&manifest);
  if (nonce) zend_string_release(nonce);
  if (tag) zend_string_release(tag);
  if (rel) zend_string_release(rel);
  if (cache_key) zend_string_release(cache_key);
  if (bundle) zend_string_release(bundle);
  phpshield_memzero(segment_key, sizeof(segment_key));
  phpshield_memzero(&keys, sizeof(keys));
  return SUCCESS;

fail:
  if (nonce) zend_string_release(nonce);
  if (tag) zend_string_release(tag);
  if (plain) zend_string_release(plain);
  if (rel) zend_string_release(rel);
  if (cache_key) zend_string_release(cache_key);
  if (bundle) zend_string_release(bundle);
  phpshield_memzero(segment_key, sizeof(segment_key));
  phpshield_memzero(&keys, sizeof(keys));
  return FAILURE;
}

void phpshield_segment_free(phpshield_segment *seg)
{
  if (!seg) return;
  if (seg->path) efree(seg->path);
  if (seg->product_id) efree(seg->product_id);
  if (seg->ir) {
    phpshield_memzero(ZSTR_VAL(seg->ir), ZSTR_LEN(seg->ir));
    zend_string_release(seg->ir);
  }
  phpshield_memzero(seg->license_key, sizeof(seg->license_key));
  phpshield_memzero(seg->license_public_key, sizeof(seg->license_public_key));
  memset(seg, 0, sizeof(*seg));
}
