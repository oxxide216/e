#include "utils.h"

void write_cstr(FILE *stream, char *cstr) {
  fwrite(cstr, 1, strlen(cstr), stream);
}

void write_str(FILE *stream, Str str) {
  fwrite(str.ptr, 1, str.len, stream);
}

void write_value(FILE *stream, Value *value) {
  switch (value->kind) {
  case ValueKindSigned: {
    fprintf(stream, "%ld", value->as._signed);
  } break;

  case ValueKindUnsigned: {
    fprintf(stream, "%lu", value->as._unsigned);
  } break;
  }
}

void get_proc_return_kind_and_size(Procs *procs, Str name, ValueKind *kind, u32 *size) {
  for (u32 i = 0; i < procs->len; ++i) {
    if (str_eq(procs->items[i].name, name)) {
      *kind = procs->items[i].return_kind;
      *size = procs->items[i].return_size;
      return;
    }
  }
}

u32 align(u32 value, u32 base) {
  if (value % base == 0)
    return value;
  return value + base - value % base;
}
