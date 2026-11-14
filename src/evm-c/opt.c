#include "opt.h"
#include "codegen/utils.h"

typedef struct {
  u32 dest;
  u32 src;
  u32 instr_index;
} CopyProp;

typedef Da(CopyProp) CopyProps;

void opt_derefs_to_copies(Instrs *instrs, VarLocs *locs) {
  for (u32 i = 0; i < instrs->len; ++i) {
    Instr *instr = instrs->items + i;

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
            instr->has_label,
          };
          *instr = new_instr;
        }
      }
    } else if (instr->kind == InstrKindCopyFromRefFixed) {
      if (!instr->as.copy_from_ref_fixed.deref &&
          !instr->as.copy_from_ref_fixed.take_ref) {
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
            instr->has_label,
          };
          *instr = new_instr;
        }
      }
    }
  }
}

static u32 get_prop_index_by_dest(CopyProps *props, u32 dest) {
  for (u32 i = 0; i < props->len; ++i)
    if (props->items[i].dest == dest)
      return i;

  return (u32) -1;
}

static u32 get_prop_index_by_src(CopyProps *props, u32 src) {
  for (u32 i = 0; i < props->len; ++i)
    if (props->items[i].src == src)
      return i;

  return (u32) -1;
}

static void remove_instr_props(Instrs *instrs, VarLocs *locs,
                               CopyProps *props, u32 index) {
  instrs->items[index + 1].has_label |= instrs->items[index].has_label;

  DA_REMOVE_AT(*instrs, index);

  for (u32 i = 0; i < instrs->len; ++i) {
    Instr *instr = instrs->items + i;

    if (instr->kind == InstrKindJump &&
        instr->as.jump.target > index)
      --instr->as.jump.target;
    else if (instr->kind == InstrKindJumpIfNot &&
             instr->as.jump_if_not.target > index)
      --instr->as.jump_if_not.target;
  }

  for (u32 i = 0; i < locs->cap; ++i) {
    if (locs->items[i].begin > index)
      --locs->items[i].begin;
    if (locs->items[i].end > index)
      --locs->items[i].end;
  }

  for (u32 i = 0; i < props->len; ++i) {
    if (props->items[i].instr_index == index) {
      DA_REMOVE_AT(*props, i);
      --i;
    } else if (props->items[i].instr_index > index) {
      --props->items[i].instr_index;
    }
  }
}

void opt_copy_prop(Instrs *instrs, VarLocs *locs) {
  CopyProps props = {0};

  for (u32 i = 0; i < instrs->len; ++i) {
    Instr *instr = instrs->items + i;

    if (instr->has_label ||
        instr->kind == InstrKindCopyToRef ||
        instr->kind == InstrKindCopyToRefFixed) {
      props.len = 0;
    } else {
      u32 *dest = get_dest(instr);
      if (dest) {
        u32 index;
        while ((index = get_prop_index_by_dest(&props, *dest)) != (u32) -1)
          DA_REMOVE_AT(props, index);
        while ((index = get_prop_index_by_src(&props, *dest)) != (u32) -1)
          DA_REMOVE_AT(props, index);
      }
    }

    u32 j = 0;
    u32 *arg;
    while ((arg = get_nth_arg(instr, j++))) {
      u32 index = get_prop_index_by_dest(&props, *arg);
      if (index != (u32) -1) {
        CopyProp *prop = props.items + index;
        *arg = prop->src;
        ++locs->items[prop->src].uses;
        if (locs->items[prop->src].end < i)
          locs->items[prop->src].end = i;
        if (locs->items[prop->dest].uses > 0)
          --locs->items[prop->dest].uses;
        if (locs->items[prop->dest].uses <= 1) {
          remove_instr_props(instrs, locs, &props, prop->instr_index);
          --instr;
          --i;
        }
      }
    }

    if (instr->kind == InstrKindCopy) {
      CopyProp prop = {
        instr->as.copy.dest_index,
        instr->as.copy.src_index,
        i,
      };
      DA_APPEND(props, prop);
    }
  }

  if (props.items)
    free(props.items);
}

void opt_const_prop(Instrs *instrs, VarLocs *locs) {
  CopyProps props = {0};

  for (u32 i = 0; i < instrs->len; ++i) {
    Instr *instr = instrs->items + i;

    if (instr->has_label ||
        instr->kind == InstrKindCopyToRef ||
        instr->kind == InstrKindCopyToRefFixed) {
      props.len = 0;
    } else {
      u32 *dest = get_dest(instr);
      if (dest) {
        u32 index;
        while ((index = get_prop_index_by_dest(&props, *dest)) != (u32) -1)
          DA_REMOVE_AT(props, index);
      }
    }

    u32 j = 0;
    u32 *arg;
    while ((arg = get_nth_arg(instr, j++))) {
      u32 index = get_prop_index_by_dest(&props, *arg);
      if (index != (u32) -1) {
        CopyProp *prop = props.items + index;
        if (locs->items[prop->dest].uses > 0)
          --locs->items[prop->dest].uses;
      }
    }

    if (instr->kind == InstrKindStore) {
      CopyProp prop = { instr->as.copy.dest_index, 0, 0 };
      DA_APPEND(props, prop);
    } else if (instr->kind == InstrKindSizeOf) {
      CopyProp prop = { instr->as.size_of.dest_index, 0, 0 };
      DA_APPEND(props, prop);
    }
  }

  if (props.items)
    free(props.items);
}
