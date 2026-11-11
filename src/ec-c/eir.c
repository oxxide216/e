#include "eir.h"

EType type_clone(EType *type) {
  EType new_type = *type;
  if (new_type.kind == ETypeKindPtr) {
    new_type.ptr_target = malloc(sizeof(EType));
    *new_type.ptr_target = type_clone(type->ptr_target);
  } else if (new_type.kind == ETypeKindArray) {
    new_type.array_element = malloc(sizeof(EType));
    *new_type.array_element = type_clone(type->array_element);
  } else if (new_type.kind == ETypeKindTuple) {
    new_type.tuple_types.cap = new_type.tuple_types.len;
    new_type.tuple_types.items =
      malloc(new_type.tuple_types.cap * sizeof(*new_type.tuple_types.items));
    for (u32 i = 0; i < new_type.tuple_types.len; ++i)
      new_type.tuple_types.items[i] = type_clone(type->tuple_types.items + i);
  }
  if (new_type.moved_members) {
    new_type.moved_members = malloc(new_type.moved_members_len);
    memcpy(new_type.moved_members,
           type->moved_members,
           new_type.moved_members_len);
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
  if (type->kind == ETypeKindPtr){
    type_free(type->ptr_target);
    free(type->ptr_target);
  } else if (type->kind == ETypeKindArray) {
    type_free(type->array_element);
    free(type->array_element);
  } else if (type->kind == ETypeKindTuple) {
    for (u32 i = 0; i < type->tuple_types.len; ++i)
      type_free(type->tuple_types.items + i);
    if (type->tuple_types.items)
      free(type->tuple_types.items);
  }
  if (type->moved_members)
    free(type->moved_members);
}

u32 get_type_size(EIr *ir, EType *type) {
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
  case ETypeKindBool: return 1;

  case ETypeKindStruct: {
    u32 size = 0;

    EStruct *_struct = get_struct(ir, type->name);
    for (u32 i = 0; i < _struct->fields.len; ++i)
      size += get_type_size(ir, &_struct->fields.items[i].type);

    return size == 0 ? 1 : size;
  }

  case ETypeKindArray: {
    return get_type_size(ir, type->array_element) * type->array_len;
  }

  case ETypeKindTuple: {
    u32 size = 0;

    for (u32 i = 0; i < type->tuple_types.len; ++i)
      size += get_type_size(ir, type->tuple_types.items + i);

    return size;
  }

  case ETypeKindStr: return 8;
  case ETypeKindPtr: return 8;
  }

  return 0;
}

EStruct *get_struct(EIr *ir, Str name) {
  for (u32 i = 0; i < ir->structs.len; ++i)
    if (str_eq(ir->structs.items[i].name, name))
      return ir->structs.items + i;

  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EStruct *_struct = get_struct(&ir->module_deps.items[i].ir, name);
    if (_struct)
      return _struct;
  }

  return NULL;
}

EField *get_field(EStruct *_struct, Str name) {
  for (u32 i = 0; i < _struct->fields.len; ++i)
    if (str_eq(_struct->fields.items[i].name, name))
      return _struct->fields.items + i;

  return NULL;
}
