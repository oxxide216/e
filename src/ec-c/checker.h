#ifndef CHECKER_H
#define CHECKER_H

#include "eir.h"
#include "parser.h"

bool check_ir(EIr *ir, Varss *varss, Arena *arena, bool require_main);

#endif // CHECKER_H
