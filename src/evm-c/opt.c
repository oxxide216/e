#include "opt.h"
#include "codegen/utils.h"

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

void opt_copy_prop(Proc *proc, VarLocs *locs, u8 *labels) {
  Replacements replacements = {0};

  for (u32 i = 0; i < proc->instrs.len; ++i) {
    Instr *instr = proc->instrs.items + i;

    if (labels[i] || is_jump(instr))
      replacements.len = 0;

    u32 *arg;
    u32 j = 0;
    while ((arg = get_nth_arg(instr, j++)))
      find_replacement(&replacements, locs, arg);

    u32 *dest = get_dest(instr);
    if (dest) {
      for (u32 j = 0; j < replacements.len; ++j) {
        if (replacements.items[j].dest_index == *dest) {
          DA_REMOVE_AT(replacements, j);
          --j;
        }
      }
    }

    if (instr->kind == InstrKindCopy) {
      VarLoc *dest_loc = locs->items + instr->as.copy.dest_index;
      VarLoc *src_loc = locs->items + instr->as.copy.src_index;
      if (dest_loc->uses <= 2) {
        --dest_loc->uses;
        --src_loc->uses;
        Replacement replacement = {
          instr->as.copy.dest_index,
          instr->as.copy.src_index,
        };
        DA_APPEND(replacements, replacement);
        DA_REMOVE_AT(proc->instrs, i);
        --i;
      }
    }
  }

  if (replacements.items)
    free(replacements.items);
}
