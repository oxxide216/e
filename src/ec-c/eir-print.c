#include "eir-print.h"

static void var_print(u32 index, Str name) {
  if (name.len == 0)
    printf("$%u", index);
  else
    printf(STR_FMT, STR_ARG(name));
}

static void value_print(EValue *value) {
  switch (value->kind) {
  case ETypeKindUnit: {
    printf("unit");
  } break;

  case ETypeKindS8:
  case ETypeKindS16:
  case ETypeKindS32:
  case ETypeKindS64: {
    printf("%ld", value->as._signed);
  } break;

  case ETypeKindU8:
  case ETypeKindU16:
  case ETypeKindU32:
  case ETypeKindU64: {
    printf("%lu", value->as._unsigned);
  } break;

  case ETypeKindBool: {
    printf("%s", value->as._bool ? "true" : "false");
  } break;

  case ETypeKindStruct: {
    printf("struct");
  } break;

  case ETypeKindArray: {
    printf("array");
  } break;

  case ETypeKindTuple: {
    printf("tuple");
  } break;

  case ETypeKindPtr: {
    printf("pointer");
  } break;
  }
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

  return NULL;
}

void proc_print(EProc *proc) {
  printf("proc "STR_FMT"(", STR_ARG(proc->name));
  for (u32 i = 0; i < proc->args.len; ++i) {
    if (i > 0)
      printf(", ");
    printf(STR_FMT, STR_ARG(proc->args.items[i].name));
  }
  printf(")\n");

  for (u32 i = 0; i < proc->instrs.len; ++i) {
    EInstr *instr = proc->instrs.items + i;

    switch (instr->kind) {
    case EInstrKindAlloc: {
      printf("  ");
      var_print(instr->as.alloc.index, instr->as.alloc.name);
      printf(" = alloc\n");
    } break;

    case EInstrKindStore: {
      printf("  ");
      var_print(instr->as.store.index, instr->as.store.name);
      printf(" = ");
      value_print(&instr->as.store.value);
      printf("\n");
    } break;

    case EInstrKindCopy: {
      printf("  ");
      var_print(instr->as.copy.dest_index, instr->as.copy.dest_name);
      printf(" = ");
      var_print(instr->as.copy.src_index, instr->as.copy.src_name);
      printf("\n");
    } break;

    case EInstrKindBinOp: {
      printf("  ");
      var_print(instr->as.bin_op.dest_index, instr->as.bin_op.dest_name);
      printf(" = ");
      var_print(instr->as.bin_op.src0_index, instr->as.bin_op.src0_name);
      printf(" %s ", get_bin_op_kind_cstr(instr->as.bin_op.kind));
      var_print(instr->as.bin_op.src1_index, instr->as.bin_op.src1_name);
      printf("\n");
    } break;

    case EInstrKindCall: {
      printf(STR_FMT"(", STR_ARG(instr->as.call.name));
      for (u32 j = 0; j < instr->as.call.arg_indices.len; ++j) {
        if (j > 0)
          printf(", ");
        printf("$%u", instr->as.call.arg_indices.items[j]);
      }
      printf(")\n");
    } break;

    case EInstrKindCallAssign: {
      printf("  ");
      var_print(instr->as.call_assign.dest_index, instr->as.call_assign.dest_name);
      printf(" = "STR_FMT"(", STR_ARG(instr->as.call_assign.name));
      for (u32 j = 0; j < instr->as.call_assign.arg_indices.len; ++j) {
        if (j > 0)
          printf(", ");
        printf("$%u", instr->as.call_assign.arg_indices.items[j]);
      }
      printf(")\n");
    } break;

    case EInstrKindRet: {
      printf("  ret\n");
    } break;

    case EInstrKindRetVal: {
      printf("  retval $%u\n", instr->as.ret_val.index);
    } break;

    case EInstrKindJump: {
      printf("  jump to %u\n", instr->as.jump.target);
    } break;

    case EInstrKindJumpIfNot: {
      printf("  jump to %u if $%u == 0\n",
             instr->as.jump_if_not.target,
             instr->as.jump_if_not.cond_index);
    } break;

    case EInstrKindRef: {
      printf("  ");
      var_print(instr->as.ref.dest_index, instr->as.ref.dest_name);
      printf(" = &");
      var_print(instr->as.ref.src_index, instr->as.ref.src_name);
      printf("\n");
    } break;

    case EInstrKindCopyToRef: {
      if (instr->as.copy_to_ref.has_offset) {
        printf("  ");
        var_print(instr->as.copy_to_ref.dest_index, instr->as.copy_to_ref.dest_name);
        printf("[");
        var_print(instr->as.copy_to_ref.dest_offset_index, instr->as.copy_to_ref.dest_offset_name);
        printf("] := &");
        var_print(instr->as.copy_to_ref.src_index, instr->as.copy_to_ref.src_name);
        printf("\n");
      } else {
        printf("  ");
        var_print(instr->as.copy_to_ref.dest_index, instr->as.copy_to_ref.dest_name);
        printf(" := ");
        var_print(instr->as.copy_to_ref.src_index, instr->as.copy_to_ref.src_name);
        printf("\n");
      }
    } break;

    case EInstrKindCopyFromRef: {
      if (instr->as.copy_from_ref.has_offset) {
        printf("  ");
        var_print(instr->as.copy_from_ref.dest_index, instr->as.copy_from_ref.dest_name);
        printf(" := ");
        var_print(instr->as.copy_from_ref.src_index, instr->as.copy_from_ref.src_name);
        printf("[");
        var_print(instr->as.copy_from_ref.src_offset_index, instr->as.copy_from_ref.src_offset_name);
        printf("]\n");
      } else {
        printf("  ");
        var_print(instr->as.copy_from_ref.dest_index, instr->as.copy_from_ref.dest_name);
        printf(" = *");
        var_print(instr->as.copy_from_ref.src_index, instr->as.copy_from_ref.src_name);
        printf("\n");
      }
    } break;

    case EInstrKindStoreNull: {
      printf("  ");
      var_print(instr->as.store.index, instr->as.store.name);
      printf(" = null\n");
    } break;

    case EInstrKindInlineAsm: {
      printf("  asm\n");
    } break;

    case EInstrKindStoreData: {
      printf("  ");
      var_print(instr->as.store_data.index, instr->as.store_data.name);
      printf(" = data %u\n", instr->as.store_data.data_index);
    } break;

    case EInstrKindCast: {
      printf("  ");
      var_print(instr->as.cast.dest_index, instr->as.cast.dest_name);
      printf(" = cast ");
      var_print(instr->as.cast.src_index, instr->as.cast.src_name);
      printf("\n");
    } break;

    case EInstrKindLenOf: {
      printf("  ");
      var_print(instr->as.len_of.dest_index, instr->as.len_of.dest_name);
      printf(" = lenof ");
      var_print(instr->as.len_of.src_index, instr->as.len_of.src_name);
      printf("\n");
    } break;

    case EInstrKindCopyToField: {
      printf("  ");
      var_print(instr->as.copy_to_field.dest_index, instr->as.copy_to_field.dest_name);
      printf("."STR_FMT" = ", STR_ARG(instr->as.copy_to_field.dest_field_name));
      var_print(instr->as.copy_to_field.src_index, instr->as.copy_to_field.src_name);
      printf("\n");
    } break;

    case EInstrKindCopyFromField: {
      printf("  ");
      var_print(instr->as.copy_from_field.dest_index, instr->as.copy_from_field.dest_name);
      printf(" = ");
      var_print(instr->as.copy_from_field.src_index, instr->as.copy_from_field.src_name);
      printf("."STR_FMT"\n", STR_ARG(instr->as.copy_from_field.src_field_name));
    } break;

    case EInstrKindTuple: {
      printf("  ");
      var_print(instr->as.tuple.dest_index, instr->as.tuple.dest_name);
      printf(" = tuple\n");
    } break;

    case EInstrKindCopyToOffset: {
      printf("  ");
      var_print(instr->as.copy_to_offset.dest_index, instr->as.copy_to_offset.dest_name);
      printf(".%u = ", instr->as.copy_to_offset.dest_offset);
      var_print(instr->as.copy_to_offset.src_index, instr->as.copy_to_offset.src_name);
      printf("\n");
    } break;

    case EInstrKindCopyFromOffset: {
      printf("  ");
      var_print(instr->as.copy_from_offset.dest_index, instr->as.copy_from_offset.dest_name);
      printf(" = ");
      var_print(instr->as.copy_from_offset.src_index, instr->as.copy_from_offset.src_name);
      printf(".%u\n", instr->as.copy_from_offset.src_offset);
    } break;
    }
  }

  printf("end\n");
}
