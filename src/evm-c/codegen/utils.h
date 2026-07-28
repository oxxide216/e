#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

#include "codegen.h"
#include "shl/shl-defs.h"
#include "shl/shl-str.h"

void write_cstr(FILE *stream, char *cstr);
void write_str(FILE *stream, Str str);
void write_value(FILE *stream, Value *value);

void get_proc_return_kind_and_size(Procs *procs, Str name, ValueKind *kind, u32 *size);
i32  align(i32 value, i32 base);

#endif // UTILS_H
