#include "ir-print.h"

static void print_value_params(ValueKind kind, u32 size) {
  printf("%s %u", kind == ValueKindSigned ? "signed" : "unsigned", size);
}

static char *get_bin_op_kind_cstr(BinOpKind kind) {
  switch (kind) {
  case BinOpKindAddInt: return "+";
  case BinOpKindSubInt: return "-";
  case BinOpKindMulInt: return "*";
  case BinOpKindDivInt: return "/";
  case BinOpKindRem:    return "%";
  case BinOpKindAnd:    return "&";
  case BinOpKindOr:     return "|";
  case BinOpKindXor:    return "^";
  case BinOpKindLShift: return "<<";
  case BinOpKindRShift: return ">>";
  case BinOpKindEqInt:  return "==";
  case BinOpKindNeInt:  return "!=";
  case BinOpKindLsInt:  return "<";
  case BinOpKindLeInt:  return "<=";
  case BinOpKindGtInt:  return ">";
  case BinOpKindGeInt:  return ">=";
  }

  return NULL;
}

void proc_print(Proc *proc, VarLocs *locs, u8 *labels) {
  printf("proc "STR_FMT"(", STR_ARG(proc->name));
  for (u32 i = 0; i < proc->args.len; ++i) {
    if (i > 0)
      printf(", ");
    printf("$%u: ", i);
    print_value_params(proc->args.items[i].kind, proc->args.items[i].size);
  }
  printf(") -> ");
  print_value_params(proc->return_kind, proc->return_size);
  putc('\n', stdout);

  for (u32 i = 0; i < proc->instrs.len; ++i) {
    Instr *instr = proc->instrs.items + i;

    if (labels[i])
      printf("l%u:\n", i);

    switch (instr->kind) {
    case InstrKindAlloc: {
      if (locs->items[instr->as.alloc.index].uses > 0) {
        AlignedSegment *last_segment = instr->as.alloc.segments.items + instr->as.alloc.segments.len - 1;
        printf("  $%u = alloc %u\n", instr->as.alloc.index,
               last_segment->offset + last_segment->size);
      }
    } break;

    case InstrKindStore: {
      printf("  $%u = ", instr->as.store.index);
      switch (instr->as.store.value.kind) {
      case ValueKindSigned: {
        printf("signed %ld\n", instr->as.store.value.as._signed);
      } break;

      case ValueKindUnsigned: {
        printf("unsigned %li\n", instr->as.store.value.as._unsigned);
      } break;
      }
    } break;

    case InstrKindCopy: {
      printf("  $%u = $%u\n", instr->as.copy.dest_index, instr->as.copy.src_index);
    } break;

    case InstrKindBinOp: {
      printf("  $%u = $%u %s $%u\n",
             instr->as.bin_op.dest_index,
             instr->as.bin_op.src0_index,
             get_bin_op_kind_cstr(instr->as.bin_op.kind),
             instr->as.bin_op.src1_index);
    } break;

    case InstrKindCall: {
      printf("  "STR_FMT"(", STR_ARG(instr->as.call.name));
      for (u32 k = 0; k < instr->as.call.arg_indices.len; ++k) {
        if (k > 0)
          printf(", ");
        printf("$%u", instr->as.call.arg_indices.items[k]);
      }
      printf(")\n");
    } break;

    case InstrKindCallAssign: {
      printf("  $%u = "STR_FMT"(",
             instr->as.call_assign.dest_index,
             STR_ARG(instr->as.call_assign.name));
      for (u32 k = 0; k < instr->as.call_assign.arg_indices.len; ++k) {
        if (k > 0)
          printf(", ");
        printf("$%u", instr->as.call_assign.arg_indices.items[k]);
      }
      printf(")\n");
    } break;

    case InstrKindRet: {
      printf("  ret\n");
    } break;

    case InstrKindRetVal: {
      printf("  retval $%u\n", instr->as.ret_val.index);
    } break;

    case InstrKindJump: {
      printf("  jump %u\n", instr->as.jump.target);
    } break;

    case InstrKindJumpIfNot: {
      printf("  jump %u if $%u == 0\n",
             instr->as.jump_if_not.target,
             instr->as.jump_if_not.cond_index);
    } break;

    case InstrKindRef: {
      printf("  $%u = &$%u\n",
             instr->as.ref.dest_index,
             instr->as.ref.src_index);
    } break;

    case InstrKindCopyToRef: {
      if (instr->as.copy_to_ref.dest_offset_index == (u32) -1)
        printf("  $%u := $%u\n",
               instr->as.copy_to_ref.dest_index,
               instr->as.copy_to_ref.src_index);
      else
        printf("  $%u[$%u] := $%u\n",
               instr->as.copy_to_ref.dest_index,
               instr->as.copy_to_ref.dest_offset_index,
               instr->as.copy_to_ref.src_index);
    } break;

    case InstrKindCopyFromRef: {
      if (instr->as.copy_from_ref.take_ref) {
        if (instr->as.copy_from_ref.src_offset_index == (u32) -1)
          printf("  $%u = &$%u\n",
                 instr->as.copy_from_ref.dest_index,
                 instr->as.copy_from_ref.src_index);
        else
          printf("  $%u = &$%u[$%u]\n",
                 instr->as.copy_from_ref.dest_index,
                 instr->as.copy_from_ref.src_index,
                 instr->as.copy_from_ref.src_offset_index);
      } else {
        if (instr->as.copy_from_ref.src_offset_index == (u32) -1)
          printf("  $%u = *$%u\n",
                 instr->as.copy_from_ref.dest_index,
                 instr->as.copy_from_ref.src_index);
        else
          printf("  $%u = $%u[$%u]\n",
                 instr->as.copy_from_ref.dest_index,
                 instr->as.copy_from_ref.src_index,
                 instr->as.copy_from_ref.src_offset_index);
      }
    } break;

    case InstrKindInlineAsm: {
      printf("  asm ");
      for (u32 k = 0; k < instr->as.inline_asm.segments.len; ++k) {
        AsmSegment *segment = instr->as.inline_asm.segments.items + k;
        if (k > 0)
          printf(", ");
        if (segment->kind == AsmSegmentKindStr)
          printf("\""STR_FMT"\"", STR_ARG(segment->value));
        else
          printf("$%u", segment->index);
      }
      printf("\n");
    } break;

    case InstrKindStoreData: {
      printf("  $%u = data %u\n",
             instr->as.store_data.index,
             instr->as.store_data.data_index);
    } break;

    case InstrKindConvert: {
      printf("  $%u = cast $%u -> ",
             instr->as.convert.dest_index,
             instr->as.convert.src_index);
      print_value_params(instr->as.convert.dest_kind,
                         instr->as.convert.dest_size);
      putc('\n', stdout);
    } break;

    case InstrKindCopyToRefFixed: {
      i32 offset = instr->as.copy_to_ref_fixed.dest_segments.items[instr->as.copy_to_ref_fixed.dest_segments.len - 1].offset;
      printf("  $%u[%d] := $%u\n",
             instr->as.copy_to_ref_fixed.dest_index,
             offset,
             instr->as.copy_to_ref_fixed.src_index);
    } break;

    case InstrKindCopyFromRefFixed: {
      i32 offset = instr->as.copy_from_ref_fixed.src_segments.items[instr->as.copy_from_ref_fixed.src_segments.len - 1].offset;
      printf("  $%u = ", instr->as.copy_from_ref_fixed.dest_index);
      if (instr->as.copy_from_ref_fixed.take_ref)
        putc('&', stdout);
      printf("$%u[%d]\n",
             instr->as.copy_from_ref_fixed.src_index,
             offset);
    } break;

    case InstrKindRefProc: {
      printf("  $%u = &"STR_FMT"\n",
             instr->as.ref_proc.dest_index,
             STR_ARG(instr->as.ref_proc.proc_name));
    } break;

    case InstrKindCallRef: {
      printf("  $%u(", instr->as.call_ref.index);
      for (u32 k = 0; k < instr->as.call_ref.arg_indices.len; ++k) {
        if (k > 0)
          printf(", ");
        printf("$%u", instr->as.call_ref.arg_indices.items[k]);
      }
      printf(")\n");
    } break;

    case InstrKindCallRefAssign: {
      printf("  $%u = $%u(",
             instr->as.call_ref_assign.dest_index,
             instr->as.call_ref_assign.index);
      for (u32 k = 0; k < instr->as.call_ref_assign.arg_indices.len; ++k) {
        if (k > 0)
          printf(", ");
        printf("$%u", instr->as.call_ref_assign.arg_indices.items[k]);
      }
      printf(")\n");
    } break;
    }
  }

  printf("end\n");
}
