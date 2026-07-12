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

  decode_buffer(decoder, &kind, 1);
  type->kind = kind;

  if (type->kind == ETypeKindStruct) {
    decode_str(decoder, &type->name);
  } else if (type->kind == ETypeKindPtr) {
    type->ptr_target = arena_alloc(decoder->arena, sizeof(EType));
    decode_type(decoder, type->ptr_target);
  }

  return true;
}

static bool decode_procs_impl(Decoder *decoder) {
  u32 new_len;
  decode_buffer(decoder, &new_len, sizeof(new_len));
  if (decoder->ir->procs.cap < decoder->ir->procs.len + new_len) {
    decoder->ir->procs.cap = decoder->ir->procs.len + new_len;

    if (decoder->ir->procs.items) {
      EProc *new_items =
        arena_alloc(decoder->arena, decoder->ir->procs.cap * sizeof(EProc));
      memcpy(new_items, decoder->ir->procs.items, decoder->ir->procs.len * sizeof(EProc));
      decoder->ir->procs.items = new_items;
    } else {
      decoder->ir->procs.items =
        arena_alloc(decoder->arena, decoder->ir->procs.cap * sizeof(EProc));
    }
  }

  for (u32 i = 0; i < new_len; ++i) {
    EProc *proc = decoder->ir->procs.items + decoder->ir->procs.len + i;

    decode_str(decoder, &proc->name);

    decode_buffer(decoder, &proc->args.len, sizeof(proc->args.len));
    proc->args.cap = proc->args.len;
    proc->args.items = arena_alloc(decoder->arena, sizeof(EArg) * proc->args.cap);
    for (u32 j = 0; j < proc->args.len; ++j) {
      decode_str(decoder, &proc->args.items[j].name);
      decode_type(decoder, &proc->args.items[j].type);
    }

    decode_type(decoder, &proc->return_type);
  }

  decoder->ir->procs.len += new_len;

  return true;
}

static bool decode_structs_impl(Decoder *decoder) {
  u32 new_len;
  decode_buffer(decoder, &new_len, sizeof(new_len));
  if (decoder->ir->structs.cap < decoder->ir->structs.len + new_len) {
    decoder->ir->structs.cap = decoder->ir->structs.len + new_len;

    if (decoder->ir->structs.items) {
      EStruct *new_items =
        arena_alloc(decoder->arena, decoder->ir->structs.cap * sizeof(EStruct));
      memcpy(new_items, decoder->ir->structs.items, decoder->ir->structs.len * sizeof(EStruct));
      decoder->ir->structs.items = new_items;
    } else {
      decoder->ir->structs.items =
        arena_alloc(decoder->arena, decoder->ir->structs.cap * sizeof(EStruct));
    }
  }

  for (u32 i = 0; i < new_len; ++i) {
    EStruct *_struct = decoder->ir->structs.items + decoder->ir->structs.len + i;

    decode_str(decoder, &_struct->name);

    decode_buffer(decoder, &_struct->fields.len,
                  sizeof(_struct->fields.len));
    _struct->fields.cap = _struct->fields.len;
    _struct->fields.items =
      arena_alloc(decoder->arena, _struct->fields.cap * sizeof(EField));

    for (u32 j = 0; j < _struct->fields.len; ++j) {
      decode_str(decoder, &_struct->fields.items[j].name);
      decode_type(decoder, &_struct->fields.items[j].type);
    }
  }

  decoder->ir->structs.len += new_len;

  return true;
}

static bool decode_module_deps_impl(Decoder *decoder) {
  u32 new_len;
  decode_buffer(decoder, &new_len, sizeof(new_len));
  if (decoder->ir->module_deps.cap < decoder->ir->module_deps.len + new_len) {
    decoder->ir->module_deps.cap = decoder->ir->module_deps.len + new_len;

    if (decoder->ir->module_deps.items) {
      EModuleDep *new_items =
        arena_alloc(decoder->arena, decoder->ir->module_deps.cap * sizeof(EModuleDep));
      memcpy(new_items, decoder->ir->module_deps.items, decoder->ir->module_deps.len * sizeof(EModuleDep));
      decoder->ir->module_deps.items = new_items;
    } else {
      decoder->ir->module_deps.items =
        arena_alloc(decoder->arena, decoder->ir->module_deps.cap * sizeof(EModuleDep));
    }
  }

  for (u32 i = 0; i < new_len; ++i) {
    EModuleDep *dep = decoder->ir->module_deps.items + decoder->ir->module_deps.len + i;

    decode_buffer(decoder, &dep->path.len, sizeof(dep->path.len));
    dep->path.cap = dep->path.len;
    dep->path.items =
      arena_alloc(decoder->arena, dep->path.cap * sizeof(Str));

    for (u32 j = 0; j < dep->path.len; ++j)
      decode_str(decoder, &dep->path.items[j]);
  }

  decoder->ir->module_deps.len += new_len;

  return true;
}

bool decode_cache(EIr *ir, Arena *arena, u8 *cache, u32 len) {
  Decoder decoder = { ir, arena, cache, len, 0 };

  decode_procs(&decoder);
  decode_structs(&decoder);
  decode_module_deps(&decoder);

  return true;
}
