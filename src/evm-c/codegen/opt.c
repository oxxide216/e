#include "opt.h"

#define DA_ARENA_INSERT(da, index, element, arena)                      \
  do {                                                                  \
    if ((da).cap <= (da).len) {                                         \
      if ((da).cap != 0)                                                \
        while ((da).cap <= (da).len)                                    \
          (da).cap *= 2;                                                \
      else                                                              \
        (da).cap = 1;                                                   \
      void *new_items = arena_alloc(arena, (da).cap * sizeof(element)); \
      if ((da).items)                                                   \
        memcpy(new_items, (da).items, (da).len * sizeof(element));      \
      (da).items = new_items;                                           \
    }                                                                   \
    memmove((da).items + (index) + 1,                                   \
            (da).items + (index),                                       \
            ((da).len - (index)) * sizeof(element));                    \
    (da).items[index] = element;                                        \
    ++(da).len;                                                         \
  } while (0)

typedef struct {
  u32      dest_index;
  u32      src_index;
  Segments segments;
  u32      instr_index;
} DestIndexWithSegments;

typedef Da(DestIndexWithSegments) DestIndicesWithSegments;

typedef struct {
  u32 dest_index;
  u32 src_index;
} Replacement;

typedef Da(Replacement) Replacements;

Segments *find_segments(DestIndicesWithSegments *indices, u32 index,
                        u32 *src_index, u32 *instr_index) {
  for (u32 i = 0; i < indices->len; ++i) {
    if (index == indices->items[i].dest_index) {
      *src_index = indices->items[i].src_index;
      *instr_index = indices->items[i].instr_index;
      return &indices->items[i].segments;
    }
  }

  return NULL;
}

void find_replacement(Replacements *replacements, VarLocs *locs, u32 *src_index) {
  for (u32 i = replacements->len; i > 0; --i) {
    if (*src_index == replacements->items[i - 1].dest_index) {
      --locs->items[*src_index].uses;
      *src_index = replacements->items[i - 1].src_index;
      ++locs->items[*src_index].uses;
      break;
    }
  }
}

void opt_merge_derefs(Proc *proc, Arena *arena) {
  DestIndicesWithSegments indices = {0};

  for (u32 i = 0; i < proc->instrs.len; ++i) {
    Instr *instr = proc->instrs.items + i;
    u32 j = i + 1;
    Instr *next_instr = proc->instrs.items + j;
    bool removed = false;

    if (instr->kind == InstrKindCopyFromRefFixed &&
        instr->as.copy_from_ref_fixed.take_ref) {
      DestIndexWithSegments new_index_with_segments = {
        instr->as.copy_from_ref_fixed.dest_index,
        instr->as.copy_from_ref_fixed.src_index,
        instr->as.copy_from_ref_fixed.src_segments,
        i,
      };
      DA_APPEND(indices, new_index_with_segments);
    }

    while (j < proc->instrs.len) {
      if (instr->kind != InstrKindCopyFromRefFixed ||
          !instr->as.copy_from_ref_fixed.take_ref)
        break;

      if (next_instr->kind == InstrKindCopyFromRefFixed) {
        u32 src_index, instr_index;
        Segments *segments = find_segments(&indices, next_instr->as.copy_from_ref_fixed.src_index, &src_index, &instr_index);
        if (segments) {
          Segments new_segments;
          new_segments.len = segments->len + next_instr->as.copy_from_ref_fixed.src_segments.len;
          new_segments.cap = new_segments.len;
          new_segments.items = arena_alloc(arena, new_segments.cap * sizeof(AlignedSegment));
          memcpy(new_segments.items, segments->items, segments->len * sizeof(AlignedSegment));
          memcpy(new_segments.items + segments->len,
                 next_instr->as.copy_from_ref_fixed.src_segments.items,
                 next_instr->as.copy_from_ref_fixed.src_segments.len * sizeof(AlignedSegment));
          next_instr->as.copy_from_ref_fixed.src_index = src_index;
          next_instr->as.copy_from_ref_fixed.src_segments = new_segments;
          next_instr->as.copy_from_ref_fixed.deref = instr->as.copy_from_ref_fixed.deref;
          DA_REMOVE_AT(proc->instrs, instr_index);
          if (instr_index <= i)
            removed = true;
          DestIndexWithSegments new_index_with_segments = {
            next_instr->as.copy_from_ref_fixed.dest_index,
            next_instr->as.copy_from_ref_fixed.src_index,
            new_segments,
            j,
          };
          DA_APPEND(indices, new_index_with_segments);
        }
      } else if (next_instr->kind == InstrKindCopyToRefFixed) {
        u32 src_index, instr_index;
        Segments *segments = find_segments(&indices, next_instr->as.copy_to_ref_fixed.dest_index, &src_index, &instr_index);
        if (segments) {
          Segments new_segments;
          new_segments.len = segments->len + next_instr->as.copy_to_ref_fixed.dest_segments.len;
          new_segments.cap = new_segments.len;
          new_segments.items = arena_alloc(arena, new_segments.cap * sizeof(AlignedSegment));
          memcpy(new_segments.items, segments->items, segments->len * sizeof(AlignedSegment));
          memcpy(new_segments.items + segments->len,
                 next_instr->as.copy_to_ref_fixed.dest_segments.items,
                 next_instr->as.copy_to_ref_fixed.dest_segments.len * sizeof(AlignedSegment));
          next_instr->as.copy_to_ref_fixed.dest_segments = new_segments;
          next_instr->as.copy_to_ref_fixed.dest_index = src_index;
          next_instr->as.copy_to_ref_fixed.deref = instr->as.copy_from_ref_fixed.deref;
          DA_REMOVE_AT(proc->instrs, instr_index);
          if (instr_index <= i)
            removed = true;
        }
      } else if (next_instr->kind == InstrKindAlloc ||
                 next_instr->kind == InstrKindStore) {
        ++next_instr;
        ++j;
        continue;
      } else {
        break;
      }
    }

    j = i + 1;
    next_instr = proc->instrs.items + j;

    while (j < proc->instrs.len) {
      if (instr->kind != InstrKindCopyToRefFixed)
        break;

      if (next_instr->kind == InstrKindCopyToRefFixed &&
          next_instr->as.copy_to_ref_fixed.src_index ==
          instr->as.copy_to_ref_fixed.dest_index) {
        Segments new_segments;
        new_segments.len = instr->as.copy_to_ref_fixed.dest_segments.len +
                           next_instr->as.copy_to_ref_fixed.dest_segments.len;
        new_segments.cap = new_segments.len;
        new_segments.items = arena_alloc(arena, new_segments.cap * sizeof(AlignedSegment));
        memcpy(new_segments.items, instr->as.copy_to_ref_fixed.dest_segments.items,
               instr->as.copy_to_ref_fixed.dest_segments.len * sizeof(AlignedSegment));
        memcpy(new_segments.items + instr->as.copy_to_ref_fixed.dest_segments.len,
               next_instr->as.copy_to_ref_fixed.dest_segments.items,
               next_instr->as.copy_to_ref_fixed.dest_segments.len * sizeof(AlignedSegment));
        next_instr->as.copy_to_ref_fixed.dest_segments = new_segments;
        next_instr->as.copy_to_ref_fixed.src_index = instr->as.copy_to_ref_fixed.src_index;
        next_instr->as.copy_to_ref_fixed.deref = instr->as.copy_to_ref_fixed.deref;
        DA_REMOVE_AT(proc->instrs, i);
        removed = true;
      } else if (next_instr->kind == InstrKindAlloc ||
                 next_instr->kind == InstrKindStore) {
        ++next_instr;
        ++j;
        continue;
      } else {
        break;
      }
    }

    if (removed)
      --i;
    indices.len = 0;
  }

  if (indices.items)
    free(indices.items);
}

void opt_derefs_to_copies(Proc *proc) {
  for (u32 i = 0; i < proc->instrs.len; ++i) {
    Instr *instr = proc->instrs.items + i;

    if (instr->kind == InstrKindCopyToRefFixed) {
      if (!instr->as.copy_to_ref_fixed.deref) {
        i32 offset = instr->as.copy_to_ref_fixed.dest_segments.items[0].offset;
        for (u32 j = 1; j < instr->as.copy_to_ref_fixed.dest_segments.len; ++j)
          offset += instr->as.copy_to_ref_fixed.dest_segments.items[j].offset;

        if (offset == 0) {
          Instr new_instr = {
            InstrKindCopy,
            {
              .copy = {
                instr->as.copy_to_ref_fixed.dest_index,
                instr->as.copy_to_ref_fixed.src_index,
              },
            },
          };
          *instr = new_instr;
        }
      }
    } else if (instr->kind == InstrKindCopyFromRefFixed) {
      if (!instr->as.copy_from_ref_fixed.deref) {
        i32 offset = instr->as.copy_from_ref_fixed.src_segments.items[0].offset;
        for (u32 j = 1; j < instr->as.copy_from_ref_fixed.src_segments.len; ++j)
          offset += instr->as.copy_from_ref_fixed.src_segments.items[j].offset;

        if (offset == 0) {
          Instr new_instr = {
            InstrKindCopy,
            {
              .copy = {
                instr->as.copy_from_ref_fixed.dest_index,
                instr->as.copy_from_ref_fixed.src_index,
              },
            },
          };
          *instr = new_instr;
        }
      }
    }
  }
}

void opt_copy_prop(Proc *proc, VarLocs *locs) {
  Replacements replacements = {0};

  for (u32 i = 0; i < proc->instrs.len; ++i) {
    Instr *instr = proc->instrs.items + i;

    switch (instr->kind) {
    case InstrKindAlloc: break;
    case InstrKindStore: break;

    case InstrKindCopy: {
      find_replacement(&replacements, locs, &instr->as.copy.src_index);

      VarLoc *dest_loc = locs->items + instr->as.copy.dest_index;
      VarLoc *src_loc = locs->items + instr->as.copy.src_index;
      if (dest_loc->uses <= 2) {
        --dest_loc->uses;
        --src_loc->uses;
        if (dest_loc->uses == 2) {
          Replacement replacement = {
            instr->as.copy.dest_index,
            instr->as.copy.src_index,
          };
          DA_APPEND(replacements, replacement);
        }
        DA_REMOVE_AT(proc->instrs, i);
        --i;
      }
    } break;

    case InstrKindBinOp: {
      find_replacement(&replacements, locs, &instr->as.bin_op.src0_index);
      find_replacement(&replacements, locs, &instr->as.bin_op.src1_index);
    } break;

    case InstrKindCall: {
      for (u32 j = 0; j < instr->as.call.arg_indices.len; ++j)
        find_replacement(&replacements, locs, instr->as.call.arg_indices.items + j);
    } break;

    case InstrKindCallAssign: {
      for (u32 j = 0; j < instr->as.call_assign.arg_indices.len; ++j)
        find_replacement(&replacements, locs, instr->as.call_assign.arg_indices.items + j);
    } break;

    case InstrKindRet: break;

    case InstrKindRetVal: {
      find_replacement(&replacements, locs, &instr->as.ret_val.index);
    } break;

    case InstrKindJump: break;

    case InstrKindJumpIfNot: {
      find_replacement(&replacements, locs, &instr->as.jump_if_not.cond_index);
    } break;

    case InstrKindRef: {
      find_replacement(&replacements, locs, &instr->as.ref.src_index);
    } break;

    case InstrKindCopyToRef: {
      find_replacement(&replacements, locs, &instr->as.copy_to_ref.dest_index);
      find_replacement(&replacements, locs, &instr->as.copy_to_ref.dest_offset_index);
      find_replacement(&replacements, locs, &instr->as.copy_to_ref.src_index);
    } break;

    case InstrKindCopyFromRef: {
      find_replacement(&replacements, locs, &instr->as.copy_from_ref.dest_index);
      find_replacement(&replacements, locs, &instr->as.copy_from_ref.src_index);
      find_replacement(&replacements, locs, &instr->as.copy_from_ref.src_offset_index);
    } break;

    case InstrKindInlineAsm: {
      for (u32 j = 0; j < instr->as.inline_asm.segments.len; ++j)
        if (instr->as.inline_asm.segments.items[j].kind == AsmSegmentKindVar)
          find_replacement(&replacements, locs, &instr->as.inline_asm.segments.items[j].index);
    } break;

    case InstrKindStoreData: break;

    case InstrKindConvert: {
      find_replacement(&replacements, locs, &instr->as.convert.src_index);
    } break;

    case InstrKindCopyToRefFixed: {
      find_replacement(&replacements, locs, &instr->as.copy_to_ref_fixed.dest_index);
      find_replacement(&replacements, locs, &instr->as.copy_to_ref_fixed.src_index);
    } break;

    case InstrKindCopyFromRefFixed: {
      find_replacement(&replacements, locs, &instr->as.copy_from_ref_fixed.dest_index);
      find_replacement(&replacements, locs, &instr->as.copy_from_ref_fixed.src_index);
    } break;

    case InstrKindRefProc: break;

    case InstrKindCallRef: {
      find_replacement(&replacements, locs, &instr->as.call_ref.index);
      for (u32 j = 0; j < instr->as.call_ref.arg_indices.len; ++j)
        find_replacement(&replacements, locs, instr->as.call_ref.arg_indices.items + j);
    } break;

    case InstrKindCallRefAssign: {
      find_replacement(&replacements, locs, &instr->as.call_ref_assign.index);
      for (u32 j = 0; j < instr->as.call_ref_assign.arg_indices.len; ++j)
        find_replacement(&replacements, locs, instr->as.call_ref_assign.arg_indices.items + j);
    } break;
    }
  }

  if (replacements.items)
    free(replacements.items);
}

void opt_ret_val_prop(Proc *proc, VarLocs *locs, Arena *arena) {
  if (proc->return_size == 0 || proc->return_size > 8)
    return;

  u32 ret_val_instr_index = (u32) -1;
  for (u32 i = proc->instrs.len; i > 0; --i) {
    Instr *instr = proc->instrs.items + i - 1;
    u32 *dest_index = NULL;

    switch (instr->kind) {
    case InstrKindAlloc: break;

    case InstrKindStore: {
      dest_index = &instr->as.store.index;
    } break;

    case InstrKindCopy: {
      dest_index = &instr->as.copy.dest_index;
    } break;

    case InstrKindBinOp: {
      dest_index = &instr->as.bin_op.dest_index;
    } break;

    case InstrKindCall: break;

    case InstrKindCallAssign: {
      dest_index = &instr->as.call_assign.dest_index;
    } break;

    case InstrKindRet: break;

    case InstrKindRetVal: {
      ret_val_instr_index = i - 1;
    } break;

    case InstrKindJump: break;
    case InstrKindJumpIfNot: break;

    case InstrKindRef: {
      dest_index = &instr->as.ref.dest_index;
    } break;

    case InstrKindCopyToRef: break;

    case InstrKindCopyFromRef: {
      dest_index = &instr->as.copy_from_ref.dest_index;
    } break;

    case InstrKindInlineAsm: break;

    case InstrKindStoreData: {
      dest_index = &instr->as.store_data.index;
    } break;

    case InstrKindConvert: {
      dest_index = &instr->as.convert.dest_index;
    } break;

    case InstrKindCopyToRefFixed: break;

    case InstrKindCopyFromRefFixed: {
      dest_index = &instr->as.copy_from_ref_fixed.dest_index;
    } break;

    case InstrKindRefProc: {
      dest_index = &instr->as.ref_proc.dest_index;
    } break;

    case InstrKindCallRef: break;

    case InstrKindCallRefAssign: {
      dest_index = &instr->as.call_ref_assign.dest_index;
    } break;
    }

    if (ret_val_instr_index != (u32) -1 && dest_index) {
      VarLoc ret_var_loc = {
        0,
        proc->return_kind,
        proc->return_size,
        2,
        i - 1,
        ret_val_instr_index,
        false,
        false,
        false,
        0,
        true,
      };
      if (locs->items)
        locs->items = realloc(locs->items, (locs->cap + 1) * sizeof(VarLoc));
      else
        locs->items = malloc((locs->cap + 1) * sizeof(VarLoc));
      locs->items[locs->cap] = ret_var_loc;

      --locs->items[*dest_index].uses;
      --locs->items[proc->instrs.items[ret_val_instr_index].as.ret_val.index].uses;

      *dest_index = locs->cap;
      proc->instrs.items[ret_val_instr_index].as.ret_val.index = locs->cap;

      Segments segments;
      segments.len = 1;
      segments.cap = segments.len;
      segments.items = arena_alloc(arena, segments.cap * sizeof(AlignedSegment));
      segments.items[0].offset = 0;
      segments.items[0].size = proc->return_size;

      Instr new_instr = {
        InstrKindAlloc,
        {
          .alloc = {
            locs->cap,
            segments,
          },
        },
      };
      DA_ARENA_INSERT(proc->instrs, i - 1, new_instr, arena);

      ++locs->cap;

      ret_val_instr_index = (u32) -1;
    } else if (instr->kind == InstrKindCall || instr->kind == InstrKindCallAssign ||
               instr->kind == InstrKindCallRef || instr->kind == InstrKindCallRefAssign) {
      ret_val_instr_index = (u32) -1;
    }
  }
}

void opt_const_fold(Proc *proc, VarLocs *locs, u8 *labels) {
  for (u32 i = 0; i < proc->instrs.len; ++i) {
    Instr *instr = proc->instrs.items + i;

    if (labels[i])
      for (u32 j = 0; j < locs->cap; ++j)
        locs->items[j].has_imm_value = false;

    switch (instr->kind) {
    case InstrKindAlloc: break;

    case InstrKindStore: {
      locs->items[instr->as.store.index].has_imm_value = true;
      --locs->items[instr->as.store.index].uses;
    } break;

    case InstrKindCopy: {
      locs->items[instr->as.copy.dest_index].has_imm_value = locs->items[instr->as.copy.src_index].has_imm_value;
      if (locs->items[instr->as.copy.src_index].has_imm_value) {
        --locs->items[instr->as.copy.src_index].uses;
        --locs->items[instr->as.copy.dest_index].uses;
      }
    } break;

    case InstrKindBinOp: {
      locs->items[instr->as.bin_op.dest_index].has_imm_value = false;
      if (locs->items[instr->as.bin_op.src0_index].has_imm_value)
        --locs->items[instr->as.bin_op.src0_index].uses;
      if (locs->items[instr->as.bin_op.src1_index].has_imm_value)
        --locs->items[instr->as.bin_op.src1_index].uses;
    } break;

    case InstrKindCall: {
      for (u32 j = 0; j < instr->as.call.arg_indices.len; ++j)
        if (locs->items[instr->as.call.arg_indices.items[j]].has_imm_value)
          --locs->items[instr->as.call.arg_indices.items[j]].uses;
    } break;

    case InstrKindCallAssign: {
      locs->items[instr->as.call_assign.dest_index].has_imm_value = false;
      for (u32 j = 0; j < instr->as.call_assign.arg_indices.len; ++j)
        if (locs->items[instr->as.call_assign.arg_indices.items[j]].has_imm_value)
          --locs->items[instr->as.call_assign.arg_indices.items[j]].uses;
    } break;

    case InstrKindRet: break;

    case InstrKindRetVal: {
      if (locs->items[instr->as.ret_val.index].has_imm_value)
        --locs->items[instr->as.ret_val.index].uses;
    } break;

    case InstrKindJump: break;

    case InstrKindJumpIfNot: {
      if (locs->items[instr->as.jump_if_not.cond_index].has_imm_value)
        --locs->items[instr->as.jump_if_not.cond_index].uses;
    } break;

    case InstrKindRef: {
      locs->items[instr->as.ref.dest_index].has_imm_value = false;
      if (locs->items[instr->as.ref.src_index].has_imm_value)
        --locs->items[instr->as.ref.src_index].uses;
    } break;

    case InstrKindCopyToRef: {
      if (locs->items[instr->as.copy_to_ref.dest_index].has_imm_value)
        --locs->items[instr->as.copy_to_ref.dest_index].uses;
      if (locs->items[instr->as.copy_to_ref.dest_offset_index].has_imm_value)
        --locs->items[instr->as.copy_to_ref.dest_offset_index].uses;
      if (locs->items[instr->as.copy_to_ref.src_index].has_imm_value)
        --locs->items[instr->as.copy_to_ref.src_index].uses;
    } break;

    case InstrKindCopyFromRef: {
      locs->items[instr->as.copy_from_ref.dest_index].has_imm_value = false;
      if (locs->items[instr->as.copy_from_ref.src_index].has_imm_value)
        --locs->items[instr->as.copy_from_ref.src_index].uses;
      if (locs->items[instr->as.copy_from_ref.src_offset_index].has_imm_value)
        --locs->items[instr->as.copy_from_ref.src_offset_index].uses;
    } break;

    case InstrKindInlineAsm: {
      for (u32 j = 0; j < instr->as.inline_asm.segments.len; ++j)
        if (instr->as.inline_asm.segments.items[j].kind == AsmSegmentKindVar)
          if (locs->items[instr->as.inline_asm.segments.items[j].index].has_imm_value)
            --locs->items[instr->as.inline_asm.segments.items[j].index].uses;
    } break;

    case InstrKindStoreData: {
      locs->items[instr->as.store_data.index].has_imm_value = false;
    } break;

    case InstrKindConvert: {
      locs->items[instr->as.convert.dest_index].has_imm_value = false;
      if (locs->items[instr->as.convert.src_index].has_imm_value)
        --locs->items[instr->as.convert.src_index].uses;
    } break;

    case InstrKindCopyToRefFixed: {
      if (locs->items[instr->as.copy_to_ref_fixed.dest_index].has_imm_value)
        --locs->items[instr->as.copy_to_ref_fixed.dest_index].uses;
      if (locs->items[instr->as.copy_to_ref_fixed.src_index].has_imm_value)
        --locs->items[instr->as.copy_to_ref_fixed.src_index].uses;
    } break;

    case InstrKindCopyFromRefFixed: {
      locs->items[instr->as.copy_from_ref_fixed.dest_index].has_imm_value = false;
      if (locs->items[instr->as.copy_from_ref_fixed.src_index].has_imm_value)
        --locs->items[instr->as.copy_from_ref_fixed.src_index].uses;
    } break;

    case InstrKindRefProc: {
      locs->items[instr->as.ref_proc.dest_index].has_imm_value = false;
    } break;

    case InstrKindCallRef: {
      for (u32 j = 0; j < instr->as.call_ref.arg_indices.len; ++j)
        if (locs->items[instr->as.call_ref.arg_indices.items[j]].has_imm_value)
          --locs->items[instr->as.call_ref.arg_indices.items[j]].uses;
    } break;

    case InstrKindCallRefAssign: {
      locs->items[instr->as.call_ref_assign.dest_index].has_imm_value = false;
      for (u32 j = 0; j < instr->as.call_ref_assign.arg_indices.len; ++j)
        if (locs->items[instr->as.call_ref_assign.arg_indices.items[j]].has_imm_value)
          --locs->items[instr->as.call_ref_assign.arg_indices.items[j]].uses;
    } break;
    }
  }

  for (u32 i = 0; i < locs->cap; ++i)
    locs->items[i].has_imm_value = false;
}
