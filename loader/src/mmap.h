#ifndef PHPSHIELD_MMAP_H
#define PHPSHIELD_MMAP_H

#include "php.h"

typedef struct {
  void *addr;
  size_t size;
  int fd;
} phpshield_mmap;

int phpshield_mmap_open(const char *path, phpshield_mmap *map);
void phpshield_mmap_close(phpshield_mmap *map);

#endif
