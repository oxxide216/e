#include "parser.h"
#include "shl/shl-log.h"
#include "grammar.h"
#include "codegen.h"

#define MASK(id) (1lu << (id))

#define parser_expect_token(parser, mask, expected)   \
  do {                                                \
    parser_expect_token_impl(parser, mask, expected); \
    if ((parser)->has_error)                          \
      return;                                         \
  } while (0)

#define parser_parse_primary_expr(parser, dest_name, dest_index, is_const)   \
  do {                                                                       \
    parser_parse_primary_expr_impl(parser, dest_name, dest_index, is_const); \
    if ((parser)->has_error)                                                 \
      return;                                                                \
  } while (0)

#define parser_parse_unary_expr(parser, dest_name, dest_index)   \
  do {                                                           \
    parser_parse_unary_expr_impl(parser, dest_name, dest_index); \
    if ((parser)->has_error)                                     \
      return;                                                    \
  } while (0)

#define parser_parse_post_expr(parser, dest_name, dest_index)   \
  do {                                                          \
    parser_parse_post_expr_impl(parser, dest_name, dest_index); \
    if ((parser)->has_error)                                    \
      return;                                                   \
  } while (0)

#define parser_parse_mul(parser, dest_name, dest_index)   \
  do {                                                    \
    parser_parse_mul_impl(parser, dest_name, dest_index); \
    if ((parser)->has_error)                              \
      return;                                             \
  } while (0)

#define parser_parse_add(parser, dest_name, dest_index)   \
  do {                                                    \
    parser_parse_add_impl(parser, dest_name, dest_index); \
    if ((parser)->has_error)                              \
      return;                                             \
  } while (0)

#define parser_parse_shift(parser, dest_name, dest_index)   \
  do {                                                      \
    parser_parse_shift_impl(parser, dest_name, dest_index); \
    if ((parser)->has_error)                                \
      return;                                               \
  } while (0)

#define parser_parse_bit(parser, dest_name, dest_index)   \
  do {                                                    \
    parser_parse_bit_impl(parser, dest_name, dest_index); \
    if ((parser)->has_error)                              \
      return;                                             \
  } while (0)

#define parser_parse_cmp(parser, dest_name, dest_index)   \
  do {                                                    \
    parser_parse_cmp_impl(parser, dest_name, dest_index); \
    if ((parser)->has_error)                              \
      return;                                             \
  } while (0)

#define parser_parse_expr(parser, dest_name, dest_index) \
  parser_parse_cmp(parser, dest_name, dest_index)

#define parser_parse_stmt(parser)   \
  do {                              \
    parser_parse_stmt_impl(parser); \
    if ((parser)->has_error)        \
      return;                       \
  } while (0)

#define parser_parse_type(parser)   \
  do {                              \
    parser_parse_type_impl(parser); \
    if ((parser)->has_error)        \
      return;                       \
  } while (0)

#define parser_parse_proc(parser)   \
  do {                              \
    parser_parse_proc_impl(parser); \
    if ((parser)->has_error)        \
      return;                       \
  } while (0)

#define parser_parse_struct(parser)   \
  do {                                \
    parser_parse_struct_impl(parser); \
    if ((parser)->has_error)          \
      return;                         \
  } while (0)

typedef struct {
  EIr         *ir;
  Varss       *varss;
  EValue       last_const_value;
  EType        last_type;
  Lexer        lexer;
  TokenStatus  status;
  Token        token;
  Str          file_path;
  bool         has_error;
} Parser;

TokenStatus parser_peek_token(Parser *parser, Token *token) {
  if (token)
    *token = parser->token;
  return parser->status;
}

TokenStatus parser_next_token(Parser *parser, Token *token) {
  TokenStatus status = parser->status;
  if (token)
    *token = parser->token;
  while ((parser->status = lexer_lex(&parser->lexer,
                                     &parser->token,
                                     parser->file_path)) == TokenStatusEmpty);
  return status;
}

void parser_expect_token_impl(Parser *parser, u64 mask, char *expected) {
  Token token;
  TokenStatus status = parser_next_token(parser, &token);

  if (status == TokenStatusEOF) {
    PERROR(STR_FMT": ", "Unexpected EOF, expected %s\n",
           STR_ARG(parser->file_path), expected);
    parser->has_error = true;
    return;
  }

  if (!(MASK(token.id) & mask)) {
    PERROR(STR_FMT":%u:%u: ", "Unexpected `"STR_FMT"`, expected %s\n",
           STR_ARG(parser->file_path), token.row + 1,
           token.col + 1, STR_ARG(token.lexeme), expected);
    parser->has_error = true;
  }
}

static EType get_type_from_token(Token *name_token) {
  ETypeLoc loc = { name_token->file_path, name_token->row, name_token->col };
  if (str_eq(name_token->lexeme, STR_LIT("unit")))
    return (EType) { ETypeKindUnit, {}, loc };
  else if (str_eq(name_token->lexeme, STR_LIT("s8")))
    return (EType) { ETypeKindS8, {}, loc };
  else if (str_eq(name_token->lexeme, STR_LIT("s16")))
    return (EType) { ETypeKindS16, {}, loc };
  else if (str_eq(name_token->lexeme, STR_LIT("s32")))
    return (EType) { ETypeKindS32, {}, loc };
  else if (str_eq(name_token->lexeme, STR_LIT("s64")))
    return (EType) { ETypeKindS64, {}, loc };
  else if (str_eq(name_token->lexeme, STR_LIT("u8")))
    return (EType) { ETypeKindU8, {}, loc };
  else if (str_eq(name_token->lexeme, STR_LIT("u16")))
    return (EType) { ETypeKindU16, {}, loc };
  else if (str_eq(name_token->lexeme, STR_LIT("u32")))
    return (EType) { ETypeKindU32, {}, loc };
  else if (str_eq(name_token->lexeme, STR_LIT("u64")))
    return (EType) { ETypeKindU64, {}, loc };
  else if (str_eq(name_token->lexeme, STR_LIT("bool")))
    return (EType) { ETypeKindBool, {}, loc };
  else if (str_eq(name_token->lexeme, STR_LIT("str")))
    return (EType) { ETypeKindStr, {}, loc };
  else
    return (EType) { ETypeKindStruct, { .name = name_token->lexeme }, loc };
}

static void parser_parse_type_impl(Parser *parser) {
  Token token;
  parser_peek_token(parser, &token);
  parser_expect_token(parser,
                      MASK(TT_IDENT) | MASK(TT_AND) |
                      MASK(TT_OBRACE),
                      "identifier, `&` or `[`");

  if (token.id == TT_IDENT) {
    parser->last_type = get_type_from_token(&token);
  } else if (token.id == TT_AND) {
    parser_parse_type(parser);

    EType *ptr_target = malloc(sizeof(EType));
    *ptr_target = parser->last_type;
    ETypeLoc loc = { token.file_path, token.row, token.col };
    parser->last_type = (EType) {
      ETypeKindPtr,
      {
        .ptr_target = ptr_target,
      },
      loc,
    };
  } else {
    ETypeLoc loc = { token.file_path, token.row, token.col };

    parser_parse_type(parser);
    parser_expect_token(parser, MASK(TT_SEMI), "`;`");
    parser_peek_token(parser, &token);
    parser_expect_token(parser, MASK(TT_INT), "integer");
    parser_expect_token(parser, MASK(TT_CBRACE), "`]`");

    EType *array_element = malloc(sizeof(EType));
    *array_element = parser->last_type;
    parser->last_type = (EType) {
      ETypeKindArray,
      {
        .array_element = array_element,
        .array_len = str_to_u32(token.lexeme),
      },
      loc,
    };
  }
}

static u32 alloc_var(Parser *parser, Str name, Token token) {
  Vars *vars = parser->varss->items + parser->varss->len - 1;

  Var var = { name, {}, false, {}, };
  DA_APPEND(*vars, var);

  emit_instr(
    &parser->ir->procs,
    token,
    EInstrKindAlloc,
    .alloc = { name, vars->len - 1 },
  );

  return vars->len - 1;
}

static u32 get_var_index(Varss *varss, Str name) {
  Vars *vars = varss->items + varss->len - 1;

  for (u32 i = vars->len; i > 0; --i)
    if (str_eq(vars->items[i - 1].name, name))
      return i - 1;

  return (u32) -1;
}

static Var *get_var_by_index(Varss *varss, u32 index) {
  return varss->items[varss->len - 1].items + index;
}

static void backpatch_dest(EProc *proc, u32 starting_index, Str new_dest_name, u32 new_dest_index, u32 prev_dest_index) {
  for (u32 i = starting_index + 1; i > 0; --i) {
    EInstr *instr = proc->instrs.items + i - 1;

    switch (instr->kind) {
    case EInstrKindAlloc: {
      if (instr->as.alloc.index == prev_dest_index) {
        instr->as.alloc.name = new_dest_name;
        instr->as.alloc.index = new_dest_index;
      }
    } break;

    case EInstrKindStore: {
      if (instr->as.store.index == prev_dest_index) {
        instr->as.store.name = new_dest_name;
        instr->as.store.index = new_dest_index;
      }
    } break;

    case EInstrKindCopy: {
      if (instr->as.copy.dest_index == prev_dest_index) {
        instr->as.copy.dest_name = new_dest_name;
        instr->as.copy.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindBinOp: {
      if (instr->as.bin_op.dest_index == prev_dest_index) {
        instr->as.bin_op.dest_name = new_dest_name;
        instr->as.bin_op.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindCall: break;

    case EInstrKindCallAssign: {
      if (instr->as.call_assign.dest_index == prev_dest_index) {
        instr->as.call_assign.dest_name = new_dest_name;
        instr->as.call_assign.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindRet:       break;
    case EInstrKindRetVal:    break;
    case EInstrKindJump:      break;
    case EInstrKindJumpIfNot: break;

    case EInstrKindRef: {
      if (instr->as.ref.dest_index == prev_dest_index) {
        instr->as.ref.dest_name = new_dest_name;
        instr->as.ref.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindCopyToRef: {
      if (instr->as.copy_to_ref.dest_index == prev_dest_index) {
        instr->as.copy_to_ref.dest_name = new_dest_name;
        instr->as.copy_to_ref.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindCopyFromRef: {
      if (instr->as.copy_from_ref.dest_index == prev_dest_index) {
        instr->as.copy_from_ref.dest_name = new_dest_name;
        instr->as.copy_from_ref.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindStoreNull: {
      if (instr->as.store_null.index == prev_dest_index) {
        instr->as.store_null.name = new_dest_name;
        instr->as.store_null.index = new_dest_index;
      }
    } break;

    case EInstrKindInlineAsm: break;

    case EInstrKindStoreStr: {
      if (instr->as.store_str.index == prev_dest_index) {
        instr->as.store_str.name = new_dest_name;
        instr->as.store_str.index = new_dest_index;
      }
    } break;

    case EInstrKindCast: {
      if (instr->as.cast.dest_index == prev_dest_index) {
        instr->as.cast.dest_name = new_dest_name;
        instr->as.cast.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindLenOf: {
      if (instr->as.len_of.dest_index == prev_dest_index) {
        instr->as.len_of.dest_name = new_dest_name;
        instr->as.len_of.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindCopyToField: {
      if (instr->as.copy_to_field.dest_index == prev_dest_index) {
        instr->as.copy_to_field.dest_name = new_dest_name;
        instr->as.copy_to_field.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindCopyFromField: {
      if (instr->as.copy_from_field.dest_index == prev_dest_index) {
        instr->as.copy_from_field.dest_name = new_dest_name;
        instr->as.copy_from_field.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindTuple: {
      if (instr->as.tuple.dest_index == prev_dest_index) {
        instr->as.tuple.dest_name = new_dest_name;
        instr->as.tuple.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindCopyToOffset: {
      if (instr->as.copy_to_offset.dest_index == prev_dest_index) {
        instr->as.copy_to_offset.dest_name = new_dest_name;
        instr->as.copy_to_offset.dest_index = new_dest_index;
      }
    } break;

    case EInstrKindCopyFromOffset: {
      if (instr->as.copy_from_offset.dest_index == prev_dest_index) {
        instr->as.copy_from_offset.dest_name = new_dest_name;
        instr->as.copy_from_offset.dest_index = new_dest_index;
      }
    } break;
    }
  }
}

static void parser_parse_cmp_impl(Parser *parser, Str dest_name, u32 dest_index);

static void parser_parse_primary_expr_impl(Parser *parser, Str dest_name,
                                           u32 dest_index, bool is_const) {
  Token token;
  parser_peek_token(parser, &token);
  if (is_const)
    parser_expect_token(parser,
                        MASK(TT_INT) | MASK(TT_BOOL),
                        "int, float or bool");
  else
    parser_expect_token(parser,
                        MASK(TT_INT) | MASK(TT_BOOL) |
                        MASK(TT_STR) | MASK(TT_OPAREN) |
                        MASK(TT_OBRACE) | MASK(TT_IDENT) |
                        MASK(TT_NULL),
                        "int, bool, string, `(`, `[`, identifier or `null`");


  if (is_const) {
    switch (token.id) {
    case TT_INT: {
      parser->last_const_value = (EValue) {
        ETypeKindS64,
        { ._signed = str_to_i64(token.lexeme) },
      };
    } break;

    case TT_BOOL: {
      parser->last_const_value = (EValue) {
        ETypeKindBool,
        { ._bool = str_eq(token.lexeme, STR_LIT("true")) },
      };
    } break;
    }
  } else {
    switch (token.id) {
    case TT_INT: {
      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindStore,
        .store = {
          dest_name,
          dest_index,
          {
            ETypeKindS64,
            { ._signed = str_to_i64(token.lexeme) },
          }
        },
      );
    } break;

    case TT_BOOL: {
      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindStore,
        .store = {
          dest_name,
          dest_index,
          {
            ETypeKindBool,
            { ._bool = str_eq(token.lexeme, STR_LIT("true")) },
          }
        },
      );
    } break;

    case TT_STR: {
      u32 index = parser->ir->data.len;

      u8 *data = malloc(4 + token.lexeme.len - 1 + 1);
      *(u32 *) data = token.lexeme.len - 2;
      memcpy(data + 4, token.lexeme.ptr + 1, token.lexeme.len - 2);
      data[4 + token.lexeme.len - 2] = 0;
      data[4 + token.lexeme.len - 1] = 1;

      emit_data(&parser->ir->data, data, 4 + token.lexeme.len - 1 + 1);

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindStoreStr,
        .store_str = {
          dest_name,
          dest_index,
          index,
        },
      );
    } break;

    case TT_OPAREN: {
      Indices field_indices = {0};

      EProc *proc = parser->ir->procs.items + parser->ir->procs.len - 1;

      u32 alloc_index = proc->instrs.len;
      u32 temp0_index = alloc_var(parser, (Str) {0}, token);
      parser_parse_expr(parser, (Str) {0}, temp0_index);

      Token temp_token;
      if (parser_peek_token(parser, &temp_token) != TokenStatusEOF &&
          temp_token.id != TT_CPAREN) {

        DA_APPEND(field_indices, temp0_index);

        while (parser_peek_token(parser, &temp_token) != TokenStatusEOF &&
               temp_token.id != TT_CPAREN) {
          parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_CPAREN), "`,` or `)`");

          u32 temp1_index = alloc_var(parser, (Str) {0}, token);
          parser_parse_expr(parser, (Str) {0}, temp1_index);

          DA_APPEND(field_indices, temp1_index);

          if (parser_peek_token(parser, &temp_token) == TokenStatusEOF ||
              temp_token.id != TT_CPAREN)
          parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_CPAREN), "`,` or `)`");

        }
      }

      parser_expect_token(parser, MASK(TT_CPAREN), "`,` or `)`");

      if (field_indices.len == 0) {
        DA_REMOVE_AT(proc->instrs, alloc_index);
        backpatch_dest(proc, proc->instrs.len, dest_name, dest_index, temp0_index);
      } else {
        emit_instr(
          &parser->ir->procs,
          token,
          EInstrKindTuple,
          .tuple = {
            dest_name,
            dest_index,
            field_indices,
          },
        );

        for (u32 i = 0; i < field_indices.len; ++i) {
          emit_instr(
            &parser->ir->procs,
            temp_token,
            EInstrKindCopyToOffset,
            .copy_to_offset = {
              dest_name,
              dest_index,
              i,
              {},
              field_indices.items[i],
            },
          );
        }
      }
    } break;

    case TT_OBRACE: {
      parser_parse_type(parser);

      EType element_type = parser->last_type;

      Token temp_token;
      if (parser_peek_token(parser, &temp_token) != TokenStatusEOF &&
          temp_token.id != TT_SEMI) {
        parser_expect_token(parser, MASK(TT_SEMI) | MASK(TT_CBRACE), "`;` or `]`");

        parser->varss->items[parser->varss->len - 1].items[dest_index].type = (EType) {
          ETypeKindArray,
          {
            .array_element = malloc(sizeof(EType)),
            .array_len = 0,
          },
          {
            token.file_path,
            token.row,
            token.col,
          },
        };
        *parser->varss->items[parser->varss->len - 1].items[dest_index].type.array_element = element_type;

        break;
      }
      parser_expect_token(parser, MASK(TT_SEMI) | MASK(TT_CBRACE), "`;` or `]`");

      u32 i = 0;
      u32 index_index = alloc_var(parser, (Str) {0}, token);
      u32 element_index = alloc_var(parser, (Str) {0}, token);

      while (parser_peek_token(parser, &temp_token) != TokenStatusEOF &&
             temp_token.id != TT_CBRACE) {
        if (i > 0)
          parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_CBRACE), "`,` or `]`");

        parser_peek_token(parser, &temp_token);
        parser_parse_expr(parser, (Str) {0}, element_index);

        emit_instr(
          &parser->ir->procs,
          temp_token,
          EInstrKindStore,
          .store = {
            {},
            index_index,
            {
              ETypeKindU64,
              { ._unsigned = i },
            },
          },
        );

        emit_instr(
          &parser->ir->procs,
          temp_token,
          EInstrKindCopyToRef,
          .copy_to_ref = {
            dest_name,
            dest_index,
            true,
            {},
            index_index,
            {},
            element_index,
          },
        );

        ++i;

        if (parser_peek_token(parser, &temp_token) == TokenStatusEOF ||
            temp_token.id != TT_CBRACE)
          parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_CBRACE), "`,` or `]`");
      }

      parser_expect_token(parser, MASK(TT_CBRACE), "`,` or `]`");

      if (i == 0)
        parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len -= 2;

      parser->varss->items[parser->varss->len - 1].items[dest_index].type = (EType) {
        ETypeKindArray,
        {
          .array_element = malloc(sizeof(EType)),
          .array_len = i,
        },
        {
          token.file_path,
          token.row,
          token.col,
        },
      };
      *parser->varss->items[parser->varss->len - 1].items[dest_index].type.array_element = element_type;
    } break;

    case TT_IDENT: {
      Token name_token = token;
      if (parser_peek_token(parser, &token) != TokenStatusEOF &&
          token.id == TT_OPAREN) {
        parser_next_token(parser, NULL);

        Indices arg_indices = {0};

        while (parser_peek_token(parser, &token) != TokenStatusEOF &&
               token.id != TT_CPAREN) {
          u32 arg_index = alloc_var(parser, (Str) {0}, token);
          DA_APPEND(arg_indices, arg_index);

          parser_parse_expr(parser, (Str) {0}, arg_index);
          if (parser_peek_token(parser, &token) == TokenStatusEOF || token.id != TT_CPAREN)
            parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_CPAREN), "`,` or `)`");
        }

        emit_instr(
          &parser->ir->procs,
          name_token,
          EInstrKindCallAssign,
          .call_assign = {
            dest_name,
            dest_index,
            name_token.lexeme,
            arg_indices,
          },
        );

        parser_expect_token(parser, MASK(TT_CPAREN), "`)`");
      } else if (parser_peek_token(parser, &token) != TokenStatusEOF &&
                 token.id == TT_COLON) {
        parser_next_token(parser, NULL);

        u32 i = 0;
        u32 element_index = alloc_var(parser, (Str) {0}, token);

        Token temp_token;
        while (parser_peek_token(parser, &temp_token) != TokenStatusEOF &&
               temp_token.id != TT_END) {
          parser_peek_token(parser, &temp_token);
          parser_expect_token(parser, MASK(TT_IDENT), "identifier");
          parser_expect_token(parser, MASK(TT_SET), "`=`");
          parser_parse_expr(parser, (Str) {0}, element_index);

          emit_instr(
            &parser->ir->procs,
            temp_token,
            EInstrKindCopyToField,
            .copy_to_field = {
              dest_name,
              dest_index,
              temp_token.lexeme,
              {},
              element_index,
            },
          );

          ++i;

          if (parser_peek_token(parser, &temp_token) == TokenStatusEOF ||
              temp_token.id != TT_END)
            parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_END), "`,` or `end`");
        }

        parser_expect_token(parser, MASK(TT_END), "`,` or `end`");

        if (i == 0)
          --parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len;

        parser->varss->items[parser->varss->len - 1].items[dest_index].type = (EType) {
          ETypeKindStruct,
          {
            .name = name_token.lexeme,
          },
          {
            token.file_path,
            token.row,
            token.col,
          },
        };
      } else {
        emit_instr(
          &parser->ir->procs,
          name_token,
          EInstrKindCopy,
          .copy = {
            dest_name,
            dest_index,
            name_token.lexeme,
            get_var_index(parser->varss, name_token.lexeme),
            false,
          },
        );
      }
    } break;

    case TT_NULL: {
      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindStoreNull,
        .store_null = { dest_name, dest_index },
      );
    } break;
    }
  }
}

static void parser_parse_unary_expr_impl(Parser *parser, Str dest_name, u32 dest_index) {
  Token token;
  parser_peek_token(parser, &token);

  if (token.id == TT_AND || token.id == TT_STAR || token.id == TT_LENOF) {
    parser_next_token(parser, NULL);

    switch (token.id) {
    case TT_AND: {
      Token name_token;
      parser_peek_token(parser, &name_token);
      parser_expect_token(parser, MASK(TT_IDENT), "identifier");

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindRef,
        .ref = {
          dest_name,
          dest_index,
          name_token.lexeme,
          get_var_index(parser->varss, name_token.lexeme),
        },
      );
    } break;

    case TT_STAR: {
      u32 temp_index = alloc_var(parser, (Str) {0}, (Token) {});
      parser_parse_unary_expr(parser, (Str) {0}, temp_index);

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindCopyFromRef,
        .copy_from_ref = {
          dest_name,
          dest_index,
          {},
          temp_index,
          false,
          {},
          0,
        },
      );
    } break;

    case TT_LENOF: {
      u32 temp_index = alloc_var(parser, (Str) {0}, (Token) {});
      parser_parse_unary_expr(parser, (Str) {0}, temp_index);

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindLenOf,
        .len_of = {
          dest_name,
          dest_index,
          {},
          temp_index,
        },
      );
    } break;
    }
  } else {
    parser_parse_primary_expr(parser, dest_name, dest_index, false);
  }
}

static void parser_parse_post_expr_impl(Parser *parser, Str dest_name, u32 dest_index) {
  u32 alloc_index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len;
  u32 temp0_index = alloc_var(parser, (Str) {0}, (Token) {});
  parser_parse_unary_expr(parser, (Str) {0}, temp0_index);
  u32 index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len - 2;

  Token token;
  if (parser_peek_token(parser, &token) != TokenStatusEOF &&
      (token.id == TT_AS || token.id == TT_OBRACE || token.id == TT_DOT)) {
    parser_next_token(parser, NULL);

    index = (u32) -1;

    if (token.id == TT_AS) {
      parser_parse_type(parser);

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindCast,
        .cast = {
          dest_name,
          dest_index,
          parser->last_type,
          {},
          temp0_index,
        },
      );
    } else if (token.id == TT_OBRACE) {
      u32 temp1_index = alloc_var(parser, (Str) {0}, token);

      parser_parse_expr(parser, (Str) {0}, temp1_index);
      parser_expect_token(parser, MASK(TT_CBRACE), "`]`");

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindCopyFromRef,
        .copy_from_ref = {
          dest_name,
          dest_index,
          {},
          temp0_index,
          true,
          {},
          temp1_index,
        },
      );
    } else if (token.id == TT_DOT) {
      Token field_name_token;
      parser_peek_token(parser, &field_name_token);
      parser_expect_token(parser, MASK(TT_IDENT) | MASK(TT_INT), "identifier or integer");

      if (field_name_token.id == TT_IDENT) {
        emit_instr(
          &parser->ir->procs,
          token,
          EInstrKindCopyFromField,
          .copy_from_field = {
            dest_name,
            dest_index,
            {},
            temp0_index,
            field_name_token.lexeme,
          },
        );
      } else {
        emit_instr(
          &parser->ir->procs,
          token,
          EInstrKindCopyFromOffset,
          .copy_from_offset = {
            dest_name,
            dest_index,
            {},
            temp0_index,
            str_to_u32(field_name_token.lexeme),
          },
        );
      }
    }
  }

  if (index != (u32) -1) {
    DA_REMOVE_AT(parser->ir->procs.items[parser->ir->procs.len - 1].instrs, alloc_index);
    backpatch_dest(parser->ir->procs.items + parser->ir->procs.len - 1,
                   index, dest_name, dest_index, temp0_index);
    Vars *vars = parser->varss->items + parser->varss->len - 1;
    vars->items[dest_index].type = vars->items[temp0_index].type;
  }
}

static void parser_parse_mul_impl(Parser *parser, Str dest_name, u32 dest_index) {
  u32 alloc_index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len;
  u32 temp0_index = alloc_var(parser, (Str) {0}, (Token) {});
  parser_parse_post_expr(parser, (Str) {0}, temp0_index);
  u32 index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len - 2;

  Token token;
  u32 temp1_index = (u32) -1;
  while (parser_peek_token(parser, &token) != TokenStatusEOF &&
         (token.id == TT_STAR || token.id == TT_SLASH || token.id == TT_PERC)) {
    parser_next_token(parser, NULL);

    index = (u32) -1;
    if (temp1_index == (u32) -1)
      temp1_index = alloc_var(parser, (Str) {0}, token);

    parser_parse_post_expr(parser, (Str) {0}, temp1_index);

    emit_instr(
      &parser->ir->procs,
      token,
      EInstrKindBinOp,
      .bin_op = {
        {},
        temp0_index,
        {},
        temp0_index,
        {},
        temp1_index,
        token.id == TT_STAR ? EBinOpKindMul :
          token.id == TT_SLASH ? EBinOpKindDiv : EBinOpKindRem,
      },
    );
  }

  if (index == (u32) -1) {
    emit_instr(
      &parser->ir->procs,
      (Token) {},
      EInstrKindCopy,
      .copy = {
        dest_name,
        dest_index,
        {},
        temp0_index,
        false,
      },
    );
  } else {
    DA_REMOVE_AT(parser->ir->procs.items[parser->ir->procs.len - 1].instrs, alloc_index);
    backpatch_dest(parser->ir->procs.items + parser->ir->procs.len - 1,
                   index, dest_name, dest_index, temp0_index);
    Vars *vars = parser->varss->items + parser->varss->len - 1;
    vars->items[dest_index].type = vars->items[temp0_index].type;
  }
}

static void parser_parse_add_impl(Parser *parser, Str dest_name, u32 dest_index) {
  u32 alloc_index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len;
  u32 temp0_index = alloc_var(parser, (Str) {0}, (Token) {});
  parser_parse_mul(parser, (Str) {0}, temp0_index);
  u32 index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len - 2;

  Token token;
  u32 temp1_index = (u32) -1;
  while (parser_peek_token(parser, &token) != TokenStatusEOF &&
         (token.id == TT_PLUS || token.id == TT_MINUS)) {
    parser_next_token(parser, NULL);

    index = (u32) -1;
    if (temp1_index == (u32) -1)
      temp1_index = alloc_var(parser, (Str) {0}, token);

    parser_parse_mul(parser, (Str) {0}, temp1_index);

    emit_instr(
      &parser->ir->procs,
      token,
      EInstrKindBinOp,
      .bin_op = {
        {},
        temp0_index,
        {},
        temp0_index,
        {},
        temp1_index,
        token.id == TT_PLUS ? EBinOpKindAdd : EBinOpKindSub,
      },
    );
  }

  if (index == (u32) -1) {
    emit_instr(
      &parser->ir->procs,
      (Token) {},
      EInstrKindCopy,
      .copy = {
        dest_name,
        dest_index,
        {},
        temp0_index,
        false,
      },
    );
  } else {
    DA_REMOVE_AT(parser->ir->procs.items[parser->ir->procs.len - 1].instrs, alloc_index);
    backpatch_dest(parser->ir->procs.items + parser->ir->procs.len - 1,
                   index, dest_name, dest_index, temp0_index);
    Vars *vars = parser->varss->items + parser->varss->len - 1;
    vars->items[dest_index].type = vars->items[temp0_index].type;
  }
}

static void parser_parse_shift_impl(Parser *parser, Str dest_name, u32 dest_index) {
  u32 alloc_index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len;
  u32 temp0_index = alloc_var(parser, (Str) {0}, (Token) {});
  parser_parse_add(parser, (Str) {0}, temp0_index);
  u32 index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len - 2;

  Token token;
  u32 temp1_index = (u32) -1;
  while (parser_peek_token(parser, &token) != TokenStatusEOF &&
         (token.id == TT_LSHIFT || token.id == TT_RSHIFT)) {
    parser_next_token(parser, NULL);

    index = (u32) -1;
    if (temp1_index == (u32) -1)
      temp1_index = alloc_var(parser, (Str) {0}, token);

    parser_parse_add(parser, (Str) {0}, temp1_index);

    emit_instr(
      &parser->ir->procs,
      token,
      EInstrKindBinOp,
      .bin_op = {
        {},
        temp0_index,
        {},
        temp0_index,
        {},
        temp1_index,
        token.id == TT_LSHIFT ? EBinOpKindLShift : EBinOpKindRShift,
      },
    );
  }

  if (index == (u32) -1) {
    emit_instr(
      &parser->ir->procs,
      (Token) {},
      EInstrKindCopy,
      .copy = {
        dest_name,
        dest_index,
        {},
        temp0_index,
        false,
      },
    );
  } else {
    DA_REMOVE_AT(parser->ir->procs.items[parser->ir->procs.len - 1].instrs, alloc_index);
    backpatch_dest(parser->ir->procs.items + parser->ir->procs.len - 1,
                   index, dest_name, dest_index, temp0_index);
    Vars *vars = parser->varss->items + parser->varss->len - 1;
    vars->items[dest_index].type = vars->items[temp0_index].type;
  }
}

static void parser_parse_bit_impl(Parser *parser, Str dest_name, u32 dest_index) {
  u32 alloc_index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len;
  u32 temp0_index = alloc_var(parser, (Str) {0}, (Token) {});
  parser_parse_shift(parser, (Str) {0}, temp0_index);
  u32 index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len - 2;

  Token token;
  u32 temp1_index = (u32) -1;
  while (parser_peek_token(parser, &token) != TokenStatusEOF &&
         (token.id == TT_AND || token.id == TT_OR || token.id == TT_XOR)) {
    parser_next_token(parser, NULL);

    index = (u32) -1;
    if (temp1_index == (u32) -1)
      temp1_index = alloc_var(parser, (Str) {0}, token);

    parser_parse_shift(parser, (Str) {0}, temp1_index);

    emit_instr(
      &parser->ir->procs,
      token,
      EInstrKindBinOp,
      .bin_op = {
        {},
        temp0_index,
        {},
        temp0_index,
        {},
        temp1_index,
        token.id == TT_AND ? EBinOpKindAnd :
          token.id == TT_OR ? EBinOpKindOr : EBinOpKindXor,
      },
    );
  }

  if (index == (u32) -1) {
    emit_instr(
      &parser->ir->procs,
      (Token) {},
      EInstrKindCopy,
      .copy = {
        dest_name,
        dest_index,
        {},
        temp0_index,
        false,
      },
    );
  } else {
    DA_REMOVE_AT(parser->ir->procs.items[parser->ir->procs.len - 1].instrs, alloc_index);
    backpatch_dest(parser->ir->procs.items + parser->ir->procs.len - 1,
                   index, dest_name, dest_index, temp0_index);
    Vars *vars = parser->varss->items + parser->varss->len - 1;
    vars->items[dest_index].type = vars->items[temp0_index].type;
  }
}

static void parser_parse_cmp_impl(Parser *parser, Str dest_name, u32 dest_index) {
  u32 alloc_index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len;
  u32 temp0_index = alloc_var(parser, (Str) {0}, (Token) {});
  parser_parse_bit(parser, (Str) {0}, temp0_index);
  u32 index = parser->ir->procs.items[parser->ir->procs.len - 1].instrs.len - 2;

  Token token;
  if (parser_peek_token(parser, &token) != TokenStatusEOF &&
      (token.id == TT_EQ || token.id == TT_NE ||
       token.id == TT_LS || token.id == TT_LE ||
       token.id == TT_GT || token.id == TT_GE)) {
    parser_next_token(parser, NULL);

    index = (u32) -1;
    u32 temp1_index = alloc_var(parser, (Str) {0}, token);

    parser_parse_bit(parser, (Str) {0}, temp1_index);

    static EBinOpKind cmp_ops[] = {
      [TT_EQ] = EBinOpKindEq,
      [TT_NE] = EBinOpKindNe,
      [TT_LS] = EBinOpKindLs,
      [TT_LE] = EBinOpKindLe,
      [TT_GT] = EBinOpKindGt,
      [TT_GE] = EBinOpKindGe,
    };

    emit_instr(
      &parser->ir->procs,
      token,
      EInstrKindBinOp,
      .bin_op = {
        dest_name,
        dest_index,
        {},
        temp0_index,
        {},
        temp1_index,
        cmp_ops[token.id],
      },
    );
  }

  if (index != (u32) -1) {
    DA_REMOVE_AT(parser->ir->procs.items[parser->ir->procs.len - 1].instrs, alloc_index);
    backpatch_dest(parser->ir->procs.items + parser->ir->procs.len - 1,
                   index, dest_name, dest_index, temp0_index);
    Vars *vars = parser->varss->items + parser->varss->len - 1;
    vars->items[dest_index].type = vars->items[temp0_index].type;
  }
}

static void parser_parse_stmt_impl(Parser *parser) {
  Token token;
  parser_peek_token(parser, &token);
  parser_expect_token(parser,
                      MASK(TT_LET) | MASK(TT_IDENT) |
                      MASK(TT_RET) | MASK(TT_RETVAL) |
                      MASK(TT_WHILE) | MASK(TT_IF) |
                      MASK(TT_ASM),
                      "`let`, identifier, `ret`, `retval`, `while`, `if` or `asm`");

  if (token.id == TT_LET) {
    Token name_token;
    parser_peek_token(parser, &name_token);
    parser_expect_token(parser, MASK(TT_IDENT) | MASK(TT_OPAREN), "identifier or `(`");

    u32 index = alloc_var(parser, (Str) {0}, token);

    if (name_token.id == TT_IDENT) {
      parser_expect_token(parser, MASK(TT_SET), "`=`");
      parser_parse_expr(parser, (Str) {0}, index);

      EProc *proc = parser->ir->procs.items + parser->ir->procs.len - 1;

      get_var_by_index(parser->varss, index)->name = name_token.lexeme;
      backpatch_dest(proc, proc->instrs.len - 1, name_token.lexeme, index, index);

      EInstr *instr = proc->instrs.items + proc->instrs.len - 1;
      if (instr->kind == EInstrKindCopy)
        instr->as.copy.is_explicit = true;
    } else {
      Tokens name_tokens = {0};

      parser_peek_token(parser, &name_token);
      parser_expect_token(parser, MASK(TT_IDENT), "identifier");

      DA_APPEND(name_tokens, name_token);

      while (parser_peek_token(parser, &name_token) != TokenStatusEOF &&
             name_token.id != TT_CPAREN) {
        parser_expect_token(parser, MASK(TT_COMMA), "`,` or `)`");
        parser_peek_token(parser, &name_token);
        parser_expect_token(parser, MASK(TT_IDENT), "identifier");

        DA_APPEND(name_tokens, name_token);
      }

      parser_expect_token(parser, MASK(TT_CPAREN), "`,` or `)`");
      parser_expect_token(parser, MASK(TT_SET), "`=`");
      parser_parse_expr(parser, (Str) {0}, index);

      for (u32 i = 0; i < name_tokens.len; ++i) {
        Token *name_token = name_tokens.items + i;
        u32 field_index = alloc_var(parser, name_token->lexeme, *name_token);

        emit_instr(
          &parser->ir->procs,
          *name_token,
          EInstrKindCopyFromOffset,
          .copy_from_offset = {
            name_token->lexeme,
            field_index,
            {},
            index,
            i,
          },
        );
      }
    }
  } else if (token.id == TT_IDENT) {
    Token name_token = token;

    parser_peek_token(parser, &token);
    parser_expect_token(parser,
                        MASK(TT_SET) | MASK(TT_COLONSET) |
                        MASK(TT_OPAREN) | MASK(TT_OBRACE) |
                        MASK(TT_DOT),
                        "`=`, `:=`, `(`, `[` or `.`");

    if (token.id == TT_SET) {
      u32 index = get_var_index(parser->varss, name_token.lexeme);
      parser_parse_expr(parser, name_token.lexeme, index);

      EProc *proc = parser->ir->procs.items + parser->ir->procs.len - 1;
      EInstr *instr = proc->instrs.items + proc->instrs.len - 1;
      if (instr->kind == EInstrKindCopy)
        instr->as.copy.is_explicit = true;
    } else if (token.id == TT_COLONSET) {
      u32 index = get_var_index(parser->varss, name_token.lexeme);
      u32 temp_index = alloc_var(parser, (Str) {0}, token);
      parser_parse_expr(parser, (Str) {0}, temp_index);

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindCopyToRef,
        .copy_to_ref = {
          name_token.lexeme,
          index,
          false,
          {},
          0,
          {},
          temp_index,
        },
      );
    } else if (token.id == TT_OPAREN) {
      Indices arg_indices = {0};

      Token temp_token;
      while (parser_peek_token(parser, &temp_token) != TokenStatusEOF &&
             temp_token.id != TT_CPAREN) {
        u32 arg_index = alloc_var(parser, (Str) {0}, token);
        DA_APPEND(arg_indices, arg_index);

        parser_parse_expr(parser, (Str) {0}, arg_index);
        if (parser_peek_token(parser, &temp_token) == TokenStatusEOF ||
            temp_token.id != TT_CPAREN)
          parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_CPAREN), "`,` or `)`");
      }
      parser_expect_token(parser, MASK(TT_CPAREN), "`,` or `)`");

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindCall,
        .call = { name_token.lexeme, arg_indices },
      );
    } else if (token.id == TT_OBRACE) {
      u32 index_index = alloc_var(parser, (Str) {0}, token);
      u32 temp_index = alloc_var(parser, (Str) {0}, token);

      parser_parse_expr(parser, (Str) {0}, index_index);
      parser_expect_token(parser, MASK(TT_CBRACE), "`]`");
      parser_expect_token(parser, MASK(TT_COLONSET), "`:=`");
      parser_parse_expr(parser, (Str) {0}, temp_index);

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindCopyToRef,
        .copy_to_ref = {
          name_token.lexeme,
          get_var_index(parser->varss, name_token.lexeme),
          true,
          {},
          index_index,
          {},
          temp_index,
        },
      );
    } else if (token.id == TT_DOT) {
      Token field_name_token;
      u32 temp_index = alloc_var(parser, (Str) {0}, token);

      parser_peek_token(parser, &field_name_token);
      parser_expect_token(parser, MASK(TT_IDENT) | MASK(TT_INT), "identifier or integer");
      parser_expect_token(parser, MASK(TT_SET), "`=`");
      parser_parse_expr(parser, (Str) {0}, temp_index);

      if (field_name_token.id == TT_IDENT) {
        emit_instr(
          &parser->ir->procs,
          token,
          EInstrKindCopyToField,
          .copy_to_field = {
            name_token.lexeme,
            get_var_index(parser->varss, name_token.lexeme),
            field_name_token.lexeme,
            {},
            temp_index,
          },
        );
      } else {
        emit_instr(
          &parser->ir->procs,
          token,
          EInstrKindCopyToOffset,
          .copy_to_offset = {
            name_token.lexeme,
            get_var_index(parser->varss, name_token.lexeme),
            str_to_u32(field_name_token.lexeme),
            {},
            temp_index,
          },
        );
      }
    }
  } else if (token.id == TT_RET) {
    emit_instr(&parser->ir->procs, token, EInstrKindRet,);
  } else if (token.id == TT_RETVAL) {
    u32 ret_val_index = alloc_var(parser, (Str) {0}, token);
    parser_parse_expr(parser, (Str) {0}, ret_val_index);

    emit_instr(
      &parser->ir->procs,
      token,
      EInstrKindRetVal,
      .ret_val = { ret_val_index },
    );
  } else if (token.id == TT_WHILE) {
    EInstrs *instrs = &parser->ir->procs.items[parser->ir->procs.len - 1].instrs;

    u32 label_index = instrs->len;

    u32 cond_index = alloc_var(parser, (Str) {0}, token);
    parser_parse_expr(parser, (Str) {0}, cond_index);

    u32 jump_index = instrs->len;

    emit_instr(
      &parser->ir->procs,
      token,
      EInstrKindJumpIfNot,
      .jump_if_not = { cond_index, 0 },
    );

    while (parser_peek_token(parser, &token) != TokenStatusEOF &&
           token.id != TT_END) {
      parser_parse_stmt(parser);
    }
    parser_expect_token(parser, MASK(TT_END), "`end`");

    emit_instr(
      &parser->ir->procs,
      token,
      EInstrKindJump,
      .jump = { label_index },
    );

    instrs->items[jump_index].as.jump_if_not.target = instrs->len;
  } else if (token.id == TT_IF) {
    EInstrs *instrs = &parser->ir->procs.items[parser->ir->procs.len - 1].instrs;

    u32 cond_index = alloc_var(parser, (Str) {0}, token);
    parser_parse_expr(parser, (Str) {0}, cond_index);

    u32 jump_index = instrs->len;
    Da(u32) jumps_to_end_indices = {0};

    emit_instr(
      &parser->ir->procs,
      token,
      EInstrKindJumpIfNot,
      .jump_if_not = { cond_index, 0 },
    );

    while (parser_peek_token(parser, &token) != TokenStatusEOF &&
           token.id != TT_ELIF && token.id != TT_ELSE && token.id != TT_END) {
      parser_parse_stmt(parser);
    }

    DA_APPEND(jumps_to_end_indices, instrs->len);

    emit_instr(&parser->ir->procs, token, EInstrKindJump, .jump = { 0 });

    while (parser_peek_token(parser, &token) != TokenStatusEOF &&
           token.id != TT_ELSE && token.id != TT_END) {
      instrs->items[jump_index].as.jump_if_not.target = instrs->len;

      parser_expect_token(parser,
                          MASK(TT_ELIF) | MASK(TT_ELSE) | MASK(TT_END),
                          "`elif`, `else` or `end`");
      parser_parse_expr(parser, (Str) {0}, cond_index);

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindJumpIfNot,
        .jump_if_not = { cond_index, 0 },
      );

      jump_index = instrs->len - 1;

      while (parser_peek_token(parser, &token) != TokenStatusEOF &&
             token.id != TT_ELIF && token.id != TT_ELSE && token.id != TT_END) {
        parser_parse_stmt(parser);
      }

      DA_APPEND(jumps_to_end_indices, instrs->len);

      emit_instr(&parser->ir->procs, token, EInstrKindJump, .jump = { 0 });

      instrs->items[jump_index].as.jump_if_not.target = instrs->len;
    }
    parser_expect_token(parser,
                        MASK(TT_ELIF) | MASK(TT_ELSE) | MASK(TT_END),
                        "`elif`, `else` or `end`");

    if (token.id == TT_ELSE) {
      instrs->items[jump_index].as.jump_if_not.target = instrs->len;
      jump_index = instrs->len - 1;

      while (parser_peek_token(parser, &token) != TokenStatusEOF &&
             token.id != TT_END) {
        parser_parse_stmt(parser);
      }
      parser_expect_token(parser, MASK(TT_END), "`end`");

      instrs->items[jump_index].as.jump.target = instrs->len;
    } else {
      instrs->items[jump_index].as.jump_if_not.target = instrs->len;
    }

    for (u32 i = 0; i < jumps_to_end_indices.len; ++i)
      instrs->items[jumps_to_end_indices.items[i]].as.jump.target = instrs->len;
    } else if (token.id == TT_ASM) {
      EAsmSegments asm_segments = {0};

      Token temp_token = {0};
      do {
        if (temp_token.id == TT_COMMA)
          parser_next_token(parser, NULL);

        parser_peek_token(parser, &temp_token);
        parser_expect_token(parser,
                            MASK(TT_STR) | MASK(TT_IDENT),
                            "string literal or identifier");

        if (temp_token.id == TT_STR) {
          EAsmSegment asm_segment = {
            EAsmSegmentKindStr,
            {
              temp_token.lexeme.ptr + 1,
              temp_token.lexeme.len - 2,
            },
            0,
          };
          DA_APPEND(asm_segments, asm_segment);
        } else {
          EAsmSegment asm_segment = {
            EAsmSegmentKindVar,
            temp_token.lexeme,
            get_var_index(parser->varss, temp_token.lexeme),
          };
          DA_APPEND(asm_segments, asm_segment);
        }
      } while (parser_peek_token(parser, &temp_token) != TokenStatusEOF &&
               temp_token.id == TT_COMMA);

      emit_instr(
        &parser->ir->procs,
        token,
        EInstrKindInlineAsm,
        .inline_asm = { asm_segments },
      );
    }
}

static void parser_parse_proc_impl(Parser *parser) {
  Token token;
  parser_peek_token(parser, &token);
  parser_expect_token(parser, MASK(TT_IDENT), "identifier");
  Str name = token.lexeme;

  emit_proc(&parser->ir->procs, name);
  DA_APPEND(*parser->varss, (Vars) {0});

  parser_expect_token(parser, MASK(TT_OPAREN), "`(`");
  while (parser_peek_token(parser, &token) != TokenStatusEOF &&
         token.id != TT_CPAREN) {
    parser_peek_token(parser, &token);
    parser_expect_token(parser, MASK(TT_IDENT), "identifier");
    parser_expect_token(parser, MASK(TT_COLON), "`:`");
    parser_parse_type(parser);

    emit_arg(&parser->ir->procs, token.lexeme, parser->last_type);

    if (parser_peek_token(parser, &token) != TokenStatusEOF &&
        token.id != TT_CPAREN)
      parser_expect_token(parser, MASK(TT_CPAREN) | MASK(TT_COMMA), "`,` or `)`");
  }
  parser_expect_token(parser, MASK(TT_CPAREN) | MASK(TT_COMMA), "`,` or `)`");

  if (parser_peek_token(parser, &token) != TokenStatusEOF &&
      token.id == TT_ARROW) {
    parser_next_token(parser, NULL);
    parser_parse_type(parser);
    emit_return_type(&parser->ir->procs, parser->last_type);
  }

  EProc *proc = parser->ir->procs.items + parser->ir->procs.len - 1;
  Vars *vars = parser->varss->items + parser->varss->len - 1;

  for (u32 i = 0; i < proc->args.len; ++i) {
    Var var = {
      proc->args.items[i].name,
      type_clone(&proc->args.items[i].type),
      false,
      {},
    };
    DA_APPEND(*vars, var);
  }

  while (parser_peek_token(parser, &token) != TokenStatusEOF &&
         token.id != TT_END) {
    parser_parse_stmt(parser);
  }

  parser_expect_token(parser, MASK(TT_END), "`end`");
}

static void parser_parse_struct_impl(Parser *parser) {
  Token token;
  parser_peek_token(parser, &token);
  parser_expect_token(parser, MASK(TT_IDENT), "identifier");

  emit_struct(&parser->ir->structs, token.lexeme);

  while (parser_peek_token(parser, &token) != TokenStatusEOF &&
         token.id != TT_END) {
    parser_peek_token(parser, &token);
    parser_expect_token(parser, MASK(TT_IDENT), "identifier");
    parser_expect_token(parser, MASK(TT_COLON), "`:`");
    parser_parse_type(parser);

    emit_field(&parser->ir->structs, token.lexeme, parser->last_type);

    if (parser_peek_token(parser, &token) == TokenStatusEOF || token.id != TT_END)
      parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_END), "`,` or `end`");
  }
  parser_expect_token(parser, MASK(TT_COMMA) | MASK(TT_END), "`,` or `end`");
}

static void parser_parse_program(Parser *parser) {
  Token token;
  while (parser_peek_token(parser, &token) != TokenStatusEOF) {
    parser_expect_token(parser,
                        MASK(TT_PROC) | MASK(TT_CONST) |
                        MASK(TT_USE) | MASK(TT_STRUCT),
                        "`proc`, `const`, `use` or `struct`");

    switch (token.id) {
    case TT_PROC: {
      parser_parse_proc(parser);
    } break;

    case TT_CONST: {
      parser_peek_token(parser, &token);
      parser_expect_token(parser, MASK(TT_IDENT), "identifier");
      parser_expect_token(parser, MASK(TT_SET), "`=`");
      parser_parse_primary_expr(parser, (Str) {0}, 0, true);

      emit_const(&parser->ir->consts, token.lexeme, parser->last_const_value);
    } break;

    case TT_USE: {
      EModulePath module_path = {0};

      parser_peek_token(parser, &token);
      DA_APPEND(module_path, token.lexeme);
      parser_expect_token(parser, MASK(TT_IDENT) | MASK(TT_SUPER), "identifier or `super`");
      while (parser_peek_token(parser, &token) != TokenStatusEOF &&
             token.id == TT_COLONCOLON) {
        parser_next_token(parser, NULL);
        parser_peek_token(parser, &token);
        parser_expect_token(parser, MASK(TT_IDENT) | MASK(TT_SUPER), "identifier or `super`");
        DA_APPEND(module_path, token.lexeme);
      }

      emit_module_dep(&parser->ir->module_deps, module_path);
    } break;

    case TT_STRUCT: {
      parser_parse_struct(parser);
    } break;
    }
  }
}

bool parse(EIr *ir, Varss *varss, Str code, Str file_path) {
  Parser parser = {
    ir,
    varss,
    {},
    {},
    lexer_create(code),
    TokenStatusOk,
    {},
    file_path,
    false,
  };

  parser_next_token(&parser, NULL);
  parser_parse_program(&parser);

  lexer_destroy(&parser.lexer);

  return !parser.has_error;
}
