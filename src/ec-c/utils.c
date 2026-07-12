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
