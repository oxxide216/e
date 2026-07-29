#ifndef OPT_H
#define OPT_H

#include "codegen.h"

void opt_merge_derefs(Proc *proc, u8 *labels, Arena *arena);
void opt_derefs_to_copies(Proc *proc);
void opt_copy_prop(Proc *proc, VarLocs *locs, u8 *labels);
// TODO: fixed version of opt_ret_val_prop
// TODO: fixed version of opt_const_fold

#endif // OPT_H
