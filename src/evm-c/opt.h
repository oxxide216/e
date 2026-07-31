#ifndef OPT_H
#define OPT_H

#include "codegen/codegen.h"

void opt_merge_derefs(Proc *proc, Arena *arena);
void opt_derefs_to_copies(Proc *proc);
// TODO: fixed version of opt_copy_prop
// TODO: fixed version of opt_ret_val_prop
// TODO: fixed version of opt_const_fold

#endif // OPT_H
