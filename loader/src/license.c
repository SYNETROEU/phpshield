#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#endif

#include "license.h"
#include "crypto.h"
#include "manifest.h"
#include "ext/standard/base64.h"
#include "SAPI.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#ifdef PHPSHIELD_HAVE_SODIUM
#include <sodium.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#endif

static int iso_to_time(const char *s, time_t *out)
{
  struct tm tm;
  memset(&tm, 0, sizeof(tm));
  if (!s || strlen(s) < 19) return FAILURE;
  if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) return FAILURE;
  tm.tm_year -= 1900;
  tm.tm_mon -= 1;
#ifdef _WIN32
  *out = _mkgmtime(&tm);
#else
  *out = timegm(&tm);
#endif
  return SUCCESS;
}

static int array_contains_string(zval *arr, const char *needle)
{
  zval *v;
  if (!arr || Z_TYPE_P(arr) != IS_ARRAY) return 0;
  ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(arr), v) {
    if (Z_TYPE_P(v) == IS_STRING && strcmp(Z_STRVAL_P(v), needle) == 0) return 1;
  } ZEND_HASH_FOREACH_END();
  return 0;
}

static int array_empty_or_contains(zval *arr, const char *needle)
{
  if (!arr || Z_TYPE_P(arr) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(arr)) == 0) return 1;
  return needle && array_contains_string(arr, needle);
}

static void lowercase_ascii(char *s)
{
  for (; s && *s; s++) *s = (char)tolower((unsigned char)*s);
}

static int host_name_matches(zval *domains)
{
  char host[256];
  char *dot;
  if (!domains || Z_TYPE_P(domains) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(domains)) == 0) return 1;
  if (gethostname(host, sizeof(host) - 1) != 0) return 0;
  host[sizeof(host) - 1] = '\0';
  lowercase_ascii(host);
  if (array_contains_string(domains, host)) return 1;
  dot = strchr(host, '.');
  return dot && array_contains_string(domains, dot + 1);
}

#ifdef _WIN32
static int registry_machine_guid_matches(zval *allowed)
{
  HKEY key;
  char value[256];
  DWORD type = REG_SZ;
  DWORD size = sizeof(value);
  int ok;
  if (!allowed || Z_TYPE_P(allowed) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(allowed)) == 0) return 1;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &key) != ERROR_SUCCESS) return 0;
  }
  if (RegQueryValueExA(key, "MachineGuid", NULL, &type, (LPBYTE)value, &size) != ERROR_SUCCESS || type != REG_SZ) {
    RegCloseKey(key);
    return 0;
  }
  RegCloseKey(key);
  value[sizeof(value) - 1] = '\0';
  ok = array_contains_string(allowed, value);
  lowercase_ascii(value);
  return ok || array_contains_string(allowed, value);
}

static int windows_mac_addresses_match(zval *allowed)
{
  ULONG size = 0;
  IP_ADAPTER_ADDRESSES *adapters = NULL;
  IP_ADAPTER_ADDRESSES *cur;
  DWORD rc;
  int ok = 0;
  if (!allowed || Z_TYPE_P(allowed) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(allowed)) == 0) return 1;
  rc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, NULL, &size);
  if (rc != ERROR_BUFFER_OVERFLOW || size == 0) return 0;
  adapters = (IP_ADAPTER_ADDRESSES *)emalloc(size);
  rc = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &size);
  if (rc != NO_ERROR) {
    efree(adapters);
    return 0;
  }
  for (cur = adapters; cur; cur = cur->Next) {
    char mac[32];
    if (cur->PhysicalAddressLength == 6) {
      snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
        cur->PhysicalAddress[0], cur->PhysicalAddress[1], cur->PhysicalAddress[2],
        cur->PhysicalAddress[3], cur->PhysicalAddress[4], cur->PhysicalAddress[5]);
      if (array_contains_string(allowed, mac)) {
        ok = 1;
        break;
      }
    }
  }
  efree(adapters);
  return ok;
}
#endif

static int file_trimmed_equals_any(const char *path, zval *allowed, int lowercase)
{
  php_stream *stream;
  zend_string *raw;
  char *value;
  size_t len;
  int ok;
  if (!allowed || Z_TYPE_P(allowed) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(allowed)) == 0) return 1;
  stream = php_stream_open_wrapper((char *)path, "rb", 0, NULL);
  if (!stream) return 0;
  raw = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, 0);
  php_stream_close(stream);
  if (!raw) return 0;
  value = estrndup(ZSTR_VAL(raw), ZSTR_LEN(raw));
  zend_string_release(raw);
  len = strlen(value);
  while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r' || value[len - 1] == ' ' || value[len - 1] == '\t')) value[--len] = '\0';
  if (lowercase) lowercase_ascii(value);
  ok = array_contains_string(allowed, value);
  efree(value);
  return ok;
}

static int mac_addresses_match(zval *allowed)
{
#ifdef _WIN32
  return windows_mac_addresses_match(allowed);
#else
  php_stream *dir;
  php_stream_dirent entry;
  int ok = 0;
  if (!allowed || Z_TYPE_P(allowed) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(allowed)) == 0) return 1;
  dir = php_stream_opendir("/sys/class/net", REPORT_ERRORS, NULL);
  if (!dir) return 0;
  while (php_stream_readdir(dir, &entry)) {
    char path[512];
    if (entry.d_name[0] == '.') continue;
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", entry.d_name);
    if (file_trimmed_equals_any(path, allowed, 1)) {
      ok = 1;
      break;
    }
  }
  php_stream_closedir(dir);
  return ok;
#endif
}

static int machine_fingerprint_matches(zval *allowed)
{
#ifdef _WIN32
  return registry_machine_guid_matches(allowed);
#else
  if (!allowed || Z_TYPE_P(allowed) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(allowed)) == 0) return 1;
  if (file_trimmed_equals_any("/etc/machine-id", allowed, 0)) return 1;
  if (file_trimmed_equals_any("/var/lib/dbus/machine-id", allowed, 0)) return 1;
  if (file_trimmed_equals_any("/var/db/dbus/machine-id", allowed, 0)) return 1;
  return 0;
#endif
}

static int ip_constraints_match(zval *allowed)
{
  zval *server;
  zval *addr;
  if (!allowed || Z_TYPE_P(allowed) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(allowed)) == 0) return 1;
  server = &PG(http_globals)[TRACK_VARS_SERVER];
  if (Z_TYPE_P(server) == IS_ARRAY) {
    addr = zend_hash_str_find(Z_ARRVAL_P(server), "SERVER_ADDR", strlen("SERVER_ADDR"));
    if (addr && Z_TYPE_P(addr) == IS_STRING && array_contains_string(allowed, Z_STRVAL_P(addr))) return 1;
    addr = zend_hash_str_find(Z_ARRVAL_P(server), "LOCAL_ADDR", strlen("LOCAL_ADDR"));
    if (addr && Z_TYPE_P(addr) == IS_STRING && array_contains_string(allowed, Z_STRVAL_P(addr))) return 1;
  }
  return 0;
}

static int verify_development_hmac(phpshield_segment *seg, zend_string *payload_json, zval *license)
{
  zend_string *sig_b64 = phpshield_array_string(Z_ARRVAL_P(license), "signature");
  zend_string *signature;
  unsigned char expected[32];
  int ok;
  if (!sig_b64) return FAILURE;
  signature = php_base64_decode((unsigned char *)ZSTR_VAL(sig_b64), ZSTR_LEN(sig_b64));
  if (!signature || ZSTR_LEN(signature) != 32) {
    if (signature) zend_string_release(signature);
    return FAILURE;
  }
  if (phpshield_manifest_mac(seg->license_key, (const unsigned char *)ZSTR_VAL(payload_json), ZSTR_LEN(payload_json), expected) != SUCCESS) {
    zend_string_release(signature);
    return FAILURE;
  }
  ok = phpshield_secure_memcmp(expected, (const unsigned char *)ZSTR_VAL(signature), 32);
  phpshield_memzero(expected, sizeof(expected));
  zend_string_release(signature);
  return ok == 0 ? SUCCESS : FAILURE;
}

static int verify_ed25519(phpshield_segment *seg, zend_string *payload_json, zval *license)
{
#ifdef PHPSHIELD_HAVE_SODIUM
  zend_string *sig_b64 = phpshield_array_string(Z_ARRVAL_P(license), "signature");
  zend_string *signature;
  int ok;
  if (!seg->has_license_public_key || !sig_b64) return FAILURE;
  signature = php_base64_decode((unsigned char *)ZSTR_VAL(sig_b64), ZSTR_LEN(sig_b64));
  if (!signature || ZSTR_LEN(signature) != crypto_sign_BYTES) {
    if (signature) zend_string_release(signature);
    return FAILURE;
  }
  ok = crypto_sign_verify_detached((const unsigned char *)ZSTR_VAL(signature), (const unsigned char *)ZSTR_VAL(payload_json), ZSTR_LEN(payload_json), seg->license_public_key);
  zend_string_release(signature);
  return ok == 0 ? SUCCESS : FAILURE;
#else
  return FAILURE;
#endif
}

int phpshield_license_verify(phpshield_segment *seg, const char *license_path, zend_bool debug)
{
  php_stream *stream;
  zend_string *raw, *payload_b64, *payload_json;
  zval license, payload;
  zval *allowed, *domains, *ips, *macs, *fingerprints;
  zend_string *product, *nb, *exp, *min_php, *max_php;
  zend_string *alg;
  time_t now = time(NULL), t;

  if (!seg->license_required) return SUCCESS;
  if (!license_path || strlen(license_path) == 0) return FAILURE;
  stream = php_stream_open_wrapper((char *)license_path, "rb", 0, NULL);
  if (!stream) return FAILURE;
  raw = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, 0);
  php_stream_close(stream);
  if (!raw) return FAILURE;
  if (phpshield_json_decode_assoc(ZSTR_VAL(raw), ZSTR_LEN(raw), &license) != SUCCESS) {
    zend_string_release(raw);
    return FAILURE;
  }
  payload_b64 = phpshield_array_string(Z_ARRVAL(license), "payload");
  if (!payload_b64) {
    zval_ptr_dtor(&license); zend_string_release(raw); return FAILURE;
  }
  payload_json = php_base64_decode((unsigned char *)ZSTR_VAL(payload_b64), ZSTR_LEN(payload_b64));
  if (!payload_json || phpshield_json_decode_assoc(ZSTR_VAL(payload_json), ZSTR_LEN(payload_json), &payload) != SUCCESS) {
    if (payload_json) zend_string_release(payload_json);
    zval_ptr_dtor(&license); zend_string_release(raw); return FAILURE;
  }
  alg = phpshield_array_string(Z_ARRVAL(license), "alg");
  if (!alg) {
    goto fail;
  }
  if (strcmp(ZSTR_VAL(alg), "Ed25519") == 0) {
    if (verify_ed25519(seg, payload_json, &license) != SUCCESS) goto fail;
  } else if (strcmp(ZSTR_VAL(alg), "HMAC-SHA256-DEVELOPMENT") == 0) {
    if (seg->has_license_public_key || verify_development_hmac(seg, payload_json, &license) != SUCCESS) goto fail;
  } else {
    goto fail;
  }
  product = phpshield_array_string(Z_ARRVAL(payload), "product_id");
  if (!product || !seg->product_id || strcmp(ZSTR_VAL(product), seg->product_id) != 0) goto fail;
  nb = phpshield_array_string(Z_ARRVAL(payload), "not_before");
  exp = phpshield_array_string(Z_ARRVAL(payload), "expires_at");
  if (iso_to_time(nb ? ZSTR_VAL(nb) : NULL, &t) != SUCCESS || now < t) goto fail;
  if (iso_to_time(exp ? ZSTR_VAL(exp) : NULL, &t) != SUCCESS || now > t) goto fail;
  allowed = zend_hash_str_find(Z_ARRVAL(payload), "allowed_sapis", strlen("allowed_sapis"));
  if (allowed && Z_TYPE_P(allowed) == IS_ARRAY && !array_contains_string(allowed, sapi_module.name)) goto fail;
  domains = zend_hash_str_find(Z_ARRVAL(payload), "domains", strlen("domains"));
  ips = zend_hash_str_find(Z_ARRVAL(payload), "ips", strlen("ips"));
  macs = zend_hash_str_find(Z_ARRVAL(payload), "mac_addresses", strlen("mac_addresses"));
  fingerprints = zend_hash_str_find(Z_ARRVAL(payload), "machine_fingerprints", strlen("machine_fingerprints"));
  if (!host_name_matches(domains)) goto fail;
  if (!ip_constraints_match(ips)) goto fail;
  if (!mac_addresses_match(macs)) goto fail;
  if (!machine_fingerprint_matches(fingerprints)) goto fail;
  min_php = phpshield_array_string(Z_ARRVAL(payload), "min_php_version");
  max_php = phpshield_array_string(Z_ARRVAL(payload), "max_php_version");
  if (min_php && php_version_compare(PHP_VERSION, ZSTR_VAL(min_php)) < 0) goto fail;
  if (max_php && php_version_compare(PHP_VERSION, ZSTR_VAL(max_php)) > 0) goto fail;
  zval_ptr_dtor(&payload); zend_string_release(payload_json); zval_ptr_dtor(&license); zend_string_release(raw);
  return SUCCESS;
fail:
  zval_ptr_dtor(&payload); zend_string_release(payload_json); zval_ptr_dtor(&license); zend_string_release(raw);
  return FAILURE;
}
