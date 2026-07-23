#ifndef CODEGEN_H
#define CODEGEN_H

#include "ir.h"

typedef struct {
  // value >= means register, < 0 means stack
  i32       value;
  ValueKind kind;
  u32       size;
  u32       uses;
  // Lifetime af a variable
  u32       begin;
  u32       end;
  bool      is_stack_only;
  bool      is_arg;
} VarLoc;

typedef struct {
  VarLoc *items;
  u32     cap;
} VarLocs;

typedef Da(VarLoc *) VarLocRefs;

typedef struct {
  u32 regs;
  u32 stack_size;
} SpaceUsed;

typedef struct {
  SpaceUsed  space_used;
  VarLocRefs refs;
} Allocator;

typedef u32 (*AlignmentFunc)(u32 size);

void      add_var_locs(VarLocs *locs, Proc *proc);
SpaceUsed var_locs_set_values(VarLocs *locs, u32 scratch_regs_len);
void      promote_lifetimes_of_pre_loop_vars_to_ends_of_loops(Proc *proc, VarLocs *locs);
void      align_fixed_offsets(Proc *proc, AlignmentFunc alignment_func);
bool      get_has_function_call(Proc *proc);

void write_ir_as_asm_yasm_x86_64(FILE *stream, Ir *ir);

#endif // CODEGEN_H
