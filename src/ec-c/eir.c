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
    return a->kind == b->kind &&
           (a->kind != ETypeKindStruct ||
            !a->_struct || !b->_struct ||
            a->_struct == b->_struct);
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

u32 get_type_size(EType *type) {
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

    for (u32 i = 0; i < type->_struct->fields.len; ++i)
      size += get_type_size(&type->_struct->fields.items[i].type);

    return size == 0 ? 1 : size;
  }

  case ETypeKindArray: {
    return get_type_size(type->array_element) * type->array_len;
  }

  case ETypeKindTuple: {
    u32 size = 0;

    for (u32 i = 0; i < type->tuple_types.len; ++i)
      size += get_type_size(type->tuple_types.items + i);

    return size;
  }

  case ETypeKindStr: return 8;
  case ETypeKindPtr: return 8;
  }

  return 0;
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
    if (!check_struct_existence(ir, &ir->consts.items[i].type, NULL, NULL))
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

    if (instr->kind == EInstrKindCast) {
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
    type_free(&ir->consts.items[i].type);
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
