// TODO: Windows support
#include <sys/stat.h>

#include "utils.h"

bool needs_recompilation(char *src_path, char *dest_path) {
  struct stat src_stat, dest_stat;

  if (stat(src_path, &src_stat) < 0)
    return true;

  if (stat(dest_path, &dest_stat) < 0)
    return true;

  return src_stat.st_mtime > dest_stat.st_mtime;
}

void make_directory(Str path, bool is_file_path) {
  u32 i = 0;

  while (i < path.len) {
    if (path.ptr[i] == '/') {
      char prev = path.ptr[i];
      path.ptr[i] = '\0';
      mkdir(path.ptr, 0777);
      path.ptr[i] = prev;
    }

    ++i;
  }

  if (!is_file_path && path.ptr[path.len - 1] != '/') {
    Str temp_path;
    temp_path.len = path.len;
    temp_path.ptr = malloc(temp_path.len + 1);
    memcpy(temp_path.ptr, path.ptr, temp_path.len);
    temp_path.ptr[temp_path.len] = '\0';
    mkdir(temp_path.ptr, 0777);
    free(temp_path.ptr);
  }
}

u32 str_find(Str str, Str sub) {
  if (str.len < sub.len)
    return (u32) -1;

  for (u32 i = 0; i < str.len - sub.len + 1; ++i)
    if (str_eq((Str) { str.ptr + i, sub.len }, sub))
      return i;

  return (u32) -1;
}

EModulePath merge_module_paths(EModulePath *a, EModulePath *b) {
  EModulePath result;
  result.len = a->len + b->len;
  result.cap = result.len;
  result.items = malloc(result.cap * sizeof(*result.items));
  memcpy(result.items, a->items, a->len * sizeof(*result.items));
  memcpy(result.items + a->len, b->items, b->len * sizeof(*result.items));
  return result;
}

EModulePath dup_module_path(EModulePath *path) {
  EModulePath result;
  result.len = path->len;
  result.cap = result.len;
  result.items = malloc(result.cap * sizeof(*result.items));
  memcpy(result.items, path->items, path->len * sizeof(*result.items));
  return result;
}
