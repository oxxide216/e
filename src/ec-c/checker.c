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

typedef Da(EType *) ETypeRefs;

static bool check_struct_existence(EStructs *structs, EType *type) {
  if (type->kind == ETypeKindStruct) {
    if (!get_struct(structs, type->name)) {
      CERRORFT("Type "STR_FMT" was not defined\n", STR_ARG(type->name));
      return false;
    }
  } else if (type->kind == ETypeKindPtr) {
    return check_struct_existence(structs, type->ptr_target);
  }

  return true;
}

static EType make_type_from_kind(ETypeKind kind) {
  return (EType) { kind, {}, NULL, {} };
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

    case ETypeKindPtr: {
      Str target_str = get_type_str(type->ptr_target);
      Str result;
      result.len = target_str.len + 1;
      result.ptr = malloc(result.len);
      result.ptr[0] = '&';
      memcpy(result.ptr + 1, target_str.ptr, target_str.len);
      return result;
    }
    }
  }

  ERROR("Unreachable\n");
  return (Str) {0};
}

static void free_type_str(Str str, EType *type) {
  if (type->kind == ETypeKindPtr)
    free(str.ptr);
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

static EType *get_proc_return_type(EProcs *procs, Str name, ETypeRefs *arg_types) {
  for (u32 i = 0; i < procs->len; ++i) {
    EProc *proc = procs->items + i;

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
    if (a->kind == ETypeKindPtr && is_int(b))
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
  return (src->kind == dest->kind && src->kind != ETypeKindStruct) ||
         (is_int(src) && is_int(dest)) ||
         (is_int(src) && dest->kind == ETypeKindBool) ||
         (is_int(src) && dest->kind == ETypeKindPtr) ||
         (dest->kind == ETypeKindBool && is_int(src)) ||
         (dest->kind == ETypeKindPtr && is_int(src));
}

bool check_ir(EIr *ir, Varss *varss) {
  bool found_main = false;

  for (u32 i = 0; i < ir->structs.len; ++i) {
    EStruct *_struct = ir->structs.items + i;

    for (u32 j = 0; j < _struct->fields.len; ++j)
      check_struct_existence(&ir->structs, &_struct->fields.items[j].type);
  }

  for (u32 i = 0; i < ir->procs.len; ++i) {
    EProc *proc = ir->procs.items + i;

    if (str_eq(proc->name, STR_LIT("main")))
      found_main = true;

    for (u32 j = 0; j < proc->args.len; ++j)
      check_struct_existence(&ir->structs, &proc->args.items[j].type);
    check_struct_existence(&ir->structs, &proc->return_type);

    for (u32 j = 0; j < proc->instrs.len; ++j) {
      EInstr *instr = proc->instrs.items + j;

      switch (instr->kind) {
      case EInstrKindAlloc: break;

      case EInstrKindStore: {
        if (instr->as.store.index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.store.name));
          return false;
        }

        Var *var = varss->items[i].items + instr->as.store.index;
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
        }
      } break;

      case EInstrKindCopy: {
        if (instr->as.copy.dest_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.copy.dest_name));
          return false;
        }
        if (instr->as.copy.src_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.copy.src_name));
          return false;
        }

        Var *dest = varss->items[i].items + instr->as.copy.dest_index;
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
          return false;
        }
      } break;

      case EInstrKindBinOp: {
        if (instr->as.bin_op.dest_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.bin_op.dest_name));
          return false;
        }
        if (instr->as.bin_op.src0_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.bin_op.src0_name));
          return false;
        }
        if (instr->as.bin_op.src1_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.bin_op.src1_name));
          return false;
        }

        Var *dest = varss->items[i].items + instr->as.bin_op.dest_index;
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
          return false;
        }

        if (dest->type.kind == ETypeKindUnit) {
          if (instr->as.bin_op.kind >= EBinOpKindEq &&
              instr->as.bin_op.kind <= EBinOpKindGe)
            dest->type = (EType) { ETypeKindBool, {}, NULL, {} };
          else
            dest->type = type_clone(&src0->type);
        } else if (!type_eq(&src0->type, &dest->type)) {
          Str src0_type_str = get_type_str(&src0->type);
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Cannot assign value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(src0_type_str), STR_ARG(dest_type_str));
          free_type_str(src0_type_str, &src0->type);
          free_type_str(dest_type_str, &dest->type);
          return false;
        }
      } break;

      case EInstrKindCall: {
        ETypeRefs arg_types = {0};
        for (u32 k = 0; k < instr->as.call.arg_indices.len; ++k)
          DA_APPEND(arg_types, &varss->items[i].items[instr->as.call.arg_indices.items[k]].type);

        EType *return_type = get_proc_return_type(&ir->procs, instr->as.call.name, &arg_types);
        if (!return_type) {
          CERROR("Procedure ");
          fprintf_proc_signature(stderr, instr->as.call.name, &arg_types);
          fprintf(stderr, " was not declared or defined\n");
          if (arg_types.items)
            free(arg_types.items);
          return false;
        }

        if (arg_types.items)
          free(arg_types.items);
      } break;

      case EInstrKindCallAssign: {
        if (instr->as.call_assign.dest_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.call_assign.dest_name));
          return false;
        }

        ETypeRefs arg_types = {0};
        for (u32 k = 0; k < instr->as.call_assign.arg_indices.len; ++k)
          DA_APPEND(arg_types, &varss->items[i].items[instr->as.call_assign.arg_indices.items[k]].type);

        EType *return_type = get_proc_return_type(&ir->procs, instr->as.call_assign.name, &arg_types);
        if (!return_type) {
          CERROR("Procedure ");
          fprintf_proc_signature(stderr, instr->as.call_assign.name, &arg_types);
          fprintf(stderr, " was not declared or defined\n");
          if (arg_types.items)
            free(arg_types.items);
          return false;
        }

        Var *dest = varss->items[i].items + instr->as.call_assign.dest_index;
        if (dest->type.kind == ETypeKindUnit) {
          dest->type = type_clone(return_type);
        } else if (!type_eq(return_type, &dest->type)) {
          Str return_type_str = get_type_str(return_type);
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Cannot assign return value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(return_type_str), STR_ARG(dest_type_str));
          free_type_str(return_type_str, return_type);
          free_type_str(dest_type_str, &dest->type);
          return false;
        }

        if (arg_types.items)
          free(arg_types.items);
      } break;

      case EInstrKindRet: {
        if (proc->return_type.kind != ETypeKindUnit) {
          Str return_type_str = get_type_str(&proc->return_type);
          CERRORF("Unexpected return type: unit, expected "STR_FMT"\n",
                  STR_ARG(return_type_str));
          free_type_str(return_type_str, &proc->return_type);
          return false;
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
          return false;
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
          return false;
        }
      } break;

      case EInstrKindRef: {
        if (instr->as.ref.dest_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.ref.dest_name));
          return false;
        }
        if (instr->as.ref.src_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.ref.src_name));
          return false;
        }

        Var *dest = varss->items[i].items + instr->as.ref.dest_index;
        Var *src = varss->items[i].items + instr->as.ref.src_index;

        EType ptr_type = {
          ETypeKindPtr,
          {},
          malloc(sizeof(EType)),
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
          return false;
        } else {
          type_free(ptr_type.ptr_target);
        }
      } break;

      case EInstrKindCopyToRef: {
        if (instr->as.copy_to_ref.dest_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.copy_to_ref.dest_name));
          return false;
        }
        if (instr->as.copy_to_ref.src_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.copy_to_ref.src_name));
          return false;
        }

        Var *dest = varss->items[i].items + instr->as.copy_to_ref.dest_index;
        Var *src = varss->items[i].items + instr->as.copy_to_ref.src_index;

        if (dest->type.kind != ETypeKindPtr) {
          Str dest_type_str = get_type_str(&dest->type);
          CERRORF("Trying to dereference "STR_FMT"\n", STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          return false;
        } else if (!dest->type.ptr_target) {
          dest->type.ptr_target = malloc(sizeof(EType));
          *dest->type.ptr_target = type_clone(&src->type);
        } else if (!type_eq(&src->type, dest->type.ptr_target)) {
          Str ptr_target_type_str = get_type_str(dest->type.ptr_target);
          Str src_type_str = get_type_str(&src->type);
          CERRORF("Cannot assign value of type "STR_FMT" to a pointer target of type "STR_FMT"\n",
                  STR_ARG(src_type_str), STR_ARG(ptr_target_type_str));
          free_type_str(ptr_target_type_str, dest->type.ptr_target);
          free_type_str(src_type_str, &src->type);
          return false;
        }
      } break;

      case EInstrKindCopyFromRef: {
        if (instr->as.copy_from_ref.dest_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.copy_from_ref.dest_name));
          return false;
        }
        if (instr->as.copy_from_ref.src_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.copy_from_ref.src_name));
          return false;
        }

        Var *dest = varss->items[i].items + instr->as.copy_from_ref.dest_index;
        Var *src = varss->items[i].items + instr->as.copy_from_ref.src_index;

        if (src->type.kind != ETypeKindPtr) {
          Str src_type_str = get_type_str(&src->type);
          CERRORF("Trying to dereference "STR_FMT"\n", STR_ARG(src_type_str));
          free_type_str(src_type_str, &src->type);
          return false;
        } else if (!src->type.ptr_target) {
          CERROR("Trying to dereference a generic pointer\n");
          return false;
        } else if (dest->type.kind == ETypeKindUnit) {
          dest->type = type_clone(src->type.ptr_target);
        } else if (!type_eq(src->type.ptr_target, &dest->type)) {
          Str dest_type_str = get_type_str(&dest->type);
          Str ptr_target_type_str = get_type_str(src->type.ptr_target);
          CERRORF("Cannot assign value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(ptr_target_type_str), STR_ARG(dest_type_str));
          free_type_str(dest_type_str, &dest->type);
          free_type_str(ptr_target_type_str, src->type.ptr_target);
          return false;
        }
      } break;

      case EInstrKindStoreNull: {
        if (instr->as.store_null.index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.store_null.name));
          return false;
        }

        Var *var = varss->items[i].items + instr->as.store_null.index;

        if (var->type.kind == ETypeKindUnit) {
          var->type.kind = ETypeKindPtr;
        } else if (var->type.kind != ETypeKindPtr) {
          Str var_type_str = get_type_str(&var->type);
          CERRORF("Cannot assign value of type pointer to a variable of type "STR_FMT"\n",
                  STR_ARG(var_type_str));
          free_type_str(var_type_str, &var->type);
          return false;
        }
      } break;

      case EInstrKindInlineAsm: {
        for (u32 k = 0; k < instr->as.inline_asm.segments.len; ++k) {
          EAsmSegment *segment = instr->as.inline_asm.segments.items + k;
          if (segment->kind && EAsmSegmentKindVar && segment->value_index == (u32) -1) {
            CERRORF("Variable "STR_FMT" was not defined before usage\n",
                    STR_ARG(segment->value));
            return false;
          }
        }
      } break;

      case EInstrKindStoreData: {
        if (instr->as.store_data.index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.store_data.name));
          return false;
        }

        Var *var = varss->items[i].items + instr->as.store_data.index;

        EType ptr_type = {
          ETypeKindPtr,
          {},
          malloc(sizeof(EType)),
          {},
        };
        *ptr_type.ptr_target = (EType) { ETypeKindU8, {}, NULL, {} };

        if (var->type.kind == ETypeKindUnit) {
          var->type = ptr_type;
        } else if (!type_eq(&ptr_type, &var->type)) {
          Str var_type_str = get_type_str(&var->type);
          Str ptr_type_str = get_type_str(&ptr_type);
          CERRORF("Cannot assign value of type "STR_FMT" to a variable of type "STR_FMT"\n",
                  STR_ARG(ptr_type_str), STR_ARG(var_type_str));
          free_type_str(var_type_str, &var->type);
          free_type_str(ptr_type_str, &ptr_type);
          type_free(ptr_type.ptr_target);
          return false;
        } else {
          type_free(ptr_type.ptr_target);
        }
      } break;

      case EInstrKindCast: {
        if (instr->as.cast.dest_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.cast.dest_name));
          return false;
        }
        if (instr->as.cast.src_index == (u32) -1) {
          CERRORF("Variable "STR_FMT" was not defined before usage\n",
                  STR_ARG(instr->as.cast.src_name));
          return false;
        }

        Var *dest = varss->items[i].items + instr->as.cast.dest_index;
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
          return false;
        } else if (!can_cast(&src->type, &instr->as.cast.dest_type)) {
          Str src_type_str = get_type_str(&src->type);
          Str new_type_str = get_type_str(&instr->as.cast.dest_type);
          CERRORF("Cannot perform a "STR_FMT" -> "STR_FMT" cast\n",
                  STR_ARG(src_type_str), STR_ARG(new_type_str));
          free_type_str(src_type_str, &src->type);
          free_type_str(new_type_str, &instr->as.cast.dest_type);
          return false;
        }
      } break;
      }
    }

    if (proc->return_type.kind != ETypeKindUnit &&
        (proc->instrs.len == 0 ||
         proc->instrs.items[proc->instrs.len - 1].kind != EInstrKindRetVal)) {
      ERROR("`"STR_FMT"` has no trailing return with a value\n", STR_ARG(proc->name));
      return false;
    }
  }

  if (!found_main) {
    ERROR("`main` procedure was not found\n");
    return false;
  }

  return true;
}
