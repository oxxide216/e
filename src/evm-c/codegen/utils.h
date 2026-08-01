#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#include "codegen.h"
#include "shl/shl-defs.h"
#include "shl/shl-str.h"

#define DA_ARENA_INSERT(da, index, element, arena)                      \
  do {                                                                  \
    if ((da).cap <= (da).len) {                                         \
      if ((da).cap != 0)                                                \
        while ((da).cap <= (da).len)                                    \
          (da).cap *= 2;                                                \
      else                                                              \
        (da).cap = 1;                                                   \
      void *new_items = arena_alloc(arena, (da).cap * sizeof(element)); \
      if ((da).items)                                                   \
        memcpy(new_items, (da).items, (da).len * sizeof(element));      \
      (da).items = new_items;                                           \
    }                                                                   \
    memmove((da).items + (index) + 1,                                   \
            (da).items + (index),                                       \
            ((da).len - (index)) * sizeof(element));                    \
    (da).items[index] = element;                                        \
    ++(da).len;                                                         \
  } while (0)

typedef u32 (*AlignmentFunc)(u32 size);
typedef bool (*UsesReturnRegFunc)(Instr *instr);

void write_cstr(FILE *stream, char *cstr);
void write_str(FILE *stream, Str str);
void write_value(FILE *stream, Value *value);

void  get_proc_return_kind_and_size(Procs *procs, Str name, ValueKind *kind, u32 *size);
i32   align(i32 value, i32 base);
bool  is_jump(Instr *instr);
u32  *get_dest(Instr *instr);
u32  *get_nth_arg(Instr *instr, u32 n);
void  align_fixed_offsets(Proc *proc, AlignmentFunc alignment_func);
bool  get_have_function_call(Instrs *instrs);
bool  clutter_return_reg(Instrs *instrs, UsesReturnRegFunc uses_return_reg);

#endif // UTILS_H
