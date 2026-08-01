#include "utils.h"

void write_cstr(FILE *stream, char *cstr) {
  fwrite(cstr, 1, strlen(cstr), stream);
}

void write_str(FILE *stream, Str str) {
  fwrite(str.ptr, 1, str.len, stream);
}

void write_value(FILE *stream, Value *value) {
  switch (value->kind) {
  case ValueKindSigned: {
    fprintf(stream, "%ld", value->as._signed);
  } break;

  case ValueKindUnsigned: {
    fprintf(stream, "%lu", value->as._unsigned);
  } break;
  }
}

void get_proc_return_kind_and_size(Procs *procs, Str name, ValueKind *kind, u32 *size) {
  for (u32 i = 0; i < procs->len; ++i) {
    if (str_eq(procs->items[i].name, name)) {
      *kind = procs->items[i].return_kind;
      *size = procs->items[i].return_size;
      return;
    }
  }
}

i32 align(i32 value, i32 base) {
  if (value % base == 0)
    return value;
  return value + base - value % base;
}

bool is_jump(Instr *instr) {
  return instr->kind == InstrKindJump || instr->kind == InstrKindJumpIfNot;
}

u32 *get_dest(Instr *instr) {
  switch (instr->kind) {
  case InstrKindAlloc: {
    return &instr->as.alloc.index;
  }

  case InstrKindStore: {
    return &instr->as.store.index;
  }

  case InstrKindCopy: {
    return &instr->as.copy.dest_index;
  }

  case InstrKindBinOp: {
    return &instr->as.bin_op.dest_index;
  } break;

  case InstrKindCall: break;

  case InstrKindCallAssign: {
    return &instr->as.call_assign.dest_index;
  }

  case InstrKindRet:       break;
  case InstrKindRetVal:    break;
  case InstrKindJump:      break;
  case InstrKindJumpIfNot: break;

  case InstrKindRef: {
    return &instr->as.ref.dest_index;
  }

  case InstrKindCopyToRef: break;

  case InstrKindCopyFromRef: {
    return &instr->as.copy_from_ref.dest_index;
  }

  case InstrKindInlineAsm: break;

  case InstrKindStoreData: {
    return &instr->as.store_data.index;
  }

  case InstrKindConvert: {
    return &instr->as.convert.dest_index;
  }

  case InstrKindCopyToRefFixed: break;

  case InstrKindCopyFromRefFixed: {
    return &instr->as.copy_from_ref_fixed.dest_index;
  }

  case InstrKindRefProc: {
    return &instr->as.ref_proc.dest_index;
  }

  case InstrKindCallRef: break;

  case InstrKindCallRefAssign: {
    return &instr->as.call_ref_assign.dest_index;
  }
  }

  return NULL;
}

u32 *get_nth_arg(Instr *instr, u32 n) {
  switch (instr->kind) {
  case InstrKindAlloc: break;

  case InstrKindStore: break;

  case InstrKindCopy: {
    if (n == 0)
      return &instr->as.copy.src_index;
  } break;

  case InstrKindBinOp: {
    if (n == 0)
      return &instr->as.bin_op.src0_index;
    if (n == 1)
      return &instr->as.bin_op.src1_index;
  } break;

  case InstrKindCall: {
    if (n < instr->as.call.arg_indices.len)
      return instr->as.call.arg_indices.items + n;
  } break;

  case InstrKindCallAssign: {
    if (n < instr->as.call_assign.arg_indices.len)
      return instr->as.call_assign.arg_indices.items + n;
  } break;

  case InstrKindRet: break;

  case InstrKindRetVal: {
    if (n == 0)
      return &instr->as.ret_val.index;
  } break;

  case InstrKindJump: break;

  case InstrKindJumpIfNot: {
    if (n == 0)
      return &instr->as.jump_if_not.cond_index;
  } break;

  case InstrKindRef: {
    if (n == 0)
      return &instr->as.ref.src_index;
  } break;

  case InstrKindCopyToRef: {
    if (n == 0)
      return &instr->as.copy_to_ref.dest_index;
    if (n == 1)
      return &instr->as.copy_to_ref.dest_offset_index;
    if (n == 2)
      return &instr->as.copy_to_ref.src_index;
  } break;

  case InstrKindCopyFromRef: {
    if (n == 0)
      return &instr->as.copy_from_ref.src_index;
    if (n == 1)
      return &instr->as.copy_from_ref.src_offset_index;
  } break;

  case InstrKindInlineAsm: {
    u32 len = n <= instr->as.inline_asm.segments.len ? n : instr->as.inline_asm.segments.len;
    u32 i = 0;
    for (u32 j = 0; j < len; ++j)
      if (instr->as.inline_asm.segments.items[j].kind == AsmSegmentKindVar && i++ == n)
        return &instr->as.inline_asm.segments.items[j].index;
  } break;

  case InstrKindStoreData: break;

  case InstrKindConvert: {
    if (n == 0)
      return &instr->as.convert.src_index;
  } break;

  case InstrKindCopyToRefFixed: {
    if (n == 0)
      return &instr->as.copy_to_ref_fixed.dest_index;
    if (n == 1)
      return &instr->as.copy_to_ref_fixed.src_index;
  } break;

  case InstrKindCopyFromRefFixed: {
    if (n == 0)
      return &instr->as.copy_from_ref_fixed.src_index;
  } break;

  case InstrKindRefProc: break;

  case InstrKindCallRef: {
    if (n < instr->as.call_ref.arg_indices.len)
      return instr->as.call_ref.arg_indices.items + n;
  } break;

  case InstrKindCallRefAssign: {
    if (n < instr->as.call_ref_assign.arg_indices.len)
      return instr->as.call_ref_assign.arg_indices.items + n;
  } break;
  }

  return NULL;
}

void align_fixed_offsets(Proc *proc, AlignmentFunc alignment_func) {
  for (u32 i = 0; i < proc->instrs.len; ++i) {
    Instr *instr = proc->instrs.items + i;

    Segments *offsets = NULL;
    if (instr->kind == InstrKindCopyToRefFixed)
      offsets = &instr->as.copy_to_ref_fixed.dest_segments;
    else if (instr->kind == InstrKindCopyFromRefFixed)
      offsets = &instr->as.copy_from_ref_fixed.src_segments;
    else
      continue;

    i32 offset = 0;
    for (u32 j = 0; j < offsets->len; ++j) {
      i32 alignment = alignment_func(offsets->items[j].size);
      offsets->items[j].offset = align(offsets->items[j].offset, alignment);
      offset += offsets->items[j].size;
    }
  }
}

bool get_have_function_call(Instrs *instrs) {
  for (u32 i = 0; i < instrs->len; ++i)
    if (instrs->items[i].kind == InstrKindCall ||
        instrs->items[i].kind == InstrKindCallAssign)
      return true;

  return false;
}

bool clutter_return_reg(Instrs *instrs, UsesReturnRegFunc uses_return_reg) {
  for (u32 i = 0; i < instrs->len; ++i)
    if (uses_return_reg(instrs->items + i))
      return true;

  return false;
}
