#ifndef OPT_H
#define OPT_H

#include "codegen.h"

void opt_merge_derefs(Proc *proc, Arena *arena);
void opt_copy_prop(Proc *proc, VarLocs *locs);
void opt_ret_val_prop(Proc *proc, VarLocs *locs);

#endif // OPT_H
