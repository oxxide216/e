#include "cache-encoder.h"

static void encode_str(FILE *stream, Str str) {
  fwrite(&str.len, sizeof(str.len), 1, stream);
  fwrite(str.ptr, 1, str.len, stream);
}

static void encode_type(FILE *stream, EType *type) {
  fwrite(&type->kind, 1, 1, stream);
  if (type->kind == ETypeKindStruct) {
    encode_str(stream, type->name);
  } else if (type->kind == ETypeKindPtr) {
    encode_type(stream, type->ptr_target);
  } else if (type->kind == ETypeKindArray) {
    encode_type(stream, type->array_element);
    fwrite(&type->array_len, sizeof(type->array_len), 1, stream);
  }
}

static void encode_procs(FILE *stream, EProcs *procs) {
  fwrite(&procs->len, sizeof(procs->len), 1, stream);

  for (u32 i = 0; i < procs->len; ++i) {
    EProc *proc = procs->items + i;

    encode_str(stream, proc->name);

    fwrite(&proc->args.len, sizeof(proc->args.len), 1, stream);
    for (u32 j = 0; j < proc->args.len; ++j) {
      encode_str(stream, proc->args.items[j].name);
      encode_type(stream, &proc->args.items[j].type);
    }

    encode_type(stream, &proc->return_type);
  }
}

static void encode_structs(FILE *stream, EStructs *structs) {
  fwrite(&structs->len, sizeof(structs->len), 1, stream);

  for (u32 i = 0; i < structs->len; ++i) {
    EStruct *_struct = structs->items + i;

    encode_str(stream, _struct->name);

    fwrite(&_struct->fields.len, sizeof(_struct->fields.len), 1, stream);
    for (u32 j = 0; j < _struct->fields.len; ++j) {
      encode_str(stream, _struct->fields.items[j].name);
      encode_type(stream, &_struct->fields.items[j].type);
    }
  }
}

static void encode_module_deps(FILE *stream, EModuleDeps *deps) {
  fwrite(&deps->len, sizeof(deps->len), 1, stream);

  for (u32 i = 0; i < deps->len; ++i) {
    EModuleDep *dep = deps->items + i;

    fwrite(&dep->path.len, sizeof(dep->path.len), 1, stream);
    for (u32 j = 0; j < dep->path.len; ++j)
      encode_str(stream, dep->path.items[j]);
  }
}

void encode_cache(FILE *stream, EIr *ir, u64 code_hash) {
  fwrite(&code_hash, sizeof(code_hash), 1, stream);
  encode_procs(stream, &ir->procs);
  encode_structs(stream, &ir->structs);
  encode_module_deps(stream, &ir->module_deps);
}
