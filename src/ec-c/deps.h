#ifndef DEPS_H
#define DEPS_H

#include "eir.h"
#include "parser.h"
#include "arena.h"

typedef Da(Str) Strs;
typedef Da(u64) Hashes;

void varss_destroy(Varss *vars);
bool load_module_deps(EIr *ir, Arena *arena, Str input_path,
                      Str cache_path, Strs *include_paths,
                      Hashes *included_hashes);

#endif // DEPS_H
