#include "mmap.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

int phpshield_mmap_open(const char *path, phpshield_mmap *map)
{
  memset(map, 0, sizeof(*map));
  
#ifdef _WIN32
  HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) return FAILURE;
  
  DWORD size = GetFileSize(hFile, NULL);
  HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
  if (!hMap) {
    CloseHandle(hFile);
    return FAILURE;
  }
  
  map->addr = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
  map->size = size;
  map->fd = (int)(intptr_t)hFile;
  CloseHandle(hMap);
  
  return map->addr ? SUCCESS : FAILURE;
#else
  map->fd = open(path, O_RDONLY);
  if (map->fd < 0) return FAILURE;
  
  struct stat st;
  if (fstat(map->fd, &st) < 0) {
    close(map->fd);
    return FAILURE;
  }
  
  map->size = st.st_size;
  map->addr = mmap(NULL, map->size, PROT_READ, MAP_PRIVATE, map->fd, 0);
  
  if (map->addr == MAP_FAILED) {
    close(map->fd);
    return FAILURE;
  }
  
  return SUCCESS;
#endif
}

void phpshield_mmap_close(phpshield_mmap *map)
{
  if (!map) return;
  
#ifdef _WIN32
  if (map->addr) UnmapViewOfFile(map->addr);
  if (map->fd) CloseHandle((HANDLE)(intptr_t)map->fd);
#else
  if (map->addr) munmap(map->addr, map->size);
  if (map->fd >= 0) close(map->fd);
#endif
  
  memset(map, 0, sizeof(*map));
}
