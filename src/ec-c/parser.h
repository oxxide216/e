#ifndef PARSER_H
#define PARSER_H

#include "eir.h"
#include "lexer.h"

typedef struct {
  Str       name;
  EType     type;
  bool      moved;
  EInstrLoc moved_loc;
} Var;

typedef Da(Var) Vars;
typedef Da(Vars) Varss;

bool parse(EIr *ir, Varss *varss, Str code, Str file_path);

#endif // PARSER_H
