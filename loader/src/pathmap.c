#include "pathmap.h"
#include <string.h>

static void slashify(char *s)
{
  for (; *s; s++) {
    if (*s == '\\') *s = '/';
  }
}

zend_string *phpshield_relative_path(const char *stub_path, const char *bundle_path)
{
  char *stub = estrdup(stub_path);
  char *bundle = estrdup(bundle_path);
  char *last = NULL;
  size_t base_len;
  slashify(stub);
  slashify(bundle);
  last = strrchr(bundle, '/');
  if (!last) {
    efree(bundle);
    return zend_string_init(stub, strlen(stub), 0);
  }
  *last = '\0';
  base_len = strlen(bundle);
  if (strncmp(stub, bundle, base_len) == 0 && stub[base_len] == '/') {
    zend_string *rel = zend_string_init(stub + base_len + 1, strlen(stub + base_len + 1), 0);
    efree(stub);
    efree(bundle);
    return rel;
  }
  efree(bundle);
  return zend_string_init(stub, strlen(stub), 0);
}
