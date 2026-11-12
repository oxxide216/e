#ifndef UTILS_H
#define UTILS_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"
#include "eir.h"

bool        needs_recompilation(char *src_path, char *dest_path);
void        make_directory(Str path, bool is_file_path);
u32         str_find(Str str, Str sub);
EModulePath merge_module_paths(EModulePath *a, EModulePath *b);
EModulePath dup_module_path(EModulePath *path);

#endif // UTILS_H
