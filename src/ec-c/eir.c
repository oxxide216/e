#include "eir.h"
#include "utils.h"
#include "shl/shl-log.h"

EType type_clone(EType *type) {
  EType new_type = *type;
  if (new_type.kind == ETypeKindPtr) {
    if (type->ptr_target) {
      new_type.ptr_target = malloc(sizeof(EType));
      *new_type.ptr_target = type_clone(type->ptr_target);
    }
  } else if (new_type.kind == ETypeKindArray) {
    new_type.array_element = malloc(sizeof(EType));
    *new_type.array_element = type_clone(type->array_element);
  } else if (new_type.kind == ETypeKindTuple) {
    new_type.tuple_types.cap = new_type.tuple_types.len;
    new_type.tuple_types.items =
      malloc(new_type.tuple_types.cap * sizeof(*new_type.tuple_types.items));
    for (u32 i = 0; i < new_type.tuple_types.len; ++i)
      new_type.tuple_types.items[i] = type_clone(type->tuple_types.items + i);
  } else if (new_type.kind == ETypeKindStruct) {
    new_type.generics.cap = new_type.generics.len;
    new_type.generics.items =
      malloc(new_type.generics.cap * sizeof(*new_type.generics.items));
    for (u32 i = 0; i < new_type.generics.len; ++i)
      new_type.generics.items[i] = type_clone(type->generics.items + i);
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
  } else if (a->kind == ETypeKindStruct && a->kind != b->kind) {
    return a->_struct == NULL;
  } else if (b->kind == ETypeKindStruct && a->kind != b->kind) {
    return b->_struct == NULL;
  } else {
    if (a->kind != b->kind ||
        (a->kind == ETypeKindStruct &&
         a->_struct && b->_struct &&
         a->_struct != b->_struct))
      return false;

    if (a->kind == ETypeKindStruct) {
      if (a->generics.len != b->generics.len)
        return false;

      for (u32 i = 0; i < a->generics.len; ++i)
        if (!type_eq(a->generics.items + i, b->generics.items + i))
          return false;
    }

    return true;
  }
}

void type_free(EType *type) {
  if (type->kind == ETypeKindPtr){
    if (type->ptr_target) {
      type_free(type->ptr_target);
      free(type->ptr_target);
    }
  } else if (type->kind == ETypeKindArray) {
    type_free(type->array_element);
    free(type->array_element);
  } else if (type->kind == ETypeKindTuple) {
    for (u32 i = 0; i < type->tuple_types.len; ++i)
      type_free(type->tuple_types.items + i);
    if (type->tuple_types.items)
      free(type->tuple_types.items);
  } else if (type->kind == ETypeKindStruct) {
    if (type->generics.items)
      free(type->generics.items);
  }
  if (type->moved_members)
    free(type->moved_members);
}

EProc *get_proc(EIr *ir, Str name, ETypeRefs *arg_types,
                bool ignore_paths, bool ignore_generics,
                EIr **out_ir, Varss **out_varss) {
  if (!ignore_paths) {
    u32 index = str_find(name, STR_LIT("::"));
    if (index != (u32) -1) {
      Str prefix = { name.ptr, index };

      name.ptr += index + 2;
      name.len -= index + 2;

      for (u32 i = 0; i < ir->module_deps.len; ++i) {
        EModuleDep *dep = ir->module_deps.items + i;

        if (str_eq(dep->path.items[dep->path.len - 1], prefix)) {
          if (out_ir)
            *out_ir = &dep->ir;
          if (out_varss)
            *out_varss = &dep->varss;
          return get_proc(&dep->ir, name, arg_types, ignore_paths,
                          ignore_generics, out_ir, out_varss);
        }
      }

      return NULL;
    }
  }

  for (u32 i = 0; i < ir->procs.len; ++i) {
    EProc *proc = ir->procs.items + i;

    if (!str_eq(proc->name, name) || proc->args.len != arg_types->len)
      continue;

    if (ignore_generics && proc->generics.len > 0)
      continue;

    bool all = true;

    for (u32 j = 0; j < proc->args.len; ++j) {
      if (!type_eq(&proc->args.items[j].type, arg_types->items[j])) {
        all = false;
        break;
      }
    }

    if (all)
      return proc;
  }

  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;

    for (u32 j = 0; j < dep->imports.len; ++j) {
      if (str_eq(dep->imports.items[j], name)) {
        if (out_ir)
          *out_ir = &dep->ir;
        if (out_varss)
          *out_varss = &dep->varss;
        return get_proc(&dep->ir, name, arg_types, ignore_paths,
                        ignore_generics, out_ir, out_varss);
      }
    }

    if (ignore_paths) {
      EProc *proc = get_proc(&dep->ir, name, arg_types,
                             ignore_paths, ignore_generics,
                             out_ir, out_varss);
      if (proc) {
        if (out_ir)
          *out_ir = &dep->ir;
        if (out_varss)
          *out_varss = &dep->varss;
        return proc;
      }
    }
  }

  return NULL;
}

EModulePath get_proc_path(EIr *ir, Str name,
                          Indices *arg_indices,
                          Vars *vars,
                          EModulePath *path,
                          bool ignore_paths) {
  if (!ignore_paths) {
    u32 index = str_find(name, STR_LIT("::"));
    if (index != (u32) -1) {
      Str prefix = { name.ptr, index };

      name.ptr += index + 2;
      name.len -= index + 2;

      EModulePath temp_path = *path;
      if (temp_path.len > 0)
      --temp_path.len;

      for (u32 i = 0; i < ir->module_deps.len; ++i) {
        EModuleDep *dep = ir->module_deps.items + i;

        if (str_eq(dep->path.items[dep->path.len - 1], prefix)) {
          EModulePath new_path;
          if (dep->is_submodule)
            new_path = merge_module_paths(&temp_path, &dep->path);
          else
            new_path = dup_module_path(&dep->path);

          EModulePath result =
            get_proc_path(&dep->ir, name, arg_indices,
                          vars, &new_path, ignore_paths);

          if (result.len == 0)
            free(new_path.items);

          return result;
        }
      }

      return (EModulePath) {0};
    }
  }

  for (u32 i = 0; i < ir->procs.len; ++i) {
    EProc *proc = ir->procs.items + i;

    if (!str_eq(proc->name, name) || proc->args.len != arg_indices->len)
      continue;

    bool all = true;

    for (u32 j = 0; j < proc->args.len; ++j) {
      if (!type_eq(&proc->args.items[j].type,
                   &vars->items[arg_indices->items[j]].type)) {
        all = false;
        break;
      }
    }

    if (all)
      return *path;
  }

  EModulePath temp_path = *path;
  if (temp_path.len > 0)
    --temp_path.len;

  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;

    bool found = false;

    for (u32 j = 0; j < dep->imports.len; ++j) {
      if (str_eq(dep->imports.items[j], name)) {
        found = true;
        break;
      }
    }

    if (found) {
      if (dep->is_submodule)
        return merge_module_paths(&temp_path, &dep->path);
      else
        return dup_module_path(&dep->path);
    }

    if (ignore_paths) {
      EModulePath new_path = merge_module_paths(&temp_path, &dep->path);

      EModulePath result =
        get_proc_path(&dep->ir, name, arg_indices,
                      vars, &new_path, ignore_paths);
      if (result.len > 0)
        return result;
      else
        free(new_path.items);
    }
  }

  return (EModulePath) {0};
}

EStruct *get_struct(EIr *ir, Str name) {
  u32 index = str_find(name, STR_LIT("::"));
  if (index != (u32) -1) {
    Str prefix = { name.ptr, index };

    name.ptr += index + 2;
    name.len -= index + 2;

    for (u32 i = 0; i < ir->module_deps.len; ++i) {
      EModuleDep *dep = ir->module_deps.items + i;

      if (str_eq(dep->path.items[dep->path.len - 1], prefix))
        return get_struct(&dep->ir, name);
    }

    return NULL;
  }

  for (u32 i = 0; i < ir->structs.len; ++i)
    if (str_eq(ir->structs.items[i].name, name))
      return ir->structs.items + i;

  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;

    for (u32 j = 0; j < dep->imports.len; ++j)
      if (str_eq(dep->imports.items[j], name))
        return get_struct(&dep->ir, name);
  }

  return NULL;
}

EIr *get_struct_ir(EIr *ir, Str name) {
  u32 index = str_find(name, STR_LIT("::"));
  if (index != (u32) -1) {
    Str prefix = { name.ptr, index };

    name.ptr += index + 2;
    name.len -= index + 2;

    for (u32 i = 0; i < ir->module_deps.len; ++i) {
      EModuleDep *dep = ir->module_deps.items + i;

      if (str_eq(dep->path.items[dep->path.len - 1], prefix))
        return get_struct_ir(&dep->ir, name);
    }

    return NULL;
  }

  for (u32 i = 0; i < ir->structs.len; ++i)
    if (str_eq(ir->structs.items[i].name, name))
      return ir;

  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;

    for (u32 j = 0; j < dep->imports.len; ++j)
      if (str_eq(dep->imports.items[j], name))
        return get_struct_ir(&dep->ir, name);
  }

  return NULL;
}

EField *get_field(EStruct *_struct, Str name) {
  for (u32 i = 0; i < _struct->fields.len; ++i)
    if (str_eq(_struct->fields.items[i].name, name))
      return _struct->fields.items + i;

  return NULL;
}

EStaticVar *get_static_var(EIr *ir, Str name) {
  for (u32 i = 0; i < ir->static_vars.len; ++i)
    if (str_eq(ir->static_vars.items[i].name, name))
      return ir->static_vars.items + i;

  return NULL;
}


bool type_contains_generics(EType *type) {
  if (type->kind == ETypeKindStruct) {
    if (!type->_struct)
      return true;

    for (u32 i = 0; i < type->generics.len; ++i)
      if (type_contains_generics(type->generics.items + i))
        return true;
  } else if (type->kind == ETypeKindPtr) {
    return type_contains_generics(type->ptr_target);
  } else if (type->kind == ETypeKindArray) {
    return type_contains_generics(type->array_element);
  } else if (type->kind == ETypeKindTuple) {
    for (u32 i = 0; i < type->tuple_types.len; ++i)
      if (type_contains_generics(type->tuple_types.items + i))
        return true;
  }

  return false;
}

void _type_replace_generics(EType *type, ENames *generic_names, ETypes *generics) {
  if (type->kind == ETypeKindStruct) {
    for (u32 i = 0; i < type->generics.len; ++i)
      _type_replace_generics(type->generics.items + i, generic_names, generics);

    if (!type->_struct) {
      for (u32 i = 0; i < generic_names->len; ++i) {
        if (str_eq(generic_names->items[i], type->name)) {
          *type = type_clone(generics->items + i);
          break;
        }
      }
    }
  } else if (type->kind == ETypeKindPtr) {
    _type_replace_generics(type->ptr_target, generic_names, generics);
  } else if (type->kind == ETypeKindArray) {
    _type_replace_generics(type->array_element, generic_names, generics);
  } else if (type->kind == ETypeKindTuple) {
    for (u32 i = 0; i < type->tuple_types.len; ++i)
      _type_replace_generics(type->tuple_types.items + i, generic_names, generics);
  }
}

EType type_replace_generics(EType *type, ENames *generic_names, ETypes *generics) {
  EType result = type_clone(type);
  _type_replace_generics(&result, generic_names, generics);
  return result;
}

Str get_type_str(EType *type) {
  if (type->kind == ETypeKindStruct) {
    StringBuilder sb = {0};

    sb_push_str(&sb, type->name);
    if (type->generics.len > 0) {
      sb_push_char(&sb, '\\');
      for (u32 i = 0; i < type->generics.len; ++i) {
        if (i > 0)
          sb_push(&sb, ", ");
        Str generic_str = get_type_str(type->generics.items + i);
        sb_push_str(&sb, generic_str);
        free_type_str(generic_str, type->generics.items + i);
      }
      sb_push_char(&sb, '/');
    }

    return sb_to_str(sb);
  } else {
    switch (type->kind) {
    case ETypeKindUnit:   return STR_LIT("unit");
    case ETypeKindS8:     return STR_LIT("s8");
    case ETypeKindS16:    return STR_LIT("s16");
    case ETypeKindS32:    return STR_LIT("s32");
    case ETypeKindS64:    return STR_LIT("s64");
    case ETypeKindU8:     return STR_LIT("u8");
    case ETypeKindU16:    return STR_LIT("u16");
    case ETypeKindU32:    return STR_LIT("u32");
    case ETypeKindU64:    return STR_LIT("u64");
    case ETypeKindBool:   return STR_LIT("bool");
    case ETypeKindStruct: return STR_LIT("struct"); // unreachable

    case ETypeKindArray: {
      Str target_str = get_type_str(type->array_element);
      StringBuilder sb = {0};
      sb_push_char(&sb, '[');
      sb_push_str(&sb, target_str);
      sb_push(&sb, "; ");
      sb_push_u32(&sb, type->array_len);
      sb_push_char(&sb, ']');
      free_type_str(target_str, type->array_element);
      return sb_to_str(sb);
    }

    case ETypeKindTuple: {
      StringBuilder sb = {0};
      sb_push_char(&sb, '(');
      for (u32 i = 0; i < type->tuple_types.len; ++i) {
        if (i > 0)
          sb_push(&sb, ", ");
        Str type_str = get_type_str(type->tuple_types.items + i);
        sb_push_str(&sb, type_str);
        free_type_str(type_str, type->tuple_types.items + i);
      }
      sb_push_char(&sb, ')');
      return sb_to_str(sb);
    }

    case ETypeKindStr: return STR_LIT("str");

    case ETypeKindPtr: {
      Str target_str;
      if (type->ptr_target)
        target_str = get_type_str(type->ptr_target);
      else
        target_str = STR_LIT("unit");
      Str result;
      result.len = target_str.len + 1;
      result.ptr = malloc(result.len);
      result.ptr[0] = '&';
      memcpy(result.ptr + 1, target_str.ptr, target_str.len);
      if (type->ptr_target)
        free_type_str(target_str, type->ptr_target);
      return result;
    }
    }
  }

  ERROR("Unreachable\n");
  return (Str) {0};
}

void free_type_str(Str str, EType *type) {
  if (type->kind == ETypeKindPtr ||
      type->kind == ETypeKindArray ||
      type->kind == ETypeKindStruct)
    free(str.ptr);
}

bool check_struct_existence(EIr *ir, EType *type,
                            ENames *generic_names,
                            ETypes *generics) {
  if (type->kind == ETypeKindStruct) {
    if (generic_names)
      for (u32 i = 0; i < generic_names->len; ++i)
        if (str_eq(generic_names->items[i], type->name))
          return !generics || check_struct_existence(ir, generics->items + i,
                                                     generic_names, generics);

    type->_struct = get_struct(ir, type->name);
    if (!type->_struct) {
      PERROR(STR_FMT":%u:%u: ", "Structure "STR_FMT" was not defined\n",
             STR_ARG(type->loc.file_path),
             type->loc.row + 1,
             type->loc.col + 1,
             STR_ARG(type->name));
      return false;
    }
  } else if (type->kind == ETypeKindPtr) {
    return check_struct_existence(ir, type->ptr_target, generic_names, generics);
  } else if (type->kind == ETypeKindArray) {
    return check_struct_existence(ir, type->array_element, generic_names, generics);
  } else if (type->kind == ETypeKindTuple) {
    for (u32 i = 0; i < type->tuple_types.len; ++i)
      if (!check_struct_existence(ir, type->tuple_types.items + i, generic_names, generics))
        return false;
  }

  return true;
}

bool check_struct_existence_value(EIr *ir, EValue *value,
                                  ENames *generic_names,
                                  ETypes *generics) {
  if (!check_struct_existence(ir, &value->type, generic_names, generics))
    return false;

  if (value->type.kind == ETypeKindStruct) {
    for (u32 i = 0; i < value->as.struct_fields.len; ++i) {
      EField *field = NULL;

      for (u32 j = 0; j < value->type._struct->fields.len; ++j) {
        if (str_eq(value->type._struct->fields.items[j].name,
                   value->as.struct_fields.items[i].name)) {
          field = value->type._struct->fields.items + j;
          break;
        }
      }

      if (!field) {
        PERROR(STR_FMT":%u:%u: ", "Unexpected field "STR_FMT" in structure "STR_FMT"\n",
               STR_ARG(value->type.loc.file_path),
               value->type.loc.row + 1,
               value->type.loc.col + 1,
               STR_ARG(value->as.struct_fields.items[i].name),
               STR_ARG(value->type.name));
        return false;
      } else if (!type_eq(&value->as.struct_fields.items[i].value->type,
                          &field->type)) {
        Str field_type_str = get_type_str(&field->type);
        Str value_field_type_str = get_type_str(&value->as.struct_fields.items[i].value->type);
        PERROR(STR_FMT":%u:%u: ", "Unexpected type of field "STR_FMT" in structure "STR_FMT": expected "STR_FMT", got "STR_FMT"\n",
               STR_ARG(value->as.struct_fields.items[i].value->type.loc.file_path),
               value->as.struct_fields.items[i].value->type.loc.row + 1,
               value->as.struct_fields.items[i].value->type.loc.col + 1,
               STR_ARG(value->as.struct_fields.items[i].name),
               STR_ARG(value->type.name),
               STR_ARG(field_type_str),
               STR_ARG(value_field_type_str));
        free_type_str(field_type_str, &field->type);
        free_type_str(value_field_type_str, &value->as.struct_fields.items[i].value->type);
        return false;
      }

      if (!check_struct_existence_value(ir, value->as.struct_fields.items[i].value,
                                        generic_names, generics))
        return false;
    }
  } else if (value->type.kind == ETypeKindArray) {
    for (u32 i = 0; i < value->as.array_elements.len; ++i)
      if (!check_struct_existence_value(ir, value->as.array_elements.items + i,
                                        generic_names, generics))
        return false;
  } else if (value->type.kind == ETypeKindTuple) {
    for (u32 i = 0; i < value->as.tuple_fields.len; ++i)
      if (!check_struct_existence_value(ir, value->as.tuple_fields.items + i,
                                        generic_names, generics))
        return false;
  }

  return true;
}

bool populate_type_struct_ptrs(EIr *ir) {
  for (u32 i = 0; i < ir->structs.len; ++i) {
    EStruct *_struct = ir->structs.items + i;

    for (u32 j = 0; j < _struct->fields.len; ++j) {
      if (!check_struct_existence(ir, &_struct->fields.items[j].type,
                                  &_struct->generics, NULL))
        return false;
    }
  }

  for (u32 i = 0; i < ir->consts.len; ++i)
    if (!check_struct_existence_value(ir, &ir->consts.items[i].value, NULL, NULL))
      return false;

  for (u32 i = 0; i < ir->static_vars.len; ++i)
    if (!check_struct_existence(ir, &ir->static_vars.items[i].type, NULL, NULL))
      return false;

  for (u32 i = 0; i < ir->procs.len; ++i) {
    EProc *proc = ir->procs.items + i;

    for (u32 j = 0; j < proc->args.len; ++j)
      if (!check_struct_existence(ir, &proc->args.items[j].type, &proc->generics, NULL))
        return false;
    if (!check_struct_existence(ir, &proc->return_type, &proc->generics, NULL))
      return false;

    for (u32 i = 0; i < proc->instrs.len; ++i) {
      EInstr *instr = proc->instrs.items + i;

      if (instr->kind == EInstrKindCast) {
        if (!check_struct_existence(ir, &instr->as.cast.dest_type, &proc->generics, NULL))
          return false;
      } else if (instr->kind == EInstrKindSizeOf) {
        if (!check_struct_existence(ir, &instr->as.size_of.type, &proc->generics, NULL))
          return false;
      }
    }
  }

  return true;
}

void proc_clone(EIr *ir, Varss *varss, EProc *proc, EProc *out_proc, Vars *out_vars) {
  EProc result = *proc;

  if (result.args.len > 0) {
    result.args.items = malloc(result.args.len * sizeof(*result.args.items));
    for (u32 i = 0; i < proc->args.len; ++i) {
      result.args.items[i] = proc->args.items[i];
      result.args.items[i].type = type_clone(&result.args.items[i].type);
    }
  }

  result.return_type = type_clone(&proc->return_type);

  if (result.instrs.len > 0) {
    result.instrs.items = malloc(result.instrs.len * sizeof(*result.instrs.items));
    for (u32 i = 0; i < proc->instrs.len; ++i) {
      EInstr *instr = proc->instrs.items + i;

      result.instrs.items[i] = *instr;

      if (instr->kind == EInstrKindCast) {
        result.instrs.items[i].as.cast.dest_type =
          type_clone(&instr->as.cast.dest_type);
      } else if (instr->kind == EInstrKindSizeOf) {
        result.instrs.items[i].as.size_of.type =
          type_clone(&instr->as.size_of.type);
      } else if (instr->kind == EInstrKindCall) {
        result.instrs.items[i].as.call.arg_indices.items =
          malloc(result.instrs.items[i].as.call.arg_indices.len *
                 sizeof(*result.instrs.items[i].as.call.arg_indices.items));
        memcpy(result.instrs.items[i].as.call.arg_indices.items,
               instr->as.call.arg_indices.items,
               instr->as.call.arg_indices.len *
               sizeof(*instr->as.call.arg_indices.items));
      } else if (instr->kind == EInstrKindCallAssign) {
        result.instrs.items[i].as.call_assign.arg_indices.items =
          malloc(result.instrs.items[i].as.call_assign.arg_indices.len *
                 sizeof(*result.instrs.items[i].as.call_assign.arg_indices.items));
        memcpy(result.instrs.items[i].as.call_assign.arg_indices.items,
               instr->as.call_assign.arg_indices.items,
               instr->as.call_assign.arg_indices.len *
               sizeof(*instr->as.call_assign.arg_indices.items));
      } else if (instr->kind == EInstrKindInlineAsm) {
        result.instrs.items[i].as.inline_asm.segments.items =
          malloc(result.instrs.items[i].as.inline_asm.segments.len *
                 sizeof(*result.instrs.items[i].as.inline_asm.segments.items));
        memcpy(result.instrs.items[i].as.inline_asm.segments.items,
               instr->as.inline_asm.segments.items,
               instr->as.inline_asm.segments.len *
               sizeof(*instr->as.inline_asm.segments.items));
      }
    }
  }

  u32 proc_index = proc - ir->procs.items;

  Vars new_vars;
  new_vars.len = varss->items[proc_index].len;
  new_vars.cap = new_vars.len;
  new_vars.items = malloc(new_vars.cap * sizeof(*new_vars.items));
  for (u32 i = 0; i < new_vars.len; ++i) {
    new_vars.items[i] = varss->items[proc_index].items[i];
    new_vars.items[i].type = type_clone(&new_vars.items[i].type);
  }

  *out_proc = result;
  *out_vars = new_vars;
}

void varss_destroy(Varss *varss) {
  for (u32 i = 0; i < varss->len; ++i) {
    Vars *vars = varss->items + i;

    for (u32 j = 0; j < vars->len; ++j)
      type_free(&vars->items[j].type);
    if (vars->items)
      free(vars->items);
  }
  if (varss->items)
    free(varss->items);
}

void _proc_destroy(EProc *proc) {
  for (u32 i = 0; i < proc->args.len; ++i)
    type_free(&proc->args.items[i].type);
  if (proc->args.items)
    free(proc->args.items);

  type_free(&proc->return_type);

  for (u32 i = 0; i < proc->instrs.len; ++i) {
    EInstr *instr = proc->instrs.items + i;

    if (instr->kind == EInstrKindStore) {
      type_free(&instr->as.store.value.type);
    } else if (instr->kind == EInstrKindCast) {
      type_free(&instr->as.cast.dest_type);
    } else if (instr->kind == EInstrKindSizeOf) {
      type_free(&instr->as.size_of.type);
    } else if (instr->kind == EInstrKindCall) {
      if (instr->as.call.arg_indices.items)
        free(instr->as.call.arg_indices.items);
    } else if (instr->kind == EInstrKindCallAssign) {
      if (instr->as.call_assign.arg_indices.items)
        free(instr->as.call_assign.arg_indices.items);
    } else if (instr->kind == EInstrKindInlineAsm) {
      if (instr->as.inline_asm.segments.items)
        free(instr->as.inline_asm.segments.items);
    }
  }
  if (proc->instrs.items)
    free(proc->instrs.items);
}

bool proc_destroy(EIr *ir, Varss *varss, EProc *proc) {
  for (u32 i = 0; i < ir->procs.len; ++i) {
    if (ir->procs.items + i == proc) {
      _proc_destroy(proc);
      DA_REMOVE_AT(ir->procs, i);
      DA_REMOVE_AT(*varss, i);
      return true;
    }
  }

  for (u32 i = 0; i < ir->module_deps.len; ++i)
    if (proc_destroy(&ir->module_deps.items[i].ir,
                     &ir->module_deps.items[i].varss,
                     proc))
      return true;

  return false;
}

void ir_destroy(EIr *ir) {
  if (ir->is_copy)
    return;

  for (u32 i = 0; i < ir->procs.len; ++i)
    _proc_destroy(ir->procs.items + i);
  if (ir->procs.items)
    free(ir->procs.items);

  for (u32 i = 0; i < ir->structs.len; ++i) {
    EStruct *_struct = ir->structs.items + i;

    for (u32 j = 0; j < _struct->fields.len; ++j)
        type_free(&_struct->fields.items[j].type);
    if (_struct->fields.items)
      free(_struct->fields.items);
  }
  if (ir->structs.items)
    free(ir->structs.items);

  for (u32 i = 0; i < ir->consts.len; ++i)
    type_free(&ir->consts.items[i].value.type);
  if (ir->consts.items)
    free(ir->consts.items);

  for (u32 i = 0; i < ir->data.len; ++i) {
    EDataEntry *entry = ir->data.items + i;

    if (entry->data)
      free(entry->data);
  }
  if (ir->data.items)
    free(ir->data.items);

  for (u32 i = 0; i < ir->static_vars.len; ++i)
    type_free(&ir->static_vars.items[i].type);
  if (ir->static_vars.items)
    free(ir->static_vars.items);

  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;

    if (dep->path.items)
      free(dep->path.items);
    if (dep->imports.items)
      free(dep->imports.items);
    ir_destroy(&dep->ir);
    varss_destroy(&dep->varss);
  }
  if (ir->module_deps.items)
    free(ir->module_deps.items);
}
