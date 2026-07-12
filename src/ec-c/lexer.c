#include <wchar.h>

#include "lexer.h"
#include "shl/shl-log.h"
#define LEXGEN_TRANSITION_TABLE_IMPLEMENTATION
#include "grammar.h"

Lexer lexer_create(Str code) {
  return (Lexer) { code, 0, 0, get_transition_table(), {} };
}

void lexer_destroy(Lexer *lexer) {
  if (lexer->temp_sb.buffer)
    free(lexer->temp_sb.buffer);
}

static char escape_char(Str *str, u32 *col) {
  char _char = str->ptr[0];

  switch (_char) {
  case 'n': return '\n';
  case 'r': return '\r';
  case 't': return '\t';
  case 'v': return '\v';
  case 'e': return '\e';
  case 'b': return '\b';
  case '0': return 0;
  case '\\': return '\\';

  case 'x': {
    char result = '\0';

    ++str->ptr;
    --str->len;
    ++*col;

    while (str->len > 0 &&
           ((str->ptr[0] >= '0' && str->ptr[0] <= '9') ||
            (str->ptr[0] >= 'a' && str->ptr[0] <= 'f') ||
            (str->ptr[0] >= 'A' && str->ptr[0] <= 'F'))) {
      result *= 16;

      if (str->ptr[0] >= '0' && str->ptr[0] <= '9')
        result += str->ptr[0] - '0';
      else if (str->ptr[0] >= 'a' && str->ptr[0] <= 'f')
        result += str->ptr[0] - 'a' + 10;
      else if (str->ptr[0] >= 'A' && str->ptr[0] <= 'F')
        result += str->ptr[0] - 'A' + 10;

      ++str->ptr;
      --str->len;
    }

    --str->ptr;
    ++str->len;
    ++*col;

    return result;
  }

  case 'd': {
    char result = '\0';

    ++str->ptr;
    --str->len;
    ++*col;

    while (str->len > 0 && str->ptr[0] >= '0' && str->ptr[0] <= '9') {
      result *= 10;

      if (str->ptr[0] >= '0' && str->ptr[0] <= '9')
        result += str->ptr[0] - '0';

      ++str->ptr;
      --str->len;
      ++*col;
    }

    --str->ptr;
    ++str->len;
    ++*col;

    return result;
  }

  case 'o': {
    char result = '\0';

    ++str->ptr;
    --str->len;
    ++*col;

    while (str->len > 0 && str->ptr[0] >= '0' && str->ptr[0] <= '7') {
      result *= 8;

      if (str->ptr[0] >= '0' && str->ptr[0] <= '7')
        result += str->ptr[0] - '0';

      ++str->ptr;
      --str->len;
      ++*col;
    }

    --str->ptr;
    ++str->len;
    ++*col;

    return result;
  }

  default: return _char;
  }
}

TokenStatus lexer_lex(Lexer *lexer, Token *token, Str file_path) {
  if (lexer->code.len > 0) {
    u64 id = 0;
    u32 char_len;
    Str lexeme = table_matches(lexer->table, &lexer->code, &id, &char_len);
    u16 row = lexer->row;
    u16 col = lexer->col;

    if (id == TT_NEWLINE) {
      ++lexer->row;
      lexer->col = 0;

      return TokenStatusEmpty;
    } else if (id == TT_COMMENT) {
      u32 next_len;
      wchar next;

      while ((next = get_next_wchar(lexer->code, 0, &next_len)) != U'\0' &&
             next != U'\n') {
        lexer->code.ptr += next_len;
        lexer->code.len -= next_len;
      }

      return TokenStatusEmpty;
    } else if (id == TT_WHITESPACE) {
      lexer->col += char_len;

      return TokenStatusEmpty;
    }

    if (id == (u64) -1) {
      u32 wchar_len;
      wchar _wchar = get_next_wchar(lexer->code, 0, &wchar_len);

      PERROR(STR_FMT":%u:%u: ", "Unexpected `%lc`\n", STR_ARG(file_path),
             lexer->row + 1, lexer->col + 1, (wint_t) _wchar);
      exit(1);
    }

    if (id == TT_STR) {
      sb_push_char(&lexer->temp_sb, lexer->code.ptr[-1]);

      bool is_escaped = false;
      while (lexer->code.len > 0 &&
             (lexer->code.ptr[0] != lexer->temp_sb.buffer[0] ||
              is_escaped)) {
        u32 next_len;
        wchar next = get_next_wchar(lexer->code, 0, &next_len);

        if (is_escaped || next != U'\\') {
          if (is_escaped) {
            sb_push_char(&lexer->temp_sb, escape_char(&lexer->code, &lexer->col));
          } else {
            for (u32 i = 0; i < next_len; ++i)
              sb_push_char(&lexer->temp_sb, lexer->code.ptr[i]);
          }
        }

        if (is_escaped)
          is_escaped = false;
        else if (next == U'\\')
          is_escaped = true;

        lexer->code.ptr += next_len;
        lexer->code.len -= next_len;
        ++lexer->col;
      }

      if (lexer->code.len == 0) {
        PERROR(STR_FMT":%u:%u: ", "String literal was not closed\n",
               STR_ARG(file_path), row + 1, col + 1);
        exit(1);
      }

      sb_push_char(&lexer->temp_sb, lexer->code.ptr[0]);

      ++lexer->code.ptr;
      --lexer->code.len;
      ++lexer->col;

      lexeme.len = lexer->temp_sb.len;
      lexeme.ptr = malloc(lexeme.len);
      memcpy(lexeme.ptr, lexer->temp_sb.buffer, lexeme.len);

      lexer->temp_sb.len = 0;
    } else {
      lexer->col += char_len;
    }

    *token = (Token) { id, lexeme, file_path, row, col };

    return TokenStatusOk;
  }

  return TokenStatusEOF;
}
