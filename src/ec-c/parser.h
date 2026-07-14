#ifndef PARSER_H
#define PARSER_H

#include "eir.h"
#include "lexer.h"

bool parse(EIr *ir, Varss *varss, Str code, Str file_path);

#endif // PARSER_H
