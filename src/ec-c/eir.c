#include "eir.h"

EType type_clone(EType *type) {
  EType new_type = *type;
  if (new_type.ptr_target) {
    new_type.ptr_target = malloc(sizeof(EType));
    *new_type.ptr_target = type_clone(type->ptr_target);
  }
  return new_type;
}

bool type_eq(EType *a, EType *b) {
  if (a->kind == ETypeKindPtr)
    return a->kind == b->kind &&
           type_eq(a->ptr_target, b->ptr_target);
  else
    return a->kind == b->kind &&
           (a->kind != ETypeKindStruct ||
            str_eq(a->name, b->name));
}

void type_free(EType *type) {
  if (type->ptr_target)
    type_free(type->ptr_target);
  free(type);
}

EStruct *get_struct(EStructs *structs, Str name) {
  for (u32 i = 0; i < structs->len; ++i)
    if (str_eq(structs->items[i].name, name))
      return structs->items + i;

  return NULL;
}
