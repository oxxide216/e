#ifndef DEPS_H
#define DEPS_H

#include "eir.h"
#include "parser.h"
#include "arena.h"

typedef Da(Str) Strs;

typedef struct {
  u64         hash;
  EModuleDep *dep;
} IncludedDep;

typedef Da(IncludedDep) IncludedDeps;

bool load_module_deps(EIr *ir, Arena *arena, Str input_path,
                      Str cache_path, Strs *include_paths,
                      IncludedDeps *included_deps,
                      EModulePath *module_path, Str obj_ext);

#endif // DEPS_H
