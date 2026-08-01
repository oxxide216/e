#include "opt.h"
#include "codegen/utils.h"

void opt_derefs_to_copies(Proc *proc, VarLocs *locs) {
  for (u32 i = 0; i < proc->instrs.len; ++i) {
    Instr *instr = proc->instrs.items + i;

    if (instr->kind == InstrKindCopyToRefFixed) {
      if (!instr->as.copy_to_ref_fixed.deref) {
        i32 offset = instr->as.copy_to_ref_fixed.dest_segments.items[0].offset;
        for (u32 j = 1; j < instr->as.copy_to_ref_fixed.dest_segments.len; ++j)
          offset += instr->as.copy_to_ref_fixed.dest_segments.items[j].offset;

        VarLoc *dest_loc = locs->items + instr->as.copy_to_ref_fixed.dest_index;
        VarLoc *src_loc = locs->items + instr->as.copy_to_ref_fixed.src_index;

        if (offset == 0 && dest_loc->size == src_loc->size) {
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

        VarLoc *dest_loc = locs->items + instr->as.copy_from_ref_fixed.dest_index;
        VarLoc *src_loc = locs->items + instr->as.copy_from_ref_fixed.src_index;

        if (offset == 0 && dest_loc->size == src_loc->size) {
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
