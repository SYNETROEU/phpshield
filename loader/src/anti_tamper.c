#include "anti_tamper.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include "php.h"
#include <string.h>

#ifndef _WIN32
#include <stdio.h>
#endif

void phpshield_anti_tamper_init(void)
{
  /* Build-id and self-hash checks are packaging-time additions. Runtime debugger
     checks are enforced by phpshield_anti_tamper_check in strict mode. */
}

int phpshield_anti_tamper_check(int debug)
{
#ifdef _WIN32
  if (IsDebuggerPresent()) {
    if (debug) php_error_docref(NULL, E_WARNING, "PHPShield debugger check failed");
    return FAILURE;
  }
  return SUCCESS;
#else
  FILE *fp = fopen("/proc/self/status", "r");
  char line[256];
  if (!fp) return SUCCESS;
  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "TracerPid:", 10) == 0) {
      int pid = 0;
      sscanf(line + 10, "%d", &pid);
      fclose(fp);
      if (pid != 0) {
        if (debug) php_error_docref(NULL, E_WARNING, "PHPShield debugger check failed");
        return FAILURE;
      }
      return SUCCESS;
    }
  }
  fclose(fp);
  return SUCCESS;
#endif
}
