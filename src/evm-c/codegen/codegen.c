#include "codegen.h"
#include "utils.h"

void add_var_locs(VarLocs *locs, Proc *proc) {
  for (u32 i = 0; i < proc->instrs.len; ++i) {
    Instr *instr = proc->instrs.items + i;

    switch (instr->kind) {
    case InstrKindAlloc: {
      if (locs->cap < instr->as.alloc.index + 1) {
        u32 new_cap = locs->cap;
        if (new_cap == 0)
          new_cap = instr->as.alloc.index + 1;
        else
          while (new_cap < instr->as.alloc.index + 1)
            new_cap *= 2;
        if (locs->items)
          locs->items = realloc(locs->items, new_cap * sizeof(VarLoc));
        else
          locs->items = malloc(new_cap * sizeof(VarLoc));
        memset(locs->items + locs->cap, 0, (new_cap - locs->cap) * sizeof(VarLoc));
        locs->cap = new_cap;
      }

      AlignedSegment *last_segment = instr->as.alloc.segments.items + instr->as.alloc.segments.len - 1;
      u32 size = last_segment->offset + last_segment->size;
      bool is_stack_only = size > 8 || (size != 1 && size != 2 && size != 4 && size != 8);
      VarLoc loc = {
        ValueKindSigned, 0, size,
        0, i, 0, is_stack_only, false,
      };
      locs->items[instr->as.alloc.index] = loc;
    } break;

    case InstrKindStore: {
      if (instr->as.store.index < locs->cap) {
        ++locs->items[instr->as.store.index].uses;
        locs->items[instr->as.store.index].end = i;
      }
    } break;

    case InstrKindCopy: {
      if (instr->as.copy.dest_index < locs->cap) {
        ++locs->items[instr->as.copy.dest_index].uses;
        locs->items[instr->as.copy.dest_index].end = i;
      }
      if (instr->as.copy.src_index < locs->cap) {
        ++locs->items[instr->as.copy.src_index].uses;
        locs->items[instr->as.copy.src_index].end = i;
      }
    } break;

    case InstrKindBinOp: {
      if (instr->as.bin_op.dest_index < locs->cap) {
        ++locs->items[instr->as.bin_op.dest_index].uses;
        locs->items[instr->as.bin_op.dest_index].end = i;
      }
      if (instr->as.bin_op.src0_index < locs->cap) {
        ++locs->items[instr->as.bin_op.src0_index].uses;
        locs->items[instr->as.bin_op.src0_index].end = i;
      }
      if (instr->as.bin_op.src1_index < locs->cap) {
        ++locs->items[instr->as.bin_op.src1_index].uses;
        locs->items[instr->as.bin_op.src1_index].end = i;
      }
    } break;

    case InstrKindCall: {
      for (u32 j = 0; j < instr->as.call.arg_indices.len; ++j) {
        u32 arg_index = instr->as.call.arg_indices.items[j];
        if (arg_index < locs->cap) {
          ++locs->items[arg_index].uses;
          locs->items[arg_index].end = i;
        }
      }
    } break;

    case InstrKindCallAssign: {
      if (instr->as.call_assign.dest_index < locs->cap) {
        ++locs->items[instr->as.call_assign.dest_index].uses;
        locs->items[instr->as.call_assign.dest_index].end = i;
      }

      for (u32 j = 0; j < instr->as.call_assign.arg_indices.len; ++j) {
        u32 arg_index = instr->as.call_assign.arg_indices.items[j];
        if (arg_index < locs->cap) {
          ++locs->items[arg_index].uses;
          locs->items[arg_index].end = i;
        }
      }
    } break;

    case InstrKindRet: break;

    case InstrKindRetVal: {
      if (instr->as.ret_val.index < locs->cap) {
        ++locs->items[instr->as.ret_val.index].uses;
          locs->items[instr->as.ret_val.index].end = i;
      }
    } break;

    case InstrKindJump: break;

    case InstrKindJumpIfNot: {
      if (instr->as.jump_if_not.cond_index < locs->cap) {
        ++locs->items[instr->as.jump_if_not.cond_index].uses;
          locs->items[instr->as.jump_if_not.cond_index].end = i;
      }
    } break;

    case InstrKindRef: {
      if (instr->as.ref.dest_index < locs->cap) {
        ++locs->items[instr->as.ref.dest_index].uses;
        locs->items[instr->as.ref.dest_index].end = i;
      }
      if (instr->as.ref.src_index < locs->cap) {
        ++locs->items[instr->as.ref.src_index].uses;
        locs->items[instr->as.ref.src_index].end = i;
        locs->items[instr->as.ref.src_index].is_stack_only = true;
      }
    } break;

    case InstrKindCopyToRef: {
      if (instr->as.copy_to_ref.dest_index < locs->cap) {
        ++locs->items[instr->as.copy_to_ref.dest_index].uses;
        locs->items[instr->as.copy_to_ref.dest_index].end = i;
      }
      if (instr->as.copy_to_ref.dest_offset_index < locs->cap) {
        ++locs->items[instr->as.copy_to_ref.dest_offset_index].uses;
        locs->items[instr->as.copy_to_ref.dest_offset_index].end = i;
      }
      if (instr->as.copy_to_ref.src_index < locs->cap) {
        ++locs->items[instr->as.copy_to_ref.src_index].uses;
        locs->items[instr->as.copy_to_ref.src_index].end = i;
      }
    } break;

    case InstrKindCopyFromRef: {
      if (instr->as.copy_from_ref.dest_index < locs->cap) {
        ++locs->items[instr->as.copy_from_ref.dest_index].uses;
        locs->items[instr->as.copy_from_ref.dest_index].end = i;
      }
      if (instr->as.copy_from_ref.src_index < locs->cap) {
        ++locs->items[instr->as.copy_from_ref.src_index].uses;
        locs->items[instr->as.copy_from_ref.src_index].end = i;
      }
      if (instr->as.copy_from_ref.src_offset_index < locs->cap) {
        ++locs->items[instr->as.copy_from_ref.src_offset_index].uses;
        locs->items[instr->as.copy_from_ref.src_offset_index].end = i;
      }
    } break;

    case InstrKindInlineAsm: {
      for (u32 k = 0; k < instr->as.inline_asm.segments.len; ++k) {
        AsmSegment *segment = instr->as.inline_asm.segments.items + k;

        if (segment->kind == AsmSegmentKindVar) {
          if (segment->index < locs->cap) {
            ++locs->items[segment->index].uses;
            locs->items[segment->index].end = i;
          }
        }
      }
    } break;

    case InstrKindStoreData: {
      if (instr->as.store_data.index < locs->cap) {
        ++locs->items[instr->as.store_data.index].uses;
        locs->items[instr->as.store_data.index].end = i;
      }
    } break;

    case InstrKindConvert: {
      if (instr->as.convert.dest_index < locs->cap) {
        ++locs->items[instr->as.convert.dest_index].uses;
        locs->items[instr->as.convert.dest_index].end = i;
      }
      if (instr->as.convert.src_index < locs->cap) {
        ++locs->items[instr->as.convert.src_index].uses;
        locs->items[instr->as.convert.src_index].end = i;
      }
    } break;

    case InstrKindCopyToRefFixed: {
      if (instr->as.copy_to_ref_fixed.dest_index < locs->cap) {
        ++locs->items[instr->as.copy_to_ref_fixed.dest_index].uses;
        locs->items[instr->as.copy_to_ref_fixed.dest_index].end = i;
      }
      if (instr->as.copy_to_ref_fixed.src_index < locs->cap) {
        ++locs->items[instr->as.copy_to_ref_fixed.src_index].uses;
        locs->items[instr->as.copy_to_ref_fixed.src_index].end = i;
      }
    } break;

    case InstrKindCopyFromRefFixed: {
      if (instr->as.copy_from_ref_fixed.dest_index < locs->cap) {
        ++locs->items[instr->as.copy_from_ref_fixed.dest_index].uses;
        locs->items[instr->as.copy_from_ref_fixed.dest_index].end = i;
      }
      if (instr->as.copy_from_ref_fixed.src_index < locs->cap) {
        ++locs->items[instr->as.copy_from_ref_fixed.src_index].uses;
        locs->items[instr->as.copy_from_ref_fixed.src_index].end = i;
      }
    } break;

    case InstrKindRefProc: {
      if (instr->as.ref_proc.dest_index < locs->cap) {
        ++locs->items[instr->as.ref_proc.dest_index].uses;
        locs->items[instr->as.ref_proc.dest_index].end = i;
      }
    } break;

    case InstrKindCallRef: {
      if (instr->as.call_ref.index < locs->cap) {
        ++locs->items[instr->as.call_ref.index].uses;
        locs->items[instr->as.call_ref.index].end = i;
      }

      for (u32 j = 0; j < instr->as.call_ref.arg_indices.len; ++j) {
        u32 arg_index = instr->as.call_ref.arg_indices.items[j];
        if (arg_index < locs->cap) {
          ++locs->items[arg_index].uses;
          locs->items[arg_index].end = i;
        }
      }
    } break;

    case InstrKindCallRefAssign: {
      if (instr->as.call_ref_assign.dest_index < locs->cap) {
        ++locs->items[instr->as.call_ref_assign.dest_index].uses;
        locs->items[instr->as.call_ref_assign.dest_index].end = i;
      }

      if (instr->as.call_ref_assign.index < locs->cap) {
        ++locs->items[instr->as.call_ref_assign.index].uses;
        locs->items[instr->as.call_ref_assign.index].end = i;
      }

      for (u32 j = 0; j < instr->as.call_ref_assign.arg_indices.len; ++j) {
        u32 arg_index = instr->as.call_ref_assign.arg_indices.items[j];
        if (arg_index < locs->cap) {
          ++locs->items[arg_index].uses;
          locs->items[arg_index].end = i;
        }
      }
    } break;
    }
  }
}

static bool var_loc_collides_at_reg_index(VarLocRefs *refs, VarLoc *loc, i32 reg_index, u32 max) {
  for (u32 i = 0; i < max; ++i) {
    VarLoc *temp_loc = refs->items[i];
    if (temp_loc->value == reg_index &&
        temp_loc->end > loc->begin &&
        temp_loc->begin < loc->end)
      return true;
  }

  return false;
}

static i32 get_var_loc_reg_index(Allocator *allocator, VarLoc *loc,
                                 u32 max, u32 scratch_regs_len) {
  for (u32 i = 0; i < scratch_regs_len; ++i) {
    if (!var_loc_collides_at_reg_index(&allocator->refs, loc, i, max)) {
      if (i + 1 > allocator->space_used.regs)
        allocator->space_used.regs = i + 1;

      return i;
    }
  }

  return -1;
}

SpaceUsed var_locs_set_values(VarLocs *locs, u32 scratch_regs_len) {
  Allocator allocator = {0};
  for (u32 i = 0; i < locs->cap; ++i)
    if (locs->items[i].uses > 0 && !locs->items[i].is_arg)
      DA_APPEND(allocator.refs, locs->items + i);

  for (u32 i = 0; i + 1 < allocator.refs.len; ++i) {
    for (u32 j = i; j + 1 < allocator.refs.len; ++j) {
      VarLoc *a = allocator.refs.items[j];
      VarLoc *b = allocator.refs.items[j + 1];

      if (a->uses < b->uses) {
        VarLoc *temp = a;
        allocator.refs.items[j] = allocator.refs.items[j + 1];
        allocator.refs.items[j + 1] = temp;
      }
    }
  }

  for (u32 i = 0; i < allocator.refs.len; ++i) {
    VarLoc *loc = allocator.refs.items[i];

    if (loc->is_stack_only) {
      allocator.space_used.stack_size += loc->size;
      loc->value = -allocator.space_used.stack_size;
      continue;
    }

    i32 reg_index = get_var_loc_reg_index(&allocator, loc, i, scratch_regs_len);
    if (reg_index == -1) {
      allocator.space_used.stack_size += loc->size;
      loc->value = -allocator.space_used.stack_size;
    } else {
      loc->value = reg_index;
    }
  }

  if (allocator.refs.items)
    free(allocator.refs.items);

  return allocator.space_used;
}

// Fixes a bug when a variable that is defined before loop and used in it is overwritten byt an in-loop variable
void promote_lifetimes_of_pre_loop_vars_to_ends_of_loops(Proc *proc, VarLocs *locs) {
  for (u32 i = 0; i < proc->instrs.len; ++i) {
    Instr *instr = proc->instrs.items + i;

    if (instr->kind == InstrKindJump ||
        instr->kind == InstrKindJumpIfNot) {
      u32 target;
      if (instr->kind == InstrKindJump)
        target = instr->as.jump.target;
      else
        target = instr->as.jump_if_not.target;

      // Found a loop
      if (target < i) {
        for (u32 j = 0; j < locs->cap; ++j) {
          if (locs->items[j].begin > target)
            break;
          if (locs->items[j].end < i)
            locs->items[j].end = i;
        }
      }
    }
  }
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

    u32 offset = 0;
    for (u32 j = 0; j < offsets->len; ++j) {
      u32 alignment = alignment_func(offsets->items[j].size);
      offsets->items[j].offset = align(offsets->items[j].offset, alignment);
      offset += offsets->items[j].size;
    }
  }
}

bool get_has_function_call(Proc *proc) {
  for (u32 i = 0; i < proc->instrs.len; ++i)
    if (proc->instrs.items[i].kind == InstrKindCall ||
        proc->instrs.items[i].kind == InstrKindCallAssign)
      return true;

  return false;
}
