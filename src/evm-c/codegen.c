#include "codegen.h"
#include "utils.h"

static void add_var_locs_instrs(VarLocs *locs, Instrs *instrs, u32 offset) {
  for (u32 i = 0; i < instrs->len; ++i) {
    Instr *instr = instrs->items + i;

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

      u32 size = instr->as.alloc.size;
      bool is_stack_only = size > 8 || (size != 1 && size != 2 && size != 4 && size != 8);
      VarLoc loc = {
        0, ValueKindSigned, size,
        0, (u32) -1, (u32) -1, is_stack_only,
        false, false, 0, false,
      };
      locs->items[instr->as.alloc.index] = loc;
    } break;

    default: {
      u32 *dest = get_dest(instr);
      if (dest && *dest < locs->cap) {
        ++locs->items[*dest].uses;
        if (locs->items[*dest].end == (u32) -1 ||
            locs->items[*dest].end < i + offset)
          locs->items[*dest].end = i + offset;
        if (locs->items[*dest].begin == (u32) -1)
          locs->items[*dest].begin = i + offset;
      }

      u32 *arg;
      u32 j = 0;
      while ((arg = get_nth_arg(instr, j++))) {
        if (*arg < locs->cap) {
          ++locs->items[*arg].uses;
          if (locs->items[*arg].end == (u32) -1 ||
              locs->items[*arg].end < i + offset)
            locs->items[*arg].end = i + offset;
          if (locs->items[*arg].begin == (u32) -1)
            locs->items[*arg].begin = i + offset;
        }
      }

      if (instr->kind == InstrKindRef)
        locs->items[instr->as.ref.src_index].end = instrs->len - 1;
    } break;
    }
  }

  for (u32 i = 0; i < instrs->len; ++i) {
    Instr *instr = instrs->items + i;

    if (instr->kind == InstrKindRef)
      locs->items[instr->as.ref.src_index].is_stack_only = true;
    else if (instr->kind == InstrKindCopyToRef)
      locs->items[instr->as.copy_to_ref.dest_index].is_stack_only = true;
    else if (instr->kind == InstrKindCopyFromRef)
      locs->items[instr->as.copy_from_ref.src_index].is_stack_only = true;
    else if (instr->kind == InstrKindCopyToRefFixed &&
             !instr->as.copy_to_ref_fixed.deref)
      locs->items[instr->as.copy_to_ref_fixed.dest_index].is_stack_only = true;
    else if (instr->kind == InstrKindCopyFromRefFixed &&
             !instr->as.copy_from_ref_fixed.deref)
      locs->items[instr->as.copy_from_ref_fixed.src_index].is_stack_only = true;
  }
}

void add_var_locs(VarLocs *locs, Proc *proc) {
  add_var_locs_instrs(locs, &proc->instrs, 0);
  add_var_locs_instrs(locs, &proc->last_instrs, proc->instrs.len);
}

static bool var_loc_collides_at_value(VarLocRefs *refs, VarLoc *loc, i32 value, u32 max) {
  for (u32 i = 0; i < max; ++i) {
    VarLoc *temp_loc = refs->items[i];
    if (temp_loc->value == value &&
        temp_loc->end >= loc->begin &&
        temp_loc->begin <= loc->end)
      return true;
  }

  return false;
}

static i32 get_var_loc_reg_index(Allocator *allocator, VarLoc *loc,
                                 u32 max, u32 scratch_regs_len) {
  for (u32 i = 0; i < scratch_regs_len; ++i) {
    if (!var_loc_collides_at_value(&allocator->refs, loc, i, max)) {
      if (allocator->space_used.regs < i + 1)
        allocator->space_used.regs = i + 1;

      return i;
    }
  }

  return -1;
}

static i32 get_var_loc_stack_pos(Allocator *allocator, VarLoc *loc, u32 max) {
  for (u32 i = 0; i < allocator->used_stack_locs.len; ++i) {
    if (allocator->used_stack_locs.items[i].size < loc->size)
      continue;
    i32 pos = allocator->used_stack_locs.items[i].pos;
    if (!var_loc_collides_at_value(&allocator->refs, loc, pos, max))
      return pos;
  }

  allocator->space_used.stack_size =
    align(allocator->space_used.stack_size + loc->size,
          alignment_func(allocator->arch, loc->size));

  StackLoc new_loc = { -allocator->space_used.stack_size, loc->size };
  DA_APPEND(allocator->used_stack_locs, new_loc);

  return -allocator->space_used.stack_size;
}

static i32 partition_var_loc_refs(VarLocRefs *refs, i32 low, i32 high) {
  VarLoc *pivot = refs->items[high];
  i32 i = low;

  for (i32 j = low; j < high; ++j) {
    if (refs->items[j]->uses > pivot->uses) {
      VarLoc *temp = refs->items[i];
      refs->items[i] = refs->items[j];
      refs->items[j] = temp;
      ++i;
    }
  }

  return i;
}

static void quick_sort_var_loc_refs(VarLocRefs *refs, i32 low, i32 high) {
  if (low >= high || low < 0)
    return;

  i32 pivot = partition_var_loc_refs(refs, low, high);

  quick_sort_var_loc_refs(refs, low, pivot - 1);
  quick_sort_var_loc_refs(refs, pivot + 1, high);
}

SpaceUsed var_locs_set_values(VarLocs *locs, u32 scratch_regs_len, Arch arch) {
  Allocator allocator = {0};
  allocator.arch = arch;
  for (u32 i = 0; i < locs->cap; ++i)
    if (locs->items[i].uses > 0 && !locs->items[i].is_arg &&
        locs->items[i].begin != (u32) -1 && locs->items[i].end != (u32) -1)
      DA_APPEND(allocator.refs, locs->items + i);

  quick_sort_var_loc_refs(&allocator.refs, 0, allocator.refs.len - 1);

  for (u32 i = 0; i < allocator.refs.len; ++i) {
    VarLoc *loc = allocator.refs.items[i];

    if (loc->is_stack_only) {
      loc->value = get_var_loc_stack_pos(&allocator, loc, i);
      continue;
    }

    i32 reg_index = get_var_loc_reg_index(&allocator, loc, i, scratch_regs_len);
    if (reg_index == -1)
      loc->value = get_var_loc_stack_pos(&allocator, loc, i);
    else
      loc->value = reg_index;
  }

  if (allocator.refs.items)
    free(allocator.refs.items);
  if (allocator.used_stack_locs.items)
    free(allocator.used_stack_locs.items);

  return allocator.space_used;
}

void promote_lifetimes_of_pre_loop_vars_to_ends_of_loops_instrs(Instrs *instrs, u32 offset, VarLocs *locs) {
  for (u32 i = 0; i < instrs->len; ++i) {
    Instr *instr = instrs->items + i;

    if (instr->kind == InstrKindJump ||
        instr->kind == InstrKindJumpIfNot) {
      u32 target;
      if (instr->kind == InstrKindJump)
        target = instr->as.jump.target;
      else
        target = instr->as.jump_if_not.target;

      // Found a loop
      if (target < i)
        for (u32 j = 0; j < locs->cap; ++j)
          if (locs->items[j].begin <= target &&
              locs->items[j].end < i + offset)
            locs->items[j].end = i + offset;
    }
  }
}

// Fixes a bug when a variable that is defined before loop and used in it is overwritten by an in-loop variable
void promote_lifetimes_of_pre_loop_vars_to_ends_of_loops(Proc *proc, VarLocs *locs) {
  promote_lifetimes_of_pre_loop_vars_to_ends_of_loops_instrs(&proc->instrs, 0, locs);
  promote_lifetimes_of_pre_loop_vars_to_ends_of_loops_instrs(&proc->last_instrs, proc->instrs.len, locs);
}
