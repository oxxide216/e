#ifndef OPT_H
#define OPT_H

#include "codegen.h"

void opt_merge_derefs(Instrs *instrs);
void opt_derefs_to_copies(Instrs *instrs, VarLocs *locs);
void opt_copy_prop(Instrs *instrs, VarLocs *locs);
void opt_const_prop(Instrs *instrs, VarLocs *locs);
void opt_dce(Instrs *instrs, VarLocs *locs);
// TODO: fixed version of opt_ret_val_prop
// TODO: fixed version of opt_const_fold

void opt_before_var_locs(Proc *proc);
void opt_after_var_locs(Proc *proc, VarLocs *locs);

#endif // OPT_H
