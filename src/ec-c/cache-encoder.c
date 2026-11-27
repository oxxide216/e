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

static void encode_value(FILE *stream, EValue *value, bool is_root) {
  if (is_root)
    encode_type(stream, &value->type);
  switch (value->type.kind) {
  case ETypeKindUnit: break;

  case ETypeKindS8:
  case ETypeKindS16:
  case ETypeKindS32:
  case ETypeKindS64: {
    fwrite(&value->as._signed, sizeof(value->as._signed), 1, stream);
  } break;

  case ETypeKindU8:
  case ETypeKindU16:
  case ETypeKindU32:
  case ETypeKindU64: {
    fwrite(&value->as._unsigned, sizeof(value->as._unsigned), 1, stream);
  } break;

  case ETypeKindBool: {
    fwrite(&value->as._bool, sizeof(value->as._bool), 1, stream);
  } break;

  case ETypeKindStruct: {
    fwrite(&value->as.struct_fields.len,
           sizeof(value->as.struct_fields.len),
           1, stream);

    for (u32 i = 0; i < value->as.struct_fields.len; ++i) {
      encode_str(stream, value->as.struct_fields.items[i].name);
      encode_value(stream, value->as.struct_fields.items[i].value, false);
    }
  } break;

  case ETypeKindArray: {
    fwrite(&value->as.array_elements.len,
           sizeof(value->as.array_elements.len),
           1, stream);

    for (u32 i = 0; i < value->as.array_elements.len; ++i)
      encode_value(stream, value->as.array_elements.items + i, false);
  } break;

  case ETypeKindTuple: {
    fwrite(&value->as.tuple_fields.len,
           sizeof(value->as.tuple_fields.len),
           1, stream);

    for (u32 i = 0; i < value->as.tuple_fields.len; ++i)
      encode_value(stream, value->as.tuple_fields.items + i, false);
  } break;

  case ETypeKindStr: break;
  case ETypeKindPtr: break;
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

static void encode_consts(FILE *stream, EConsts *consts) {
  fwrite(&consts->len, sizeof(consts->len), 1, stream);

  for (u32 i = 0; i < consts->len; ++i) {
    EConst *_const = consts->items + i;

    encode_str(stream, _const->name);
    encode_value(stream, &_const->value, true);
  }
}

static void encode_static_vars(FILE *stream, EStaticVars *static_vars) {
  fwrite(&static_vars->len, sizeof(static_vars->len), 1, stream);

  for (u32 i = 0; i < static_vars->len; ++i) {
    EStaticVar *static_var = static_vars->items + i;

    encode_str(stream, static_var->name);
    encode_type(stream, &static_var->type);
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
  encode_consts(stream, &ir->consts);
  encode_static_vars(stream, &ir->static_vars);
  encode_module_deps(stream, &ir->module_deps);
}
