#ifndef LEXER_H
#define LEXER_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"
#include "lexgen/runtime.h"

typedef struct {
  Str              code;
  u32              row, col;
  TransitionTable *table;
  StringBuilder    temp_sb;
} Lexer;

typedef struct {
  u8  id;
  Str lexeme;
  Str file_path;
  u32 row, col;
} Token;

typedef Da(Token) Tokens;

typedef enum {
  TokenStatusOk = 0,
  TokenStatusEmpty,
  TokenStatusEOF,
} TokenStatus;

Lexer lexer_create(Str code);
void  lexer_destroy(Lexer *lexer);

TokenStatus lexer_lex(Lexer *lexer, Token *token, Str file_path);

#endif // LEXER_H
