#ifndef DEPS_H
#define DEPS_H

#include "eir.h"
#include "parser.h"
#include "arena.h"

void varss_destroy(Varss *vars);
bool load_module_deps(EIr *ir, Arena *arena, Str input_path, Str cache_path);

#endif // DEPS_H
