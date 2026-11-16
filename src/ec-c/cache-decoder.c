#include "cache-decoder.h"

#define decode_buffer(decoder, buffer, len)        \
  do {                                             \
    if (!decode_buffer_impl(decoder, buffer, len)) \
      return false;                                \
  } while (0)

#define decode_str(decoder, str)        \
  do {                                  \
    if (!decode_str_impl(decoder, str)) \
      return false;                     \
  } while (0)

#define decode_type(decoder, type)        \
  do {                                    \
    if (!decode_type_impl(decoder, type)) \
      return false;                       \
  } while (0)

#define decode_value(decoder, type)        \
  do {                                     \
    if (!decode_value_impl(decoder, type)) \
      return false;                        \
  } while (0)

#define decode_procs(decoder)        \
  do {                               \
    if (!decode_procs_impl(decoder)) \
      return false;                  \
  } while (0)

#define decode_structs(decoder)        \
  do {                                 \
    if (!decode_structs_impl(decoder)) \
      return false;                    \
  } while (0)

#define decode_consts(decoder)        \
  do {                                \
    if (!decode_consts_impl(decoder)) \
      return false;                  \
  } while (0)

#define decode_static_vars(decoder)        \
  do {                                     \
    if (!decode_static_vars_impl(decoder)) \
      return false;                        \
  } while (0)

#define decode_module_deps(decoder)        \
  do {                                     \
    if (!decode_module_deps_impl(decoder)) \
      return false;                        \
  } while (0)

typedef struct {
  EIr   *ir;
  Arena *arena;
  u8    *cache;
  u32    len, decoded;
} Decoder;

static bool decode_buffer_impl(Decoder *decoder, void *buffer, u32 len) {
  if (decoder->decoded + len > decoder->len)
    return false;

  for (u32 i = 0; i < len; ++i)
    ((u8 *) buffer)[i] = decoder->cache[decoder->decoded + i];

  decoder->decoded += len;

  return true;
}

static bool decode_str_impl(Decoder *decoder, Str *str) {
  decode_buffer(decoder, &str->len, sizeof(str->len));
  str->ptr = arena_alloc(decoder->arena, str->len);
  decode_buffer(decoder, str->ptr, str->len);

  return true;
}

static bool decode_type_impl(Decoder *decoder, EType *type) {
  u8 kind;

  *type = (EType) {0};

  decode_buffer(decoder, &kind, 1);
  type->kind = kind;

  if (type->kind == ETypeKindStruct) {
    decode_str(decoder, &type->name);
  } else if (type->kind == ETypeKindPtr) {
    type->ptr_target = malloc(sizeof(EType));
    decode_type(decoder, type->ptr_target);
  } else if (type->kind == ETypeKindArray) {
    type->array_element = malloc(sizeof(EType));
    decode_type(decoder, type->array_element);
    decode_buffer(decoder, &type->array_len, sizeof(type->array_len));
  }

  return true;
}

static bool decode_value_impl(Decoder *decoder, EValue *value) {
  u8 kind;

  *value = (EValue) {0};

  decode_buffer(decoder, &kind, 1);
  value->kind = kind;
  decode_buffer(decoder, &value->as, sizeof(value->as));

  return true;
}

static bool decode_procs_impl(Decoder *decoder) {
  u32 new_len;
  decode_buffer(decoder, &new_len, sizeof(new_len));

  for (u32 i = 0; i < new_len; ++i) {
    EProc proc = {0};

    decode_str(decoder, &proc.name);

    decode_buffer(decoder, &proc.args.len, sizeof(proc.args.len));
    proc.args.cap = proc.args.len;
    proc.args.items = malloc(sizeof(EArg) * proc.args.cap);
    for (u32 j = 0; j < proc.args.len; ++j) {
      decode_str(decoder, &proc.args.items[j].name);
      decode_type(decoder, &proc.args.items[j].type);
    }

    decode_type(decoder, &proc.return_type);

    DA_APPEND(decoder->ir->procs, proc);
  }

  return true;
}

static bool decode_structs_impl(Decoder *decoder) {
  u32 new_len;
  decode_buffer(decoder, &new_len, sizeof(new_len));

  for (u32 i = 0; i < new_len; ++i) {
    EStruct _struct = {0};

    decode_str(decoder, &_struct.name);

    decode_buffer(decoder, &_struct.fields.len,
                  sizeof(_struct.fields.len));
    _struct.fields.cap = _struct.fields.len;
    _struct.fields.items = malloc(_struct.fields.cap * sizeof(EField));

    for (u32 j = 0; j < _struct.fields.len; ++j) {
      decode_str(decoder, &_struct.fields.items[j].name);
      decode_type(decoder, &_struct.fields.items[j].type);
    }

    DA_APPEND(decoder->ir->structs, _struct);
  }

  return true;
}

static bool decode_consts_impl(Decoder *decoder) {
  u32 new_len;
  decode_buffer(decoder, &new_len, sizeof(new_len));

  for (u32 i = 0; i < new_len; ++i) {
    EConst _const = {0};

    decode_str(decoder, &_const.name);
    decode_type(decoder, &_const.type);
    decode_value(decoder, &_const.value);

    DA_APPEND(decoder->ir->consts, _const);
  }

  return true;
}

static bool decode_static_vars_impl(Decoder *decoder) {
  u32 new_len;
  decode_buffer(decoder, &new_len, sizeof(new_len));

  for (u32 i = 0; i < new_len; ++i) {
    EStaticVar static_var = {0};

    decode_str(decoder, &static_var.name);
    decode_type(decoder, &static_var.type);

    DA_APPEND(decoder->ir->static_vars, static_var);
  }

  return true;
}

static bool decode_module_deps_impl(Decoder *decoder) {
  u32 new_len;
  decode_buffer(decoder, &new_len, sizeof(new_len));

  for (u32 i = 0; i < new_len; ++i) {
    EModuleDep dep = {0};

    decode_buffer(decoder, &dep.path.len, sizeof(dep.path.len));
    dep.path.cap = dep.path.len;
    dep.path.items = malloc(dep.path.cap * sizeof(Str));

    for (u32 j = 0; j < dep.path.len; ++j)
      decode_str(decoder, &dep.path.items[j]);

    DA_APPEND(decoder->ir->module_deps, dep);
  }

  return true;
}

bool decode_cache(EIr *ir, Arena *arena, u8 *cache, u32 len, u64 *code_hash) {
  Decoder decoder = { ir, arena, cache, len, 0 };

  decode_buffer(&decoder, code_hash, sizeof(*code_hash));
  decode_procs(&decoder);
  decode_structs(&decoder);
  decode_consts(&decoder);
  decode_static_vars(&decoder);
  decode_module_deps(&decoder);

  ir->is_from_cache = true;

  return true;
}
