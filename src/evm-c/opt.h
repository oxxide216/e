#ifndef OPT_H
#define OPT_H

#include "codegen/codegen.h"

// TODO: fixed version of opt_merge_derefs
void opt_derefs_to_copies(Instrs *instrs, VarLocs *locs);
// TODO: fixed version of opt_copy_prop
// TODO: fixed version of opt_ret_val_prop
// TODO: fixed version of opt_const_fold

#endif // OPT_H
