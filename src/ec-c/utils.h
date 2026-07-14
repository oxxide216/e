#ifndef UTILS_H
#define UTILS_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"

bool needs_recompilation(char *src_path, char *dest_path);
void make_directory(Str path, bool is_file_path);

#endif // UTILS_H
