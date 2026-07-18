#include "checker.h"
#include "shl/shl-log.h"

#define CERROR(fmt)                     \
  PERROR(STR_FMT":%u:%u: ", fmt,        \
         STR_ARG(instr->loc.file_path), \
         instr->loc.row + 1,            \
         instr->loc.col + 1)

#define CERRORF(fmt, ...)               \
  PERROR(STR_FMT":%u:%u: ", fmt,        \
         STR_ARG(instr->loc.file_path), \
         instr->loc.row + 1,            \
         instr->loc.col + 1,            \
         __VA_ARGS__)

#define CERRORT(fmt)                   \
  PERROR(STR_FMT":%u:%u: ", fmt,       \
         STR_ARG(type->loc.file_path), \
         type->loc.row + 1,            \
         type->loc.col + 1)

#define CERRORFT(fmt, ...)             \
  PERROR(STR_FMT":%u:%u: ", fmt,       \
         STR_ARG(type->loc.file_path), \
         type->loc.row + 1,            \
         type->loc.col + 1,            \
         __VA_ARGS__)

#define CHECK_VAR(index, _name, local_index, check_move)           \
  do {                                                             \
    if (index == (u32) -1) {                                       \
      CERRORF("Variable "STR_FMT" was not defined before usage\n", \
              STR_ARG(_name));                                     \
      goto fail;                                                   \
    }                                                              \
    Var *var##local_index = varss->items[i].items + index;         \
    if (check_move && var##local_index->moved) {                   \
      CERRORF("Trying to access variable "STR_FMT" after move\n",  \
              STR_ARG(var##local_index->name));                    \
      PINFO(STR_FMT":%u:%u: ", "Moved here\n",                     \
            STR_ARG(var##local_index->moved_loc.file_path),        \
            var##local_index->moved_loc.row + 1,                   \
            var##local_index->moved_loc.col + 1);                  \
      goto fail;                                                   \
    }                                                              \
  } while (0)

typedef Da(EType *) ETypeRefs;

typedef struct {
  u32 beginning;
  u32 from;
  u32 to;
} VarSubstitution;

typedef Da(VarSubstitution) VarSubstitutions;

static bool check_struct_existence(EStructs *structs, EType *type) {
  if (type->kind == ETypeKindStruct) {
    if (!get_struct(structs, type->name)) {
      CERRORFT("Structure "STR_FMT" was not defined\n", STR_ARG(type->name));
      return false;
    }
  } else if (type->kind == ETypeKindPtr) {
    return check_struct_existence(structs, type->ptr_target);
  }

  return true;
}

static EType make_type_from_kind(ETypeKind kind) {
  return (EType) { kind, {}, {} };
}

static void free_type_str(Str str, EType *type) {
  if (type->kind == ETypeKindPtr || type->kind == ETypeKindArray)
    free(str.ptr);
}

static Str get_type_str(EType *type) {
  if (type->kind == ETypeKindStruct) {
    return type->name;
  } else {
    switch (type->kind) {
    case ETypeKindUnit:   return STR_LIT("unit");
    case ETypeKindS8:     return STR_LIT("s8");
    case ETypeKindS16:    return STR_LIT("s16");
    case ETypeKindS32:    return STR_LIT("s32");
    case ETypeKindS64:    return STR_LIT("s64");
    case ETypeKindU8:     return STR_LIT("u8");
    case ETypeKindU16:    return STR_LIT("u16");
    case ETypeKindU32:    return STR_LIT("u32");
    case ETypeKindU64:    return STR_LIT("u64");
    case ETypeKindBool:   return STR_LIT("bool");
    case ETypeKindStruct: return STR_LIT("struct"); // unreachable

    case ETypeKindArray: {
      Str target_str = get_type_str(type->array_element);
      StringBuilder sb = {0};
      sb_push_char(&sb, '[');
      sb_push_str(&sb, target_str);
      sb_push(&sb, "; ");
      sb_push_u32(&sb, type->array_len);
      sb_push_char(&sb, ']');
      free_type_str(target_str, type->array_element);
      return sb_to_str(sb);
    }

    case ETypeKindTuple: {
      StringBuilder sb = {0};
      sb_push_char(&sb, '(');
      for (u32 i = 0; i < type->tuple_types.len; ++i) {
        if (i > 0)
          sb_push(&sb, ", ");
        Str type_str = get_type_str(type->tuple_types.items + i);
        sb_push_str(&sb, type_str);
        free_type_str(type_str, type->tuple_types.items + i);
      }
      sb_push_char(&sb, ')');
      return sb_to_str(sb);
    }

    case ETypeKindStr: return STR_LIT("str");

    case ETypeKindPtr: {
      Str target_str = get_type_str(type->ptr_target);
      Str result;
      result.len = target_str.len + 1;
      result.ptr = malloc(result.len);
      result.ptr[0] = '&';
      memcpy(result.ptr + 1, target_str.ptr, target_str.len);
      free_type_str(target_str, type->ptr_target);
      return result;
    }
    }
  }

  ERROR("Unreachable\n");
  return (Str) {0};
}

static char *get_bin_op_kind_cstr(EBinOpKind kind) {
  switch (kind) {
  case EBinOpKindAdd:    return "+";
  case EBinOpKindSub:    return "-";
  case EBinOpKindMul:    return "*";
  case EBinOpKindDiv:    return "/";
  case EBinOpKindRem:    return "%";
  case EBinOpKindAnd:    return "&";
  case EBinOpKindOr:     return "|";
  case EBinOpKindXor:    return "^";
  case EBinOpKindLShift: return "<<";
  case EBinOpKindRShift: return ">>";
  case EBinOpKindEq:     return "==";
  case EBinOpKindNe:     return "!=";
  case EBinOpKindLs:     return "<";
  case EBinOpKindLe:     return "<=";
  case EBinOpKindGt:     return ">";
  case EBinOpKindGe:     return ">=";
  }

  ERROR("Unreachable\n");
  return NULL;
}

static EType *get_proc_return_type(EIr *ir, Str name, ETypeRefs *arg_types) {
  for (u32 i = 0; i < ir->procs.len; ++i) {
    EProc *proc = ir->procs.items + i;

    if (!str_eq(proc->name, name) || proc->args.len != arg_types->len)
      continue;

    bool all = true;

    for (u32 j = 0; j < proc->args.len; ++j) {
      if (!type_eq(&proc->args.items[j].type, arg_types->items[j])) {
        all = false;
        break;
      }
    }

    if (all)
      return &proc->return_type;
  }

  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;

    for (u32 j = 0; j < dep->ir.procs.len; ++j) {
      EProc *proc = dep->ir.procs.items + j;

      if (!str_eq(proc->name, name) || proc->args.len != arg_types->len)
        continue;

      bool all = true;

      for (u32 j = 0; j < proc->args.len; ++j) {
        if (!type_eq(&proc->args.items[j].type, arg_types->items[j])) {
          all = false;
          break;
        }
      }

      if (all)
        return &proc->return_type;
    }
  }

  return NULL;
}

static void fprintf_proc_signature(FILE *stream, Str name, ETypeRefs *arg_types) {
  fprintf(stream, STR_FMT"(", STR_ARG(name));
  for (u32 i = 0; i < arg_types->len; ++i) {
    Str arg_type_str = get_type_str(arg_types->items[i]);
    if (i > 0)
      fputs(", ", stream);
    fprintf(stream, STR_FMT, STR_ARG(arg_type_str));
    free_type_str(arg_type_str, arg_types->items[i]);
  }
  putc(')', stream);
}

static bool is_int(EType *type) {
  return type->kind >= ETypeKindS8 && type->kind <= ETypeKindU64;
}

static bool can_do_bin_op(EBinOpKind kind, EType *a, EType *b) {
  if (kind == EBinOpKindAdd || kind == EBinOpKindSub) {
    if ((a->kind == ETypeKindPtr || a->kind == ETypeKindArray) && is_int(b))
      return true;
    if (is_int(a) && is_int(b) && a->kind == b->kind)
      return true;
  } else {
    if (is_int(a) && is_int(b) && a->kind == b->kind)
      return true;
  }
  return false;
}

static bool can_cast(EType *src, EType *dest) {
  return (is_int(src) && is_int(dest)) ||
         (is_int(src) && dest->kind == ETypeKindBool) ||
         (is_int(src) && dest->kind == ETypeKindPtr) ||
         (src->kind == ETypeKindBool && is_int(dest)) ||
         (src->kind == ETypeKindPtr && is_int(dest)) ||
         (src->kind == ETypeKindPtr && dest->kind == ETypeKindPtr);
}

static void substitute_var_uses_with_its_src(EProc *proc, VarSubstitution *var_subst) {
  u32 var_index = var_subst->from;
  u32 src_index = var_subst->to;

  for (u32 i = var_subst->beginning; i < proc->instrs.len; ++i) {
    EInstr *instr = proc->instrs.items + i;

    switch (instr->kind) {
    case EInstrKindAlloc: break;

    case EInstrKindStore: {
      if (instr->as.store.index == var_index)
        return;
    } break;

    case EInstrKindCopy: {
      if (instr->as.copy.src_index == var_index)
        instr->as.copy.src_index = src_index;
      if (instr->as.copy.dest_index == var_index)
        return;
    } break;

    case EInstrKindBinOp: {
      if (instr->as.bin_op.src0_index == var_index)
        instr->as.bin_op.src0_index = src_index;
      if (instr->as.bin_op.src1_index == var_index)
        instr->as.bin_op.src1_index = src_index;
      if (instr->as.bin_op.dest_index == var_index)
        return;
    } break;

    case EInstrKindCall: {
      for (u32 j = 0; j < instr->as.call.arg_indices.len; ++j)
        if (instr->as.call.arg_indices.items[j] == var_index)
          instr->as.call.arg_indices.items[j] = src_index;
    } break;

    case EInstrKindCallAssign: {
      for (u32 j = 0; j < instr->as.call_assign.arg_indices.len; ++j)
        if (instr->as.call_assign.arg_indices.items[j] == var_index)
          instr->as.call_assign.arg_indices.items[j] = src_index;
      if (instr->as.call_assign.dest_index == var_index)
        return;
    } break;

    case EInstrKindRet: break;

    case EInstrKindRetVal: {
      if (instr->as.ret_val.index == var_index)
        instr->as.ret_val.index = src_index;
    } break;

    case EInstrKindJump: break;

    case EInstrKindJumpIfNot: {
      if (instr->as.jump_if_not.cond_index == var_index)
        instr->as.jump_if_not.cond_index = src_index;
    } break;

    case EInstrKindRef: {
      if (instr->as.ref.src_index == var_index)
        instr->as.ref.src_index = src_index;
      if (instr->as.ref.dest_index == var_index)
        return;
    } break;

    case EInstrKindCopyToRef: {
      if (instr->as.copy_to_ref.dest_index == var_index)
        instr->as.copy_to_ref.dest_index = src_index;
      if (instr->as.copy_to_ref.dest_offset_index == var_index)
        instr->as.copy_to_ref.dest_offset_index = src_index;
      if (instr->as.copy_to_ref.src_index == var_index)
        instr->as.copy_to_ref.src_index = src_index;
    } break;

    case EInstrKindCopyFromRef: {
      if (instr->as.copy_from_ref.src_index == var_index)
        instr->as.copy_from_ref.src_index = src_index;
      if (instr->as.copy_from_ref.src_offset_index == var_index)
        instr->as.copy_from_ref.src_offset_index = src_index;
      if (instr->as.copy_from_ref.dest_index == var_index)
        return;
    } break;

    case EInstrKindStoreNull: {
      if (instr->as.store_null.index == var_index)
        return;
    } break;

    case EInstrKindInlineAsm: {
      for (u32 j = 0; j < instr->as.inline_asm.segments.len; ++j)
        if (instr->as.inline_asm.segments.items[j].kind == EAsmSegmentKindVar &&
            instr->as.inline_asm.segments.items[j].value_index == var_index)
          instr->as.inline_asm.segments.items[j].value_index = src_index;
    } break;

    case EInstrKindStoreStr: {
      if (instr->as.store_str.index == var_index)
        return;
    } break;

    case EInstrKindCast: {
      if (instr->as.cast.src_index == var_index)
        instr->as.cast.src_index = src_index;
      if (instr->as.cast.dest_index == var_index)
        return;
    } break;

    case EInstrKindLenOf: {
      if (instr->as.len_of.src_index == var_index)
        instr->as.len_of.src_index = src_index;
      if (instr->as.len_of.dest_index == var_index)
        return;
    } break;

    case EInstrKindCopyToField: {
      if (instr->as.copy_to_field.dest_index == var_index)
        instr->as.copy_to_field.dest_index = src_index;
      if (instr->as.copy_to_field.src_index == var_index)
        instr->as.copy_to_field.src_index = src_index;
    } break;

    case EInstrKindCopyFromField: {
      if (instr->as.copy_from_field.src_index == var_index)
        instr->as.copy_from_field.src_index = src_index;
      if (instr->as.copy_from_field.dest_index == var_index)
        return;
    } break;

    case EInstrKindTuple: {
      for (u32 j = 0; j < instr->as.tuple.field_indices.len; ++j)
        if (instr->as.tuple.field_indices.items[j] == var_index)
          instr->as.tuple.field_indices.items[j] = src_index;
      if (instr->as.tuple.dest_index == var_index)
        return;
    } break;

    case EInstrKindCopyToOffset: {
      if (instr->as.copy_to_offset.dest_index == var_index)
        instr->as.copy_to_offset.dest_index = src_index;
      if (instr->as.copy_to_offset.src_index == var_index)
        instr->as.copy_to_offset.src_index = src_index;
    } break;

    case EInstrKindCopyFromOffset: {
      if (instr->as.copy_from_offset.src_index == var_index)
        instr->as.copy_from_offset.src_index = src_index;
      if (instr->as.copy_from_offset.dest_index == var_index)
        return;
    } break;
    }
  }
}

static bool tuple_type_matches(EType *type, ETypes *field_types) {
  if (type->tuple_types.len != field_types->len)
    return false;

  for (u32 i = 0; i < field_types->len; ++i)
    if (!type_eq(type->tuple_types.items + i, field_types->items + i))
      return false;

  return true;
}

static void fprintf_tuple_type(FILE *stream, ETypes *field_types) {
  fputc('(', stream);
  for (u32 i = 0; i < field_types->len; ++i) {
    Str field_type_str = get_type_str(field_types->items + i);
    if (i > 0)
      fputs(", ", stream);
    fprintf(stream, STR_FMT, STR_ARG(field_type_str));
    free_type_str(field_type_str, field_types->items + i);
  }
  fputc(')', stream);
}

bool check_ir(EIr *ir, Varss *varss, bool require_main) {
  bool found_main = false;
  VarSubstitutions var_substs = {0};

  for (u32 i = 0; i < ir->structs.len; ++i) {
    EStruct *_struct = ir->structs.items + i;

    for (u32 j = 0; j < _struct->fields.len; ++j)
      if (!check_struct_existence(&ir->structs, &_struct->fields.items[j].type))
        goto fail;
  }

  for (u32 i = 0; i < ir->procs.len; ++i) {
    EProc *proc = ir->procs.items + i;

    if (str_eq(proc->name, STR_LIT("main"))) {
      if (found_main) {
        ERROR("`main` procedure cannot be overloaded\n");
        goto fail;
      } else {
        found_main = true;
      }
    }

    for (u32 j = 0; j < proc->args.len; ++j)
      if (!check_struct_existence(&ir->structs, &proc->args.items[j].type))
        goto fail;
    if (!check_struct_existence(&ir->structs, &proc->return_type))
      goto fail;

    for (u32 j = 0; j < proc->instrs.len; ++j) {
      EInstr *instr = proc->instrs.items + j;

      switch (instr->kind) {
      case EInstrKindAlloc: break;

      case EInstrKindStore: {
        CHECK_VAR(instr->as.store.index, instr->as.store.name, 0, false);

        Var *var = varss->items[i].items + instr->as.store.index;
        var->moved = false;
        EType value_type = make_type_from_kind(instr->as.store.value.kind);
        if (var->type.kind == ETypeKindUnit) {
          var->type = value_type;
        } else if (!type_eq(&value_type, &var->type)) {
          Str value_type_str = get_type_str(&value_type);
          Str var_type_str = get_type_str(&var->type);
          CERRORF("Cannot assign value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(value_type_str), STR_ARG(var_type_str));
          free_type_str(value_type_str, &value_type);
          free_type_str(var_type_str, &var->type);
          goto fail;
        }
      } break;

      case EInstrKindCopy: {
        CHECK_VAR(instr->as.copy.dest_index, instr->as.copy.dest_name, 0, false);
        CHECK_VAR(instr->as.copy.src_index, instr->as.copy.src_name, 1, true);

        Var *dest = varss->items[i].items + instr->as.copy.dest_index;
        dest->moved = false;
        Var *src = varss->items[i].items + instr->as.copy.src_index;
        if (dest->type.kind == ETypeKindUnit) {
          dest->type = type_clone(&src->type);
        } else if (!type_eq(&src->type, &dest->type)) {
          Str src_type_str = get_type_str(&src->type);
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Cannot assign value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(src_type_str), STR_ARG(dest_type_str));
          free_type_str(src_type_str, &src->type);
          free_type_str(dest_type_str, &dest->type);
          goto fail;
        }

        if (src->type.kind == ETypeKindStruct ||
            src->type.kind == ETypeKindArray ||
            src->type.kind == ETypeKindTuple) {
          if (instr->as.copy.is_explicit) {
            src->moved = true;
            src->moved_loc = instr->loc;
          }
          VarSubstitution var_subst = {
            j,
            instr->as.copy.dest_index,
            instr->as.copy.src_index,
          };
          DA_APPEND(var_substs, var_subst);
          DA_REMOVE_AT(proc->instrs, j);
          --j;
        }
      } break;

      case EInstrKindBinOp: {
        CHECK_VAR(instr->as.bin_op.dest_index, instr->as.bin_op.dest_name, 0, false);
        CHECK_VAR(instr->as.bin_op.src0_index, instr->as.bin_op.src0_name, 1, true);
        CHECK_VAR(instr->as.bin_op.src1_index, instr->as.bin_op.src1_name, 2, true);

        Var *dest = varss->items[i].items + instr->as.bin_op.dest_index;
        dest->moved = false;
        Var *src0 = varss->items[i].items + instr->as.bin_op.src0_index;
        Var *src1 = varss->items[i].items + instr->as.bin_op.src1_index;

        if (!can_do_bin_op(instr->as.bin_op.kind, &src0->type, &src1->type)) {
          Str src0_type_str = get_type_str(&src0->type);
          Str src1_type_str = get_type_str(&src1->type);
          char *bin_op_cstr = get_bin_op_kind_cstr(instr->as.bin_op.kind);
          CERRORF("Cannot perform "STR_FMT" %s "STR_FMT"\n",
                  STR_ARG(src0_type_str),
                  bin_op_cstr,
                  STR_ARG(src1_type_str));
          free_type_str(src0_type_str, &src0->type);
          free_type_str(src1_type_str, &src1->type);
          goto fail;
        }

        EType dest_type;
        if (instr->as.bin_op.kind >= EBinOpKindEq &&
            instr->as.bin_op.kind <= EBinOpKindGe)
          dest_type = (EType) { ETypeKindBool, {}, {} };
        else
          dest_type = type_clone(&src0->type);

        if (dest->type.kind == ETypeKindUnit) {
          dest->type = dest_type;
        } else if (!type_eq(&src0->type, &dest->type)) {
          Str src0_type_str = get_type_str(&src0->type);
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Cannot assign value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(src0_type_str), STR_ARG(dest_type_str));
          free_type_str(src0_type_str, &src0->type);
          free_type_str(dest_type_str, &dest->type);
          if (dest_type.kind == ETypeKindPtr)
            type_free(dest_type.ptr_target);
          else if (dest_type.kind == ETypeKindArray)
            type_free(dest_type.array_element);
          goto fail;
        }

        if (dest_type.kind == ETypeKindPtr)
          type_free(dest_type.ptr_target);
        else if (dest_type.kind == ETypeKindArray)
          type_free(dest_type.array_element);

        if ((src0->type.kind == ETypeKindPtr || src0->type.kind == ETypeKindArray) &&
            (instr->as.bin_op.kind == EBinOpKindAdd || instr->as.bin_op.kind == EBinOpKindSub)) {
          EType *sub_type;
          if (src0->type.kind == ETypeKindPtr)
            sub_type = src0->type.ptr_target;
          else
            sub_type = src0->type.array_element;

          Var new_var = {
            {},
            { ETypeKindU64, {}, {} },
            false,
            {},
          };
          DA_APPEND(varss->items[i], new_var);
          EInstr new_instr0 = {
            EInstrKindAlloc,
            {
              .alloc = {
                {},
                varss->items[i].len - 1,
              },
            },
            {},
          };
          EInstr new_instr1 = {
            EInstrKindStore,
            {
              .store = {
                {},
                varss->items[i].len - 1,
                {
                  ETypeKindU64,
                  {
                    ._unsigned = get_type_size(&ir->structs, sub_type),
                  },
                },
              },
            },
            {},
          };
          EInstr new_instr2 = {
            EInstrKindBinOp,
            {
              .bin_op = {
                {},
                instr->as.bin_op.src1_index,
                {},
                instr->as.bin_op.src1_index,
                {},
                varss->items[i].len - 1,
                EBinOpKindMul,
              },
            },
            {},
          };
          DA_INSERT(proc->instrs, j, new_instr2);
          DA_INSERT(proc->instrs, j, new_instr1);
          DA_INSERT(proc->instrs, j, new_instr0);
          j += 3;
        }
      } break;

      case EInstrKindCall: {
        ETypeRefs arg_types = {0};
        for (u32 k = 0; k < instr->as.call.arg_indices.len; ++k) {
          Var *arg = varss->items[i].items + instr->as.call.arg_indices.items[k];
          if (arg->type.kind == ETypeKindStruct ||
              arg->type.kind == ETypeKindArray ||
              arg->type.kind == ETypeKindTuple)
            arg->moved = true;
          DA_APPEND(arg_types, &arg->type);
        }

        EType *return_type = get_proc_return_type(ir, instr->as.call.name, &arg_types);
        if (!return_type) {
          CERROR("Procedure ");
          fprintf_proc_signature(stderr, instr->as.call.name, &arg_types);
          fprintf(stderr, " was not declared or defined\n");
          if (arg_types.items)
            free(arg_types.items);
          goto fail;
        }

        if (arg_types.items)
          free(arg_types.items);
      } break;

      case EInstrKindCallAssign: {
        CHECK_VAR(instr->as.call_assign.dest_index, instr->as.call_assign.dest_name, 0, false);

        ETypeRefs arg_types = {0};
        for (u32 k = 0; k < instr->as.call_assign.arg_indices.len; ++k) {
          Var *arg = varss->items[i].items + instr->as.call_assign.arg_indices.items[k];
          if (arg->type.kind == ETypeKindStruct ||
              arg->type.kind == ETypeKindArray ||
              arg->type.kind == ETypeKindTuple)
            arg->moved = true;
          DA_APPEND(arg_types, &arg->type);
        }

        EType *return_type = get_proc_return_type(ir, instr->as.call_assign.name, &arg_types);
        if (!return_type) {
          CERROR("Procedure ");
          fprintf_proc_signature(stderr, instr->as.call_assign.name, &arg_types);
          fprintf(stderr, " was not declared or defined\n");
          if (arg_types.items)
            free(arg_types.items);
          goto fail;
        }

        if (arg_types.items)
          free(arg_types.items);

        Var *dest = varss->items[i].items + instr->as.call_assign.dest_index;
        dest->moved = false;

        if (dest->type.kind == ETypeKindUnit) {
          dest->type = type_clone(return_type);
        } else if (!type_eq(return_type, &dest->type)) {
          Str return_type_str = get_type_str(return_type);
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Cannot assign return value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(return_type_str), STR_ARG(dest_type_str));
          free_type_str(return_type_str, return_type);
          free_type_str(dest_type_str, &dest->type);
          goto fail;
        }
      } break;

      case EInstrKindRet: {
        if (proc->return_type.kind != ETypeKindUnit) {
          Str return_type_str = get_type_str(&proc->return_type);
          CERRORF("Unexpected return type: unit, expected "STR_FMT"\n",
                  STR_ARG(return_type_str));
          free_type_str(return_type_str, &proc->return_type);
          goto fail;
        }
      } break;

      case EInstrKindRetVal: {
        Var *var = varss->items[i].items + instr->as.ret_val.index;
        if (!type_eq(&var->type, &proc->return_type)) {
          Str var_type_str = get_type_str(&var->type);
          Str return_type_str = get_type_str(&proc->return_type);
          CERRORF("Unexpected return type: "STR_FMT", expected "STR_FMT"\n",
                  STR_ARG(var_type_str), STR_ARG(return_type_str));
          free_type_str(var_type_str, &var->type);
          free_type_str(return_type_str, &proc->return_type);
          goto fail;
        }
      } break;

      case EInstrKindJump: break;

      case EInstrKindJumpIfNot: {
        Var *cond = varss->items[i].items + instr->as.jump_if_not.cond_index;
        if (cond->type.kind < ETypeKindS8 && cond->type.kind > ETypeKindBool) {
          Str cond_type_str = get_type_str(&cond->type);
          CERRORF("Unexpected condition type: "STR_FMT", expected integer or boolean\n",
                  STR_ARG(cond_type_str));
          free_type_str(cond_type_str, &cond->type);
          goto fail;
        }
      } break;

      case EInstrKindRef: {
        CHECK_VAR(instr->as.ref.dest_index, instr->as.ref.dest_name, 0, false);
        CHECK_VAR(instr->as.ref.src_index, instr->as.ref.src_name, 1, true);

        Var *dest = varss->items[i].items + instr->as.ref.dest_index;
        dest->moved = false;
        Var *src = varss->items[i].items + instr->as.ref.src_index;

        EType ptr_type = {
          ETypeKindPtr,
          {
            .ptr_target = malloc(sizeof(EType)),
          },
          {},
        };
        *ptr_type.ptr_target = type_clone(&src->type);

        if (dest->type.kind == ETypeKindUnit) {
          dest->type = ptr_type;
        } else if (!type_eq(&ptr_type, &dest->type)) {
          Str dest_type_str = get_type_str(&dest->type);
          Str ptr_type_str = get_type_str(&ptr_type);
          CERRORF("Cannot assign value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(ptr_type_str), STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          free_type_str(ptr_type_str, &ptr_type);
          type_free(ptr_type.ptr_target);
          goto fail;
        } else {
          type_free(ptr_type.ptr_target);
        }
      } break;

      case EInstrKindCopyToRef: {
        CHECK_VAR(instr->as.copy_to_ref.dest_index, instr->as.copy_to_ref.dest_name, 0, true);
        if (instr->as.copy_to_ref.has_offset)
          CHECK_VAR(instr->as.copy_to_ref.dest_offset_index, instr->as.copy_to_ref.dest_offset_name, 1, true);
        CHECK_VAR(instr->as.copy_to_ref.src_index, instr->as.copy_to_ref.src_name, 2, true);

        Var *dest = varss->items[i].items + instr->as.copy_to_ref.dest_index;
        Var *index = NULL;
        if (instr->as.copy_to_ref.has_offset)
          index = varss->items[i].items + instr->as.copy_to_ref.dest_offset_index;
        Var *src = varss->items[i].items + instr->as.copy_to_ref.src_index;

        if (index && !is_int(&index->type)) {
          Str index_type_str = get_type_str(&index->type);
          CERRORF("Trying to index using "STR_FMT"\n", STR_ARG(index_type_str));
          free_type_str(index_type_str, &index->type);
          goto fail;
        }

        if (dest->type.kind != ETypeKindPtr && dest->type.kind != ETypeKindArray) {
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Trying to dereference "STR_FMT"\n", STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          goto fail;
        } else if (dest->type.kind == ETypeKindPtr && !dest->type.ptr_target) {
          dest->type.ptr_target = malloc(sizeof(EType));
          *dest->type.ptr_target = type_clone(&src->type);
        } else if (!type_eq(&src->type, dest->type.ptr_target)) {
          Str ptr_target_type_str = get_type_str(dest->type.ptr_target);
          Str src_type_str = get_type_str(&src->type);
          CERRORF("Cannot assign value of type "STR_FMT" to a pointer target of type "STR_FMT"\n",
                  STR_ARG(src_type_str), STR_ARG(ptr_target_type_str));
          free_type_str(ptr_target_type_str, dest->type.ptr_target);
          free_type_str(src_type_str, &src->type);
          goto fail;
        }

        if (src->type.kind == ETypeKindStruct ||
            src->type.kind == ETypeKindArray ||
            src->type.kind == ETypeKindTuple) {
          src->moved = true;
          src->moved_loc = instr->loc;
        }
      } break;

      case EInstrKindCopyFromRef: {
        CHECK_VAR(instr->as.copy_from_ref.dest_index, instr->as.copy_from_ref.dest_name, 0, false);
        CHECK_VAR(instr->as.copy_from_ref.src_index, instr->as.copy_from_ref.src_name, 1, true);
        if (instr->as.copy_from_ref.has_offset)
          CHECK_VAR(instr->as.copy_from_ref.src_offset_index, instr->as.copy_from_ref.src_offset_name, 2, true);

        Var *dest = varss->items[i].items + instr->as.copy_from_ref.dest_index;
        dest->moved = false;
        Var *src = varss->items[i].items + instr->as.copy_from_ref.src_index;
        Var *index = NULL;
        if (instr->as.copy_from_ref.has_offset)
          index = varss->items[i].items + instr->as.copy_from_ref.src_offset_index;

        if (index && !is_int(&index->type)) {
          Str index_type_str = get_type_str(&index->type);
          CERRORF("Trying to index using "STR_FMT"\n", STR_ARG(index_type_str));
          free_type_str(index_type_str, &index->type);
          goto fail;
        }

        if (src->type.kind != ETypeKindPtr && src->type.kind != ETypeKindArray) {
          Str src_type_str = get_type_str(&src->type);
          CERRORF("Trying to dereference "STR_FMT"\n", STR_ARG(src_type_str));
          free_type_str(src_type_str, &src->type);
          goto fail;
        } else if (src->type.kind == ETypeKindPtr && !src->type.ptr_target) {
          CERROR("Trying to dereference a generic pointer\n");
          goto fail;
        } else if (dest->type.kind == ETypeKindUnit) {
          dest->type = type_clone(src->type.ptr_target);
        } else if (!type_eq(src->type.ptr_target, &dest->type)) {
          Str dest_type_str = get_type_str(&dest->type);
          Str ptr_target_type_str = get_type_str(src->type.ptr_target);
          CERRORF("Cannot assign value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(ptr_target_type_str), STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          free_type_str(ptr_target_type_str, src->type.ptr_target);
          goto fail;
        }
      } break;

      case EInstrKindStoreNull: {
        CHECK_VAR(instr->as.store_null.index, instr->as.store_null.name, 0, false);

        Var *var = varss->items[i].items + instr->as.store_null.index;
        var->moved = false;

        if (var->type.kind == ETypeKindUnit) {
          var->type.kind = ETypeKindPtr;
        } else if (var->type.kind != ETypeKindPtr) {
          Str var_type_str = get_type_str(&var->type);
          CERRORF("Cannot assign value of type pointer to a variable of type "STR_FMT"\n",
                  STR_ARG(var_type_str));
          free_type_str(var_type_str, &var->type);
          goto fail;
        }
      } break;

      case EInstrKindInlineAsm: {
        for (u32 k = 0; k < instr->as.inline_asm.segments.len; ++k) {
          EAsmSegment *segment = instr->as.inline_asm.segments.items + k;
          if (segment->kind && EAsmSegmentKindVar)
            CHECK_VAR(segment->value_index, segment->value, 0, true);
        }
      } break;

      case EInstrKindStoreStr: {
        CHECK_VAR(instr->as.store_str.index, instr->as.store_str.name, 0, false);

        Var *var = varss->items[i].items + instr->as.store_str.index;
        var->moved = false;

        if (var->type.kind == ETypeKindUnit) {
          var->type.kind = ETypeKindStr;
        } else if (var->type.kind != ETypeKindStr) {
          Str var_type_str = get_type_str(&var->type);
          CERRORF("Cannot assign value of type str to a variable of type "STR_FMT"\n",
                  STR_ARG(var_type_str));
          free_type_str(var_type_str, &var->type);
          goto fail;
        }
      } break;

      case EInstrKindCast: {
        CHECK_VAR(instr->as.cast.dest_index, instr->as.cast.dest_name, 0, false);
        CHECK_VAR(instr->as.cast.src_index, instr->as.cast.src_name, 1, true);

        Var *dest = varss->items[i].items + instr->as.cast.dest_index;
        dest->moved = false;
        Var *src = varss->items[i].items + instr->as.cast.src_index;

        if (dest->type.kind == ETypeKindUnit) {
          dest->type = type_clone(&instr->as.cast.dest_type);
        } else if (!type_eq(&instr->as.cast.dest_type, &dest->type)) {
          Str dest_type_str = get_type_str(&dest->type);
          Str new_type_str = get_type_str(&instr->as.cast.dest_type);
          CERRORF("Cannot assign value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(new_type_str), STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          free_type_str(new_type_str, &instr->as.cast.dest_type);
          goto fail;
        } else if (!can_cast(&src->type, &instr->as.cast.dest_type)) {
          Str src_type_str = get_type_str(&src->type);
          Str new_type_str = get_type_str(&instr->as.cast.dest_type);
          CERRORF("Cannot perform a "STR_FMT" -> "STR_FMT" cast\n",
                  STR_ARG(src_type_str), STR_ARG(new_type_str));
          free_type_str(src_type_str, &src->type);
          free_type_str(new_type_str, &instr->as.cast.dest_type);
          goto fail;
        }
      } break;

      case EInstrKindLenOf: {
        CHECK_VAR(instr->as.len_of.dest_index, instr->as.len_of.dest_name, 0, false);
        CHECK_VAR(instr->as.len_of.src_index, instr->as.len_of.src_name, 1, false);

        Var *dest = varss->items[i].items + instr->as.cast.dest_index;
        dest->moved = false;
        Var *src = varss->items[i].items + instr->as.cast.src_index;

        if (dest->type.kind == ETypeKindUnit) {
          dest->type = (EType) {
            ETypeKindU64,
            {},
            {},
          };
        } else if (src->type.kind != ETypeKindArray) {
          Str src_type_str = get_type_str(&src->type);
          CERRORF("Cannot take length of a value of type "STR_FMT"\n",
                  STR_ARG(src_type_str));
          free_type_str(src_type_str, &src->type);
          goto fail;
        } else if (dest->type.kind != ETypeKindU64) {
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Cannot assign value of type u64 to a variable of type "STR_FMT"\n",
                  STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          goto fail;
        }
      } break;

      case EInstrKindCopyToField: {
        CHECK_VAR(instr->as.copy_to_field.dest_index, instr->as.copy_to_field.dest_name, 0, true);
        CHECK_VAR(instr->as.copy_to_field.src_index, instr->as.copy_to_field.src_name, 1, true);

        Var *dest = varss->items[i].items + instr->as.copy_to_field.dest_index;
        Var *src = varss->items[i].items + instr->as.copy_to_field.src_index;

        EType *dest_type = &dest->type;
        if (dest_type->kind == ETypeKindPtr)
          dest_type = dest_type->ptr_target;

        if (dest_type->kind != ETypeKindStruct && dest_type->kind != ETypeKindStr) {
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Attempt to access field of something that is not a structure nor an str nor a pointer to any of these, but "STR_FMT"\n",
                  STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          goto fail;
        }

        if (dest_type->kind == ETypeKindStruct) {
          EStruct *_struct = get_struct(&ir->structs, dest_type->name);
          if (!_struct) {
            CERRORF("Structure "STR_FMT" was not defined\n",
                    STR_ARG(dest->type.name));
            goto fail;
          }

          EField *field = get_field(_struct, instr->as.copy_to_field.dest_field_name);
          if (!field) {
            CERRORF("Field "STR_FMT" does not exist in structure "STR_FMT"\n",
                    STR_ARG(instr->as.copy_to_field.dest_field_name),
                    STR_ARG(dest_type->name));
            goto fail;
          }

          if (!type_eq(&src->type, &field->type)) {
            Str field_type_str = get_type_str(&field->type);
            Str src_type_str = get_type_str(&src->type);
            CERRORF("Cannot assign value of type "STR_FMT" to a field of type "STR_FMT"\n",
                    STR_ARG(src_type_str), STR_ARG(field_type_str));
            free_type_str(field_type_str, &field->type);
            free_type_str(src_type_str, &src->type);
            goto fail;
          }

          if (src->type.kind == ETypeKindStruct ||
              src->type.kind == ETypeKindArray ||
              src->type.kind == ETypeKindTuple) {
            src->moved = true;
            src->moved_loc = instr->loc;
          }
        } else {
          bool is_ptr = str_eq(instr->as.copy_to_field.dest_field_name, STR_LIT("ptr"));
          if (!is_ptr && !str_eq(instr->as.copy_to_field.dest_field_name, STR_LIT("len"))) {
            CERRORF("Field "STR_FMT" does not exist in str\n",
                    STR_ARG(instr->as.copy_to_field.dest_field_name));
            goto fail;
          }

          if (is_ptr) {
            CERROR("`ptr` field of str type is read-only\n");
            goto fail;
          }

          EType type = { ETypeKindU32, {}, {} };
          if (!type_eq(&src->type, &type)) {
            Str field_type_str = get_type_str(&type);
            Str src_type_str = get_type_str(&src->type);
            CERRORF("Cannot assign value of type "STR_FMT" to a field of type "STR_FMT"\n",
                    STR_ARG(src_type_str), STR_ARG(field_type_str));
            free_type_str(field_type_str, &type);
            free_type_str(src_type_str, &src->type);
            goto fail;
          }

          EInstr replacement = {
            EInstrKindCopyToOffset,
            {
              .copy_to_offset = {
                instr->as.copy_to_field.dest_name,
                instr->as.copy_to_field.dest_index,
                is_ptr ? 4 : 0,
                instr->as.copy_to_field.src_name,
                instr->as.copy_to_field.src_index,
              },
            },
            instr->loc,
          };
          *instr = replacement;
        }
      } break;

      case EInstrKindCopyFromField: {
        CHECK_VAR(instr->as.copy_from_field.dest_index, instr->as.copy_from_field.dest_name, 0, false);
        CHECK_VAR(instr->as.copy_from_field.src_index, instr->as.copy_from_field.src_name, 1, true);

        Var *dest = varss->items[i].items + instr->as.copy_from_field.dest_index;
        dest->moved = false;
        Var *src = varss->items[i].items + instr->as.copy_from_field.src_index;

        EType *src_type = &src->type;
        if (src_type->kind == ETypeKindPtr)
          src_type = src_type->ptr_target;

        if (src_type->kind != ETypeKindStruct && src_type->kind != ETypeKindStr) {
          Str src_type_str = get_type_str(src_type);
          CERRORF("Attempt to access field of something that is not a structure nor an str, but "STR_FMT"\n",
                  STR_ARG(src_type_str));
          free_type_str(src_type_str, src_type);
          goto fail;
        }

        if (src_type->kind == ETypeKindStruct) {
          EStruct *_struct = get_struct(&ir->structs, src_type->name);
          if (!_struct) {
            CERRORF("Structure "STR_FMT" was not defined\n",
                    STR_ARG(src_type->name));
            goto fail;
          }

          EField *field = get_field(_struct, instr->as.copy_from_field.src_field_name);
          if (!field) {
            CERRORF("Field "STR_FMT" does not exist in structure "STR_FMT"\n",
                    STR_ARG(instr->as.copy_from_field.src_field_name),
                    STR_ARG(src_type->name));
            goto fail;
          }

          if (dest->type.kind == ETypeKindUnit) {
            dest->type = type_clone(&field->type);
          } else if (!type_eq(&field->type, &dest->type)) {
            Str dest_type_str = get_type_str(&dest->type);
            Str field_type_str = get_type_str(&field->type);
            CERRORF("Cannot assign value of field of type "STR_FMT" to a variable of type "STR_FMT"\n",
                    STR_ARG(field_type_str), STR_ARG(dest_type_str));
            free_type_str(dest_type_str, &dest->type);
            free_type_str(field_type_str, &field->type);
            goto fail;
          }
        } else {
          bool is_ptr = str_eq(instr->as.copy_from_field.src_field_name, STR_LIT("ptr"));
          if (!is_ptr && !str_eq(instr->as.copy_from_field.src_field_name, STR_LIT("len"))) {
            CERRORF("Field "STR_FMT" does not exist in str\n",
                    STR_ARG(instr->as.copy_from_field.src_field_name));
            goto fail;
          }

          EType type;
          if (is_ptr) {
            type = (EType) {
              ETypeKindPtr,
              {
                .ptr_target = malloc(sizeof(EType)),
              },
              {},
            };
            *type.ptr_target = (EType) { ETypeKindU8, {}, {} };
          } else {
            type = (EType) { ETypeKindU32, {}, {} };
          }

          if (dest->type.kind == ETypeKindUnit) {
            dest->type = type;
          } else if (!type_eq(&type, &dest->type)) {
            Str dest_type_str = get_type_str(&dest->type);
            Str field_type_str = get_type_str(&type);
            CERRORF("Cannot assign value of field of type "STR_FMT" to a variable of type "STR_FMT"\n",
                    STR_ARG(field_type_str), STR_ARG(dest_type_str));
            free_type_str(dest_type_str, &dest->type);
            free_type_str(field_type_str, &type);
            if (is_ptr)
              type_free(type.ptr_target);
            goto fail;
          } else {
            if (is_ptr)
              type_free(type.ptr_target);
          }

          if (is_ptr) {
            Var new_var = {
              {},
              { ETypeKindU64, {}, {} },
              false,
              {},
            };
            DA_APPEND(varss->items[i], new_var);
            EInstr new_instr0 = {
              EInstrKindAlloc,
              {
                .alloc = {
                  {},
                  varss->items[i].len - 1,
                },
              },
              {},
            };
            EInstr new_instr1 = {
              EInstrKindStore,
              {
                .store = {
                  {},
                  varss->items[i].len - 1,
                  {
                    ETypeKindU64,
                    {
                      ._unsigned = 4,
                    },
                  },
                },
              },
              {},
            };
            EInstr replacement = {
              EInstrKindBinOp,
              {
                .bin_op = {
                  instr->as.copy_from_field.dest_name,
                  instr->as.copy_from_field.dest_index,
                  instr->as.copy_from_field.src_name,
                  instr->as.copy_from_field.src_index,
                  {},
                  varss->items[i].len - 1,
                  EBinOpKindAdd,
                },
              },
              instr->loc,
            };
            *instr = replacement;
            DA_INSERT(proc->instrs, j, new_instr1);
            DA_INSERT(proc->instrs, j, new_instr0);
            j += 3;
          } else {
            EInstr replacement = {
              EInstrKindCopyFromOffset,
              {
                .copy_from_offset = {
                  instr->as.copy_from_field.dest_name,
                  instr->as.copy_from_field.dest_index,
                  instr->as.copy_from_field.src_name,
                  instr->as.copy_from_field.src_index,
                  is_ptr ? 4 : 0,
                },
              },
              instr->loc,
            };
            *instr = replacement;
          }
        }
      } break;

      case EInstrKindTuple: {
        CHECK_VAR(instr->as.tuple.dest_index, instr->as.tuple.dest_name, 0, false);

        Var *dest = varss->items[i].items + instr->as.copy_from_field.dest_index;
        dest->moved = false;

        ETypes field_types = {0};

        Indices *field_indices = &instr->as.tuple.field_indices;
        for (u32 k = 0; k < field_indices->len; ++k) {
          Var *field_var = varss->items[i].items + field_indices->items[k];
          EType field_type = type_clone(&field_var->type);
          DA_APPEND(field_types, field_type);
        }

        if (dest->type.kind == ETypeKindUnit) {
          dest->type.kind = ETypeKindTuple;
          dest->type.tuple_types = field_types;
        } else if (!tuple_type_matches(&dest->type, &field_types)) {
          Str dest_type_str = get_type_str(&dest->type);
          CERROR("Cannot assign value of type ");
          fprintf_tuple_type(stderr, &field_types);
          fprintf(stderr, " to a variable of type "STR_FMT"\n",
                  STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          if (field_types.items)
            free(field_types.items);
          goto fail;
        }
      } break;

      case EInstrKindCopyToOffset: {
        CHECK_VAR(instr->as.copy_to_offset.dest_index, instr->as.copy_to_offset.dest_name, 0, true);
        CHECK_VAR(instr->as.copy_to_offset.src_index, instr->as.copy_to_offset.src_name, 1, true);

        Var *dest = varss->items[i].items + instr->as.copy_to_offset.dest_index;
        Var *src = varss->items[i].items + instr->as.copy_to_offset.src_index;

        if (dest->type.kind != ETypeKindTuple) {
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Attempt to access number field of something that is not a tuple, but "STR_FMT"\n",
                  STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          goto fail;
        }

        if (instr->as.copy_to_offset.dest_offset >= dest->type.tuple_types.len) {
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Number field %u does not exist in tuple "STR_FMT"\n",
                  instr->as.copy_to_offset.dest_offset,
                  STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          goto fail;
        }

        EType *type = dest->type.tuple_types.items +
                      instr->as.copy_to_offset.dest_offset;
        if (!type_eq(&src->type, type)) {
          Str field_type_str = get_type_str(type);
          Str src_type_str = get_type_str(&src->type);
          CERRORF("Cannot assign value of type "STR_FMT" to a numbered field of type "STR_FMT"\n",
                  STR_ARG(src_type_str), STR_ARG(field_type_str));
          free_type_str(field_type_str, type);
          free_type_str(src_type_str, &src->type);
          goto fail;
        }

        if (src->type.kind == ETypeKindStruct ||
            src->type.kind == ETypeKindArray ||
            src->type.kind == ETypeKindTuple) {
          src->moved = true;
          src->moved_loc = instr->loc;
        }
      } break;

      case EInstrKindCopyFromOffset: {
        CHECK_VAR(instr->as.copy_from_offset.dest_index, instr->as.copy_from_offset.dest_name, 0, false);
        CHECK_VAR(instr->as.copy_from_offset.src_index, instr->as.copy_from_offset.src_name, 1, true);

        Var *dest = varss->items[i].items + instr->as.copy_from_offset.dest_index;
        dest->moved = false;
        Var *src = varss->items[i].items + instr->as.copy_from_offset.src_index;

        if (src->type.kind != ETypeKindTuple) {
          Str src_type_str = get_type_str(&src->type);
          CERRORF("Attempt to access numbered field of something that is not a tuple, but "STR_FMT"\n",
                  STR_ARG(src_type_str));
          free_type_str(src_type_str, &src->type);
          goto fail;
        }

        if (instr->as.copy_from_offset.src_offset >= src->type.tuple_types.len) {
          Str src_type_str = get_type_str(&src->type);
          CERRORF("Numbered ield %u does not exist in tuple "STR_FMT"\n",
                  instr->as.copy_from_offset.src_offset,
                  STR_ARG(src_type_str));
          free_type_str(src_type_str, &src->type);
          goto fail;
        }

        EType *type = src->type.tuple_types.items +
                      instr->as.copy_from_offset.src_offset;
        if (dest->type.kind == ETypeKindUnit) {
          dest->type = type_clone(type);
        } else if (!type_eq(type, &dest->type)) {
          Str dest_type_str = get_type_str(&dest->type);
          Str field_type_str = get_type_str(type);
          CERRORF("Cannot assign value of numbered field of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(field_type_str), STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          free_type_str(field_type_str, type);
          goto fail;
        }
      } break;
      }
    }

    if (proc->return_type.kind != ETypeKindUnit &&
        (proc->instrs.len == 0 ||
         proc->instrs.items[proc->instrs.len - 1].kind != EInstrKindRetVal)) {
      ERROR("`"STR_FMT"` has no trailing return with a value\n", STR_ARG(proc->name));
      goto fail;
    }

    for (u32 j = 0; j < var_substs.len; ++j)
      substitute_var_uses_with_its_src(proc, var_substs.items + j);
    var_substs.len = 0;
  }

  if (require_main && !found_main) {
    ERROR("`main` procedure was not found\n");
    goto fail;
  }

  if (var_substs.items)
    free(var_substs.items);
  return true;
fail:
  if (var_substs.items)
    free(var_substs.items);
  return false;
}
