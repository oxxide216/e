#include "evm-encoder.h"
#include "evm/ir.h"
#include "utils.h"

typedef Da(u8) Bytes;

extern Arch arch;

static u32 get_type_size(EType *type) {
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
    StructSizeCalc ssc = ssc_make(arch);
    for (u32 i = 0; i < type->_struct->fields.len; ++i)
      ssc_append_field(&ssc, get_type_size(&type->_struct->fields.items[i].type));
    u32 size = ssc_get_result(&ssc);
    return size == 0 ? 1 : size;
  }

  case ETypeKindArray: {
    return get_type_size(type->array_element) * type->array_len;
  }

  case ETypeKindTuple: {
    StructSizeCalc ssc = ssc_make(arch);
    for (u32 i = 0; i < type->tuple_types.len; ++i)
      ssc_append_field(&ssc, get_type_size(type->tuple_types.items + i));
    return ssc_get_result(&ssc);
  }

  case ETypeKindStr: return 8;
  case ETypeKindPtr: return 8;
  }

  return 0;
}

static void encode_str(FILE *stream, Str str) {
  fwrite(&str.len, sizeof(str.len), 1, stream);
  fwrite(str.ptr, 1, str.len, stream);
}

static ValueKind e_type_kind_to_evm_value_kind(ETypeKind kind) {
  switch (kind) {
  case ETypeKindUnit:   return 0;
  case ETypeKindS8:     return ValueKindSigned;
  case ETypeKindS16:    return ValueKindSigned;
  case ETypeKindS32:    return ValueKindSigned;
  case ETypeKindS64:    return ValueKindSigned;
  case ETypeKindU8:     return ValueKindUnsigned;
  case ETypeKindU16:    return ValueKindUnsigned;
  case ETypeKindU32:    return ValueKindUnsigned;
  case ETypeKindU64:    return ValueKindUnsigned;
  case ETypeKindBool:   return ValueKindUnsigned;
  case ETypeKindStruct: return ValueKindUnsigned;
  case ETypeKindArray:  return ValueKindUnsigned;
  case ETypeKindTuple:  return ValueKindUnsigned;
  case ETypeKindStr:    return ValueKindUnsigned;
  case ETypeKindPtr:    return ValueKindUnsigned;
  }

  return 0;
}

static Value e_value_to_evm_value(EValue *value) {
  switch (value->type.kind) {
  case ETypeKindUnit:   return (Value) {0};
  case ETypeKindS8:     return (Value) { ValueKindSigned, { ._signed = value->as._signed } };
  case ETypeKindS16:    return (Value) { ValueKindSigned, { ._signed = value->as._signed } };
  case ETypeKindS32:    return (Value) { ValueKindSigned, { ._signed = value->as._signed } };
  case ETypeKindS64:    return (Value) { ValueKindSigned, { ._signed = value->as._signed } };
  case ETypeKindU8:     return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindU16:    return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindU32:    return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindU64:    return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindBool:   return (Value) { ValueKindUnsigned, { ._unsigned = value->as._bool } };
  case ETypeKindStruct: return (Value) { ValueKindUnsigned, { ._unsigned = 0 } };
  case ETypeKindArray:  return (Value) { ValueKindUnsigned, { ._unsigned = 0 } };
  case ETypeKindTuple:  return (Value) { ValueKindUnsigned, { ._unsigned = 0 } };
  case ETypeKindStr:    return (Value) { ValueKindUnsigned, { ._unsigned = 0 } };
  case ETypeKindPtr:    return (Value) { ValueKindUnsigned, { ._unsigned = 0 } };
  }

  return (Value) {0};
}

static BinOpKind e_bin_op_kind_to_evm_bin_op_kin(EBinOpKind kind, ETypeKind type_kind) {
  (void) type_kind;

  switch (kind) {
  case EBinOpKindAdd:    return BinOpKindAddInt;
  case EBinOpKindSub:    return BinOpKindSubInt;
  case EBinOpKindMul:    return BinOpKindMulInt;
  case EBinOpKindDiv:    return BinOpKindDivInt;
  case EBinOpKindRem:    return BinOpKindRem;
  case EBinOpKindAnd:    return BinOpKindAnd;
  case EBinOpKindOr:     return BinOpKindOr;
  case EBinOpKindXor:    return BinOpKindXor;
  case EBinOpKindLShift: return BinOpKindLShift;
  case EBinOpKindRShift: return BinOpKindRShift;
  case EBinOpKindEq:     return BinOpKindEqInt;
  case EBinOpKindNe:     return BinOpKindNeInt;
  case EBinOpKindLs:     return BinOpKindLsInt;
  case EBinOpKindLe:     return BinOpKindLeInt;
  case EBinOpKindGt:     return BinOpKindGtInt;
  case EBinOpKindGe:     return BinOpKindGeInt;
  }

  return 0;
}

static InstrKind e_instr_kind_to_evm_instr_kind(EInstrKind kind) {
  switch (kind) {
  case EInstrKindAlloc:          return InstrKindAlloc;
  case EInstrKindStore:          return InstrKindStore;
  case EInstrKindCopy:           return InstrKindCopy;
  case EInstrKindBinOp:          return InstrKindBinOp;
  case EInstrKindCall:           return InstrKindCall;
  case EInstrKindCallAssign:     return InstrKindCallAssign;
  case EInstrKindRet:            return InstrKindRet;
  case EInstrKindRetVal:         return InstrKindRetVal;
  case EInstrKindJump:           return InstrKindJump;
  case EInstrKindJumpIf:         return InstrKindJumpIf;
  case EInstrKindJumpIfNot:      return InstrKindJumpIfNot;
  case EInstrKindRef:            return InstrKindRef;
  case EInstrKindCopyToRef:      return InstrKindCopyToRef;
  case EInstrKindCopyFromRef:    return InstrKindCopyFromRef;
  case EInstrKindStoreNull:      return InstrKindStore;
  case EInstrKindInlineAsm:      return InstrKindInlineAsm;
  case EInstrKindStoreStr:       return InstrKindStoreData;
  case EInstrKindCast:           return InstrKindConvert;
  case EInstrKindLenOf:          return InstrKindStore;
  case EInstrKindCopyToField:    return InstrKindCopyToRefFixed;
  case EInstrKindCopyFromField:  return InstrKindCopyFromRefFixed;
  case EInstrKindTuple:          return 0;
  case EInstrKindCopyToOffset:   return InstrKindCopyToRefFixed;
  case EInstrKindCopyFromOffset: return InstrKindCopyFromRefFixed;
  case EInstrKindSizeOf:         return InstrKindStore;
  case EInstrKindStaticRef:      return InstrKindStoreUninit;
  case EInstrKindCopyToStatic:   return InstrKindCopyToRefFixed;
  case EInstrKindCopyFromStatic: return InstrKindCopyFromRefFixed;
  case EInstrKindNot:            return InstrKindNot;
  }

  return 0;
}

static void sb_push_type_hash(StringBuilder *sb, EType *type) {
  sb_push_u32(sb, type->kind);
  if (type->kind == ETypeKindPtr) {
    sb_push_type_hash(sb, type->ptr_target);
  } else if (type->kind == ETypeKindArray) {
    sb_push_type_hash(sb, type->array_element);
  } else if (type->kind == ETypeKindStruct) {
    sb_push_u64(sb, str_hash(type->_struct->name));
  } else if (type->kind == ETypeKindTuple) {
    sb_push_u32(sb, type->tuple_types.len);
    for (u32 i = 0; i < type->tuple_types.len; ++i)
      sb_push_type_hash(sb, type->tuple_types.items + i);
  }
}

static Str mangle_proc_name(EProc *proc, EModulePath *path) {
  StringBuilder sb = {0};
  for (u32 i = 0; i < path->len; ++i) {
    sb_push_str(&sb, path->items[i]);
    sb_push_str(&sb, STR_LIT("??"));
  }
  sb_push_str(&sb, proc->name);
  sb_push_u32(&sb, proc->args.len);
  for (u32 i = 0; i < proc->args.len; ++i)
    sb_push_type_hash(&sb, &proc->args.items[i].type);
  return sb_to_str(sb);
}

static Str mangle_callee_name(Str name, Indices *arg_indices,
                              Vars *vars, EModulePath *path) {
  StringBuilder sb = {0};
  for (u32 i = 0; i < path->len; ++i) {
    sb_push_str(&sb, path->items[i]);
    sb_push_str(&sb, STR_LIT("??"));
  }
  u32 index;
  while ((index = str_find(name, STR_LIT("::"))) != (u32) -1) {
    name.ptr += index + 2;
    name.len -= index + 2;
  }
  sb_push_str(&sb, name);
  sb_push_u32(&sb, arg_indices->len);
  for (u32 i = 0; i < arg_indices->len; ++i) {
    Var *var = vars->items + arg_indices->items[i];
    sb_push_type_hash(&sb, &var->type);
  }
  return sb_to_str(sb);
}

static void encode_procs_no_len(FILE *stream, EIr *ir,
                                EModulePath *path,
                                Varss *varss,
                                u32 data_base,
                                u32 static_base) {
  for (u32 i = 0; i < ir->procs.len; ++i) {
    EProc *proc = ir->procs.items + i;

    if (proc->generics.len > 0)
      continue;

    if (str_eq(proc->name, STR_LIT("main"))) {
      encode_str(stream, proc->name);
    } else {
      Str mangled_name = mangle_proc_name(proc, path);
      encode_str(stream, mangled_name);
      free(mangled_name.ptr);
    }

    fwrite(&proc->args.len, sizeof(proc->args.len), 1, stream);
    for (u32 j = 0; j < proc->args.len; ++j) {
      EType *arg_type = &proc->args.items[j].type;
      ValueKind arg_kind = e_type_kind_to_evm_value_kind(arg_type->kind);
      fwrite(&arg_kind, 1, 1, stream);
      u32 size = get_type_size(arg_type);
      fwrite(&size, sizeof(size), 1, stream);
    }

    ValueKind return_kind = e_type_kind_to_evm_value_kind(proc->return_type.kind);
    fwrite(&return_kind, 1, 1, stream);
    u32 size = get_type_size(&proc->return_type);
    fwrite(&size, sizeof(size), 1, stream);

    u32 len_without_tuples = 0;
    for (u32 j = 0; j < proc->instrs.len; ++j)
      if (proc->instrs.items[j].kind != EInstrKindTuple)
        ++len_without_tuples;

    fwrite(&len_without_tuples, sizeof(len_without_tuples), 1, stream);

    for (u32 j = 0; j < proc->instrs.len; ++j) {
      EInstr *instr = proc->instrs.items + j;

      if (instr->kind == EInstrKindTuple)
        continue;

      InstrKind instr_kind = e_instr_kind_to_evm_instr_kind(instr->kind);
      fwrite(&instr_kind, 1, 1, stream);

      switch (instr->kind) {
      case EInstrKindAlloc: {
        fwrite(&instr->as.alloc.index, sizeof(instr->as.alloc.index), 1, stream);
        Var *var = varss->items[i].items + instr->as.alloc.index;
        u32 size = get_type_size(&var->type);
        fwrite(&size, sizeof(size), 1, stream);
      } break;

      case EInstrKindStore: {
        fwrite(&instr->as.store.index, sizeof(instr->as.store.index), 1, stream);
        Value value = e_value_to_evm_value(&instr->as.store.value);
        fwrite(&value.kind, 1, 1, stream);
        switch (value.kind) {
        case ValueKindSigned: {
          fwrite(&value.as._signed, sizeof(value.as._signed), 1, stream);
        } break;

        case ValueKindUnsigned: {
          fwrite(&value.as._unsigned, sizeof(value.as._unsigned), 1, stream);
        } break;
        }
      } break;

      case EInstrKindCopy: {
        fwrite(&instr->as.copy.dest_index, sizeof(instr->as.copy.dest_index), 1, stream);
        fwrite(&instr->as.copy.src_index, sizeof(instr->as.copy.src_index), 1, stream);
      } break;

      case EInstrKindBinOp: {
        fwrite(&instr->as.bin_op.dest_index, sizeof(instr->as.bin_op.dest_index), 1, stream);
        fwrite(&instr->as.bin_op.src0_index, sizeof(instr->as.bin_op.src0_index), 1, stream);
        fwrite(&instr->as.bin_op.src1_index, sizeof(instr->as.bin_op.src1_index), 1, stream);
        Var *dest = varss->items[i].items + instr->as.bin_op.dest_index;
        BinOpKind kind = e_bin_op_kind_to_evm_bin_op_kin(instr->as.bin_op.kind, dest->type.kind);
        fwrite(&kind, 1, 1, stream);
      } break;

      case EInstrKindCall: {
        if (str_eq(instr->as.call.name, STR_LIT("main"))) {
          encode_str(stream, instr->as.call.name);
        } else {
          EModulePath temp_path =
            get_proc_path(ir, instr->as.call.name,
                          &instr->as.call.arg_indices,
                          varss->items + i, path,
                          str_eq(instr->as.call.name, STR_LIT("drop")));
          Str mangled_name =
            mangle_callee_name(instr->as.call.name,
                               &instr->as.call.arg_indices,
                               varss->items + i, &temp_path);
          encode_str(stream, mangled_name);
          free(mangled_name.ptr);
          if (temp_path.items != path->items)
            free(temp_path.items);
        }

        fwrite(&instr->as.call.arg_indices.len, sizeof(instr->as.call.arg_indices.len), 1, stream);
        for (u32 k = 0; k < instr->as.call.arg_indices.len; ++k) {
          u32 arg_index = instr->as.call.arg_indices.items[k];
          fwrite(&arg_index, sizeof(arg_index), 1, stream);
        }
      } break;

      case EInstrKindCallAssign: {
        fwrite(&instr->as.call_assign.dest_index, sizeof(instr->as.call_assign.dest_index), 1, stream);
        EType *return_type = &instr->as.call_assign.callee->return_type;
        ValueKind kind = e_type_kind_to_evm_value_kind(return_type->kind);
        fwrite(&kind, 1, 1, stream);

        if (str_eq(instr->as.call_assign.name, STR_LIT("main"))) {
          encode_str(stream, instr->as.call_assign.name);
        } else {
          EModulePath temp_path =
            get_proc_path(ir, instr->as.call_assign.name,
                          &instr->as.call_assign.arg_indices,
                          varss->items + i, path, false);
          Str mangled_name =
            mangle_callee_name(instr->as.call_assign.name,
                               &instr->as.call_assign.arg_indices,
                               varss->items + i, &temp_path);
          encode_str(stream, mangled_name);
          free(mangled_name.ptr);
          if (temp_path.items != path->items)
            free(temp_path.items);
        }

        fwrite(&instr->as.call_assign.arg_indices.len, sizeof(instr->as.call_assign.arg_indices.len), 1, stream);
        for (u32 k = 0; k < instr->as.call_assign.arg_indices.len; ++k) {
          u32 arg_index = instr->as.call_assign.arg_indices.items[k];
          fwrite(&arg_index, sizeof(arg_index), 1, stream);
        }
      } break;

      case EInstrKindRet: break;

      case EInstrKindRetVal: {
        fwrite(&instr->as.ret_val.index, sizeof(instr->as.ret_val.index), 1, stream);
      } break;

      case EInstrKindJump: {
        fwrite(&instr->as.jump.target, sizeof(instr->as.jump.target), 1, stream);
      } break;

      case EInstrKindJumpIf: {
        fwrite(&instr->as.jump_if.cond_index, sizeof(instr->as.jump_if.cond_index), 1, stream);
        fwrite(&instr->as.jump_if.target, sizeof(instr->as.jump_if.target), 1, stream);
      } break;

      case EInstrKindJumpIfNot: {
        fwrite(&instr->as.jump_if_not.cond_index, sizeof(instr->as.jump_if_not.cond_index), 1, stream);
        fwrite(&instr->as.jump_if_not.target, sizeof(instr->as.jump_if_not.target), 1, stream);
      } break;

      case EInstrKindRef: {
        fwrite(&instr->as.ref.dest_index, sizeof(instr->as.ref.dest_index), 1, stream);
        fwrite(&instr->as.ref.src_index, sizeof(instr->as.ref.src_index), 1, stream);
      } break;

      case EInstrKindCopyToRef: {
        fwrite(&instr->as.copy_to_ref.dest_index, sizeof(instr->as.copy_to_ref.dest_index), 1, stream);
        fwrite(&instr->as.copy_to_ref.has_offset, 1, 1, stream);
        if (instr->as.copy_to_ref.has_offset)
          fwrite(&instr->as.copy_to_ref.dest_offset_index, sizeof(instr->as.copy_to_ref.dest_offset_index), 1, stream);
        fwrite(&instr->as.copy_to_ref.src_index, sizeof(instr->as.copy_to_ref.src_index), 1, stream);

        Var *dest = varss->items[i].items + instr->as.copy_to_ref.dest_index;
        bool deref = dest->type.kind == ETypeKindPtr || dest->type.is_implicit_ptr;
        fwrite(&deref, 1, 1, stream);
      } break;

      case EInstrKindCopyFromRef: {
        fwrite(&instr->as.copy_from_ref.dest_index, sizeof(instr->as.copy_from_ref.dest_index), 1, stream);
        fwrite(&instr->as.copy_from_ref.src_index, sizeof(instr->as.copy_from_ref.src_index), 1, stream);
        fwrite(&instr->as.copy_from_ref.has_offset, 1, 1, stream);
        if (instr->as.copy_from_ref.has_offset)
          fwrite(&instr->as.copy_from_ref.src_offset_index, sizeof(instr->as.copy_from_ref.src_offset_index), 1, stream);

        Var *src = varss->items[i].items + instr->as.copy_from_ref.src_index;

        ValueKind src_target_kind = e_type_kind_to_evm_value_kind(src->type.ptr_target->kind);
        fwrite(&src_target_kind, 1, 1, stream);

        bool deref = src->type.kind == ETypeKindPtr || src->type.is_implicit_ptr;
        fwrite(&deref, 1, 1, stream);
        fwrite(&instr->as.copy_from_ref.take_ref, 1, 1, stream);
      } break;

      case EInstrKindStoreNull: {
        fwrite(&instr->as.store_null.index, sizeof(instr->as.store_null.index), 1, stream);
        Value value = {
          ValueKindUnsigned,
          { ._unsigned = 0 },
        };
        fwrite(&value.kind, 1, 1, stream);
        fwrite(&value.as._unsigned, sizeof(value.as._unsigned), 1, stream);
      } break;

      case EInstrKindInlineAsm: {
        fwrite(&instr->as.inline_asm.segments.len, sizeof(instr->as.inline_asm.segments.len), 1, stream);
        for (u32 k = 0; k < instr->as.inline_asm.segments.len; ++k) {
          EAsmSegment *segment = instr->as.inline_asm.segments.items + k;
          fwrite(&segment->kind, 1, 1, stream);
          if (segment->kind == EAsmSegmentKindStr)
            encode_str(stream, segment->value);
          else
            fwrite(&segment->value_index, sizeof(segment->value_index), 1, stream);
        }
      } break;

      case EInstrKindStoreStr: {
        u32 data_index = instr->as.store_str.data_index + data_base;
        fwrite(&instr->as.store_str.index, sizeof(instr->as.store_str.index), 1, stream);
        fwrite(&data_index, sizeof(data_index), 1, stream);
      } break;

      case EInstrKindCast: {
        fwrite(&instr->as.cast.dest_index, sizeof(instr->as.cast.dest_index), 1, stream);
        ValueKind dest_kind = e_type_kind_to_evm_value_kind(instr->as.cast.dest_type.kind);
        fwrite(&dest_kind, 1, 1, stream);
        u32 size = get_type_size(&instr->as.cast.dest_type);
        fwrite(&size, sizeof(size), 1, stream);
        fwrite(&instr->as.cast.src_index, sizeof(instr->as.cast.src_index), 1, stream);
      } break;

      case EInstrKindLenOf: {
        fwrite(&instr->as.len_of.dest_index, sizeof(instr->as.len_of.dest_index), 1, stream);

        Var *src = varss->items[i].items + instr->as.len_of.src_index;
        Value value = {
          ValueKindUnsigned,
          { ._unsigned = src->type.array_len },
        };
        fwrite(&value.kind, 1, 1, stream);
        fwrite(&value.as._unsigned, sizeof(value.as._unsigned), 1, stream);
      } break;

      case EInstrKindCopyToField: {
        fwrite(&instr->as.copy_to_field.dest_index, sizeof(instr->as.copy_to_field.dest_index), 1, stream);

        Var *dest = varss->items[i].items + instr->as.copy_to_field.dest_index;
        EType *dest_type = &dest->type;
        if (dest_type->kind == ETypeKindPtr)
          dest_type = dest_type->ptr_target;

        StructSizeCalc ssc = ssc_make(arch);
        EStruct *_struct = dest_type->_struct;
        u32 index = 0;

        while (!str_eq(_struct->fields.items[index].name, instr->as.copy_to_field.dest_field_name))
          ssc_append_field(&ssc, get_type_size(&_struct->fields.items[index++].type));

        i32 offset = ssc_append_field(&ssc, get_type_size(&_struct->fields.items[index].type));
        fwrite(&offset, sizeof(offset), 1, stream);

        fwrite(&instr->as.copy_to_field.src_index, sizeof(instr->as.copy_to_field.src_index), 1, stream);

        bool deref = dest->type.kind == ETypeKindStr ||
                     dest->type.kind == ETypeKindPtr ||
                     dest->type.is_implicit_ptr;
        fwrite(&deref, 1, 1, stream);
      } break;

      case EInstrKindCopyFromField: {
        fwrite(&instr->as.copy_from_field.dest_index, sizeof(instr->as.copy_from_field.dest_index), 1, stream);
        fwrite(&instr->as.copy_from_field.src_index, sizeof(instr->as.copy_from_field.src_index), 1, stream);

        Var *src = varss->items[i].items + instr->as.copy_from_field.src_index;
        EType *src_type = &src->type;
        if (src_type->kind == ETypeKindPtr)
          src_type = src_type->ptr_target;

        StructSizeCalc ssc = ssc_make(arch);
        EStruct *_struct = src_type->_struct;
        u32 index = 0;

        while (!str_eq(_struct->fields.items[index].name, instr->as.copy_from_field.src_field_name))
          ssc_append_field(&ssc, get_type_size(&_struct->fields.items[index++].type));

        i32 offset = ssc_append_field(&ssc, get_type_size(&_struct->fields.items[index].type));
        fwrite(&offset, sizeof(offset), 1, stream);

        EField *field = _struct->fields.items + index;
        ValueKind src_target_kind = e_type_kind_to_evm_value_kind(field->type.kind);
        fwrite(&src_target_kind, 1, 1, stream);

        bool deref = src->type.kind == ETypeKindStr ||
                     src->type.kind == ETypeKindPtr ||
                     src->type.is_implicit_ptr;
        fwrite(&deref, 1, 1, stream);

        fwrite(&instr->as.copy_from_field.take_ref, 1, 1, stream);
      } break;

      // Unreachable, skipped above
      case EInstrKindTuple: break;

      case EInstrKindCopyToOffset: {
        fwrite(&instr->as.copy_to_offset.dest_index, sizeof(instr->as.copy_to_offset.dest_index), 1, stream);

        Var *dest = varss->items[i].items + instr->as.copy_to_offset.dest_index;

        EType *dest_type = &dest->type;
        if (dest_type->kind == ETypeKindPtr)
          dest_type = dest_type->ptr_target;

        if (dest_type->kind == ETypeKindStr) {
          i32 offset = instr->as.copy_to_offset.dest_offset;
          fwrite(&offset, sizeof(offset), 1, stream);
        } else if (dest_type->kind == ETypeKindTuple) {
          StructSizeCalc ssc = ssc_make(arch);

          for (u32 k = 0; k < instr->as.copy_to_offset.dest_offset; ++k)
            ssc_append_field(&ssc, get_type_size(&dest_type->tuple_types.items[k]));

          i32 offset = ssc_append_field(&ssc, get_type_size(&dest_type->tuple_types.items[instr->as.copy_to_offset.dest_offset]));
          fwrite(&offset, sizeof(offset), 1, stream);
        }

        fwrite(&instr->as.copy_to_offset.src_index, sizeof(instr->as.copy_to_offset.src_index), 1, stream);

        bool deref = dest_type->kind == ETypeKindStr ||
                     dest_type->kind == ETypeKindPtr ||
                     dest_type->is_implicit_ptr;
        fwrite(&deref, 1, 1, stream);
      } break;

      case EInstrKindCopyFromOffset: {
        fwrite(&instr->as.copy_from_offset.dest_index, sizeof(instr->as.copy_from_offset.dest_index), 1, stream);
        fwrite(&instr->as.copy_from_offset.src_index, sizeof(instr->as.copy_from_offset.src_index), 1, stream);

        Var *src = varss->items[i].items + instr->as.copy_from_offset.src_index;

        EType *src_type = &src->type;
        if (src_type->kind == ETypeKindPtr)
          src_type = src_type->ptr_target;

        if (src_type->kind == ETypeKindStr) {
          i32 offset = instr->as.copy_from_offset.src_offset;
          fwrite(&offset, sizeof(offset), 1, stream);
          ValueKind src_target_kind = ValueKindUnsigned;
          fwrite(&src_target_kind, 1, 1, stream);
        } else {
          StructSizeCalc ssc = ssc_make(arch);

          for (u32 k = 0; k < instr->as.copy_from_offset.src_offset; ++k)
            ssc_append_field(&ssc, get_type_size(&src_type->tuple_types.items[k]));

          i32 offset = ssc_append_field(&ssc, get_type_size(&src_type->tuple_types.items[instr->as.copy_from_offset.src_offset]));
          fwrite(&offset, sizeof(offset), 1, stream);

          ValueKind src_target_kind = ValueKindUnsigned;
          if (src_type->kind == ETypeKindStruct) {
            EStruct *_struct = src_type->_struct;
            EField *field = _struct->fields.items + instr->as.copy_from_offset.src_offset;
            src_target_kind = e_type_kind_to_evm_value_kind(field->type.kind);
          } else if (src_type->kind == ETypeKindTuple) {
            EType *field_type = src_type->tuple_types.items + instr->as.copy_from_offset.src_offset;
            src_target_kind = e_type_kind_to_evm_value_kind(field_type->kind);
          }

          fwrite(&src_target_kind, 1, 1, stream);
        }

        bool deref = instr->as.copy_from_offset.deref ||
                     src_type->kind == ETypeKindStr ||
                     src_type->kind == ETypeKindPtr ||
                     src_type->is_implicit_ptr;
        fwrite(&deref, 1, 1, stream);

        fwrite(&instr->as.copy_from_offset.take_ref, 1, 1, stream);
      } break;

      case EInstrKindSizeOf: {
        fwrite(&instr->as.size_of.dest_index, sizeof(instr->as.size_of.dest_index), 1, stream);
        ValueKind kind = ValueKindUnsigned;
        fwrite(&kind, 1, 1, stream);
        u64 size = get_type_size(&instr->as.size_of.type);
        fwrite(&size, sizeof(size), 1, stream);
      } break;

      case EInstrKindStaticRef: {
        u32 static_index = instr->as.static_ref.static_index + static_base;
        fwrite(&instr->as.static_ref.dest_index, sizeof(instr->as.static_ref.dest_index), 1, stream);
        fwrite(&static_index, sizeof(static_index), 1, stream);
      } break;

      case EInstrKindCopyToStatic: {
        fwrite(&instr->as.copy_to_static.dest_index, sizeof(instr->as.copy_to_static.dest_index), 1, stream);

        Var *src = varss->items[i].items + instr->as.copy_to_static.src_index;

        u32 size = get_type_size(&src->type);
        fwrite(&size, sizeof(size), 1, stream);

        fwrite(&instr->as.copy_to_static.src_index, sizeof(instr->as.copy_to_static.src_index), 1, stream);

        bool deref = true;
        fwrite(&deref, 1, 1, stream);
      } break;

      case EInstrKindCopyFromStatic: {
        fwrite(&instr->as.copy_from_static.dest_index, sizeof(instr->as.copy_from_static.dest_index), 1, stream);
        fwrite(&instr->as.copy_from_static.src_index, sizeof(instr->as.copy_from_static.src_index), 1, stream);

        Var *dest = varss->items[i].items + instr->as.copy_from_static.dest_index;    

        u32 size = get_type_size(&dest->type);
        fwrite(&size, sizeof(size), 1, stream);

        ValueKind src_target_kind = e_type_kind_to_evm_value_kind(dest->type.kind);
        fwrite(&src_target_kind, 1, 1, stream);

        bool deref = true;
        fwrite(&deref, 1, 1, stream);

        bool take_ref = false;
        fwrite(&take_ref, 1, 1, stream);
       } break;

      case EInstrKindNot: {
        fwrite(&instr->as.not.dest_index, sizeof(instr->as.not.dest_index), 1, stream);
        fwrite(&instr->as.not.src_index, sizeof(instr->as.not.src_index), 1, stream);
      } break;
      }
    }

    u32 zero = 0;
    fwrite(&zero, sizeof(zero), 1, stream);
  }
}

static void encode_data_no_len(FILE *stream, EData *data) {
  for (u32 i = 0; i < data->len; ++i) {
    EDataEntry *entry = data->items + i;

    fwrite(&entry->len, sizeof(entry->len), 1, stream);
    fwrite(entry->data, 1, entry->len, stream);
  }
}

static void encode_static_vars_no_len(FILE *stream, EIr *ir) {
  for (u32 i = 0; i < ir->static_vars.len; ++i) {
    EStaticVar *var = ir->static_vars.items + i;
    u32 size = get_type_size(&var->type);
    fwrite(&size, sizeof(size), 1, stream);
  }
}

static void encode_procs_as_imports_no_len(FILE *stream,
                                           EProcs *procs,
                                           EModulePath *path) {
  for (u32 i = 0; i < procs->len; ++i) {
    Str mangled_name = mangle_proc_name(procs->items + i, path);
    encode_str(stream, mangled_name);
    free(mangled_name.ptr);
  }
}

static void traverse_deps(EIr *ir,
                          u32 *procs_len,
                          u32 *data_len,
                          u32 *static_vars_len,
                          u32 *procs_as_imports_len) {
  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;

    if (dep->ir.is_copy)
      continue;

    for (u32 j = 0; j < dep->ir.procs.len; ++j) {
      if (dep->ir.procs.items[j].generics.len == 0) {
        if (!dep->ir.is_from_cache) {
          if (procs_len)
            ++*procs_len;
        } else if (procs_as_imports_len) {
          ++*procs_as_imports_len;
        }
      }
    }

    if (!dep->ir.is_from_cache) {
      if (data_len)
        *data_len += dep->ir.data.len;
      if (static_vars_len)
        *static_vars_len += dep->ir.static_vars.len;
    }
    traverse_deps(&dep->ir, procs_len, data_len,
                  static_vars_len, procs_as_imports_len);
  }
}

static void traverse_all_deps(EIr *ir,
                              u32 *procs_len,
                              u32 *data_len,
                              u32 *static_vars_len,
                              u32 *procs_as_imports_len) {
  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;

    for (u32 j = 0; j < dep->ir.procs.len; ++j) {
      if (dep->ir.procs.items[j].generics.len == 0) {
        if (!dep->ir.is_from_cache) {
          if (procs_len)
            ++*procs_len;
        }
        if (procs_as_imports_len) {
          ++*procs_as_imports_len;
        }
      }
    }

    if (!dep->ir.is_from_cache) {
      if (data_len)
        *data_len += dep->ir.data.len;
      if (static_vars_len)
        *static_vars_len += dep->ir.static_vars.len;
    }
    traverse_all_deps(&dep->ir, procs_len, data_len,
                      static_vars_len, procs_as_imports_len);
  }
}

static void encode_deps_procs(FILE *stream, EIr *ir,
                              EModulePath *path,
                              u32 *data_base,
                              u32 *static_base) {
  EModulePath temp_path = *path;
  if (temp_path.len > 0)
    --temp_path.len;

  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;
    if (dep->ir.is_copy || dep->ir.is_from_cache)
      continue;

    EModulePath new_path;
    if (dep->is_submodule)
      new_path = merge_module_paths(&temp_path, &dep->path);
    else
      new_path = dep->path;

    encode_procs_no_len(stream, &dep->ir, &new_path,
                        &dep->varss, *data_base, *static_base);
    *data_base += dep->ir.data.len;
    *static_base += dep->ir.static_vars.len;
    encode_deps_procs(stream, &dep->ir, &new_path, data_base, static_base);

    if (dep->is_submodule)
      free(new_path.items);
  }
}

static void encode_deps_data(FILE *stream, EIr *ir) {
  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;
    if (dep->ir.is_copy || dep->ir.is_from_cache)
      continue;

    encode_data_no_len(stream, &dep->ir.data);
    encode_deps_data(stream, &dep->ir);
  }
}

static void encode_deps_static_vars(FILE *stream, EIr *ir) {
  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;
    if (dep->ir.is_copy || dep->ir.is_from_cache)
      continue;

    encode_static_vars_no_len(stream, &dep->ir);
    encode_deps_static_vars(stream, &dep->ir);
  }
}

static void encode_all_deps_procs_as_imports(FILE *stream, EIr *ir,
                                             EModulePath *path) {
  EModulePath temp_path = *path;
  if (temp_path.len > 0)
    --temp_path.len;

  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;

    EModulePath new_path;
    if (dep->is_submodule)
      new_path = merge_module_paths(&temp_path, &dep->path);
    else
      new_path = dep->path;

    encode_procs_as_imports_no_len(stream, &dep->ir.procs, &new_path);
    encode_all_deps_procs_as_imports(stream, &dep->ir, &new_path);

    if (dep->is_submodule)
      free(new_path.items);
  }
}

void encode_ir_as_evm_ir(FILE *stream, EIr *ir, Varss *varss,
                         EModulePath *path,
                         bool include_deps, bool import_deps) {
  u32 procs_len = ir->procs.len;
  u32 data_len = ir->data.len;
  u32 static_vars_len = ir->static_vars.len;
  if (include_deps)
    traverse_deps(ir, &procs_len, &data_len, &static_vars_len, NULL);

  fwrite(&procs_len, sizeof(procs_len), 1, stream);
  encode_procs_no_len(stream, ir, path, varss, 0, 0);
  if (include_deps) {
    u32 data_base = ir->data.len;
    u32 static_base = ir->static_vars.len;
    encode_deps_procs(stream, ir, path, &data_base, &static_base);
  }

  fwrite(&data_len, sizeof(data_len), 1, stream);
  encode_data_no_len(stream, &ir->data);
  if (include_deps)
    encode_deps_data(stream, ir);

  fwrite(&static_vars_len, sizeof(static_vars_len), 1, stream);
  encode_static_vars_no_len(stream, ir);
  if (include_deps)
    encode_deps_static_vars(stream, ir);

  u32 imports_len = 0;
  if (import_deps)
    traverse_all_deps(ir, NULL, NULL, NULL, &imports_len);
  fwrite(&imports_len, sizeof(imports_len), 1, stream);
  if (import_deps)
    encode_all_deps_procs_as_imports(stream, ir, path);
}

static void _value_to_bytes(Bytes *bytes, EValue *value) {
  switch (value->type.kind) {
  case ETypeKindUnit: break;

  case ETypeKindS8: {
    i8 s = value->as._signed;
    DA_APPEND(*bytes, s);
  } break;

  case ETypeKindS16: {
    i16 s = value->as._signed;
    DA_APPEND(*bytes, ((i8 *) &s)[0]);
    DA_APPEND(*bytes, ((i8 *) &s)[1]);
  } break;

  case ETypeKindS32: {
    i32 s = value->as._signed;
    DA_APPEND(*bytes, ((i8 *) &s)[0]);
    DA_APPEND(*bytes, ((i8 *) &s)[1]);
    DA_APPEND(*bytes, ((i8 *) &s)[2]);
    DA_APPEND(*bytes, ((i8 *) &s)[3]);
  } break;

  case ETypeKindS64: {
    i64 s = value->as._signed;
    DA_APPEND(*bytes, ((i8 *) &s)[0]);
    DA_APPEND(*bytes, ((i8 *) &s)[1]);
    DA_APPEND(*bytes, ((i8 *) &s)[2]);
    DA_APPEND(*bytes, ((i8 *) &s)[3]);
    DA_APPEND(*bytes, ((i8 *) &s)[4]);
    DA_APPEND(*bytes, ((i8 *) &s)[5]);
    DA_APPEND(*bytes, ((i8 *) &s)[6]);
    DA_APPEND(*bytes, ((i8 *) &s)[7]);
  } break;

  case ETypeKindU8: {
    u8 u = value->as._unsigned;
    DA_APPEND(*bytes, u);
  } break;

  case ETypeKindU16: {
    u16 u = value->as._unsigned;
    DA_APPEND(*bytes, ((u8 *) &u)[0]);
    DA_APPEND(*bytes, ((u8 *) &u)[1]);
  } break;

  case ETypeKindU32: {
    u32 u = value->as._unsigned;
    DA_APPEND(*bytes, ((u8 *) &u)[0]);
    DA_APPEND(*bytes, ((u8 *) &u)[1]);
    DA_APPEND(*bytes, ((u8 *) &u)[2]);
    DA_APPEND(*bytes, ((u8 *) &u)[3]);
  } break;

  case ETypeKindU64: {
    u64 u = value->as._unsigned;
    DA_APPEND(*bytes, ((u8 *) &u)[0]);
    DA_APPEND(*bytes, ((u8 *) &u)[1]);
    DA_APPEND(*bytes, ((u8 *) &u)[2]);
    DA_APPEND(*bytes, ((u8 *) &u)[3]);
    DA_APPEND(*bytes, ((u8 *) &u)[4]);
    DA_APPEND(*bytes, ((u8 *) &u)[5]);
    DA_APPEND(*bytes, ((u8 *) &u)[6]);
    DA_APPEND(*bytes, ((u8 *) &u)[7]);
  } break;

  case ETypeKindBool: {
    u8 b = value->as._bool;
    DA_APPEND(*bytes, b);
  } break;

  case ETypeKindStruct: {
    StructSizeCalc ssc = ssc_make(arch);
    u32 len = bytes->len;

    for (u32 i = 0; i < value->type._struct->fields.len; ++i) {
      u32 size = get_type_size(&value->type._struct->fields.items[i].type);
      u32 offset = ssc_append_field(&ssc, size);
      for (u32 j = bytes->len; j < len + offset; ++j)
        DA_APPEND(*bytes, 0);

      EValue *field_value = NULL;

      for (u32 j = 0; j < value->as.struct_fields.len; ++j) {
        if (str_eq(value->as.struct_fields.items[j].name,
                   value->type._struct->fields.items[i].name)) {
          field_value = value->as.struct_fields.items[j].value;
          break;
        }
      }

      if (field_value)
        _value_to_bytes(bytes, field_value);
      else
        for (u32 i = bytes->len; i < len + offset + size; ++i)
          DA_APPEND(*bytes, 0);
    }

    u32 size = ssc_get_result(&ssc);
    for (u32 i = bytes->len; i < len + size; ++i)
      DA_APPEND(*bytes, 0);

    if (size == 0)
      DA_APPEND(*bytes, 0);
  } break;

  case ETypeKindArray: {
    for (u32 i = 0; i < value->as.array_elements.len; ++i)
      _value_to_bytes(bytes, value->as.array_elements.items + i);
  } break;

  case ETypeKindTuple: {
    StructSizeCalc ssc = ssc_make(arch);
    u32 len = bytes->len;

    for (u32 i = 0; i < value->as.tuple_fields.len; ++i) {
      u32 size = get_type_size(&value->as.tuple_fields.items[i].type);
      u32 offset = ssc_append_field(&ssc, size);
      for (u32 j = bytes->len; j < len + offset; ++j)
        DA_APPEND(*bytes, 0);

      _value_to_bytes(bytes, value->as.tuple_fields.items + i);
    }

    u32 size = ssc_get_result(&ssc);
    for (u32 i = bytes->len; i < len + size; ++i)
      DA_APPEND(*bytes, 0);
  } break;

  case ETypeKindStr: break;
  case ETypeKindPtr: break;
  }
}

u8 *value_to_bytes(EValue *value, u32 *len) {
  Bytes bytes = {0};
  _value_to_bytes(&bytes, value);
  *len = bytes.len;
  return bytes.items;
}
