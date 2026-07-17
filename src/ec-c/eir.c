#include "eir.h"

EType type_clone(EType *type) {
  EType new_type = *type;
  if (new_type.kind == ETypeKindPtr) {
    new_type.ptr_target = malloc(sizeof(EType));
    *new_type.ptr_target = type_clone(type->ptr_target);
  } else if (new_type.kind == ETypeKindArray) {
    new_type.array_element = malloc(sizeof(EType));
    *new_type.array_element = type_clone(type->array_element);
  }
  return new_type;
}

bool type_eq(EType *a, EType *b) {
  if (a->kind == ETypeKindPtr) {
    return a->kind == b->kind &&
           (!a->ptr_target || !b->ptr_target ||
            type_eq(a->ptr_target, b->ptr_target));
  } else if (a->kind == ETypeKindArray) {
    return a->kind == b->kind &&
           a->array_element && b->array_element &&
           type_eq(a->array_element, b->array_element) &&
           a->array_len == b->array_len;
  } else if (a->kind == ETypeKindTuple) {
    if (a->kind != b->kind ||
        a->tuple_types.len != b->tuple_types.len)
      return false;
    for (u32 i = 0; i < a->tuple_types.len; ++i)
      if (!type_eq(a->tuple_types.items + i, b->tuple_types.items + i))
        return false;
    return true;
  } else {
    return a->kind == b->kind &&
           (a->kind != ETypeKindStruct ||
            str_eq(a->name, b->name));
  }
}

void type_free(EType *type) {
  if (type->kind == ETypeKindPtr)
    type_free(type->ptr_target);
  else if (type->kind == ETypeKindArray)
    type_free(type->array_element);
  free(type);
}

u32 get_type_size(EStructs *structs, EType *type) {
  switch (type->kind) {
  case ETypeKindUnit: return 0;
  case ETypeKindS8:   return 1;
  case ETypeKindS16:  return 2;
  case ETypeKindS32:  return 4;
  case ETypeKindS64:  return 8;
  case ETypeKindU8:   return 1;
  case ETypeKindU16:  return 2;
  case ETypeKindU32:  return 4;
  case ETypeKindU64:  return 8;
  case ETypeKindBool: return 4;

  case ETypeKindStruct: {
    u32 size = 0;

    EStruct *_struct = get_struct(structs, type->name);
    for (u32 i = 0; i < _struct->fields.len; ++i)
      size += get_type_size(structs, &_struct->fields.items[i].type);

    return size == 0 ? 1 : size;
  }

  case ETypeKindArray: {
    return get_type_size(structs, type->array_element) * type->array_len;
  }

  case ETypeKindTuple: {
    u32 size = 0;

    for (u32 i = 0; i < type->tuple_types.len; ++i)
      size += get_type_size(structs, type->tuple_types.items + i);

    return size;
  }

  case ETypeKindStr: return 8;
  case ETypeKindPtr: return 8;
  }

  return 0;
}

EStruct *get_struct(EStructs *structs, Str name) {
  for (u32 i = 0; i < structs->len; ++i)
    if (str_eq(structs->items[i].name, name))
      return structs->items + i;

  return NULL;
}

EField *get_field(EStruct *_struct, Str name) {
  for (u32 i = 0; i < _struct->fields.len; ++i)
    if (str_eq(_struct->fields.items[i].name, name))
      return _struct->fields.items + i;

  return NULL;
}
