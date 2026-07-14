#include "evm-encoder.h"
#include "ir.h"

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
  case ETypeKindPtr:    return ValueKindUnsigned;
  }

  return 0;
}

static Value e_value_to_evm_value(EValue *value) {
  switch (value->kind) {
  case ETypeKindUnit:   return (Value) {0};
  case ETypeKindS8:     return (Value) { ValueKindSigned, { ._signed = value->as._signed } };
  case ETypeKindS16:    return (Value) { ValueKindSigned, { ._signed = value->as._signed } };
  case ETypeKindS32:    return (Value) { ValueKindSigned, { ._signed = value->as._signed } };
  case ETypeKindS64:    return (Value) { ValueKindSigned, { ._signed = value->as._signed } };
  case ETypeKindU8:     return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindU16:    return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindU32:    return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindU64:    return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindBool:   return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindStruct: return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindArray:  return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
  case ETypeKindPtr:    return (Value) { ValueKindUnsigned, { ._unsigned = value->as._unsigned } };
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
  case EInstrKindAlloc:       return InstrKindAlloc;
  case EInstrKindStore:       return InstrKindStore;
  case EInstrKindCopy:        return InstrKindCopy;
  case EInstrKindBinOp:       return InstrKindBinOp;
  case EInstrKindCall:        return InstrKindCall;
  case EInstrKindCallAssign:  return InstrKindCallAssign;
  case EInstrKindRet:         return InstrKindRet;
  case EInstrKindRetVal:      return InstrKindRetVal;
  case EInstrKindJump:        return InstrKindJump;
  case EInstrKindJumpIfNot:   return InstrKindJumpIfNot;
  case EInstrKindRef:         return InstrKindRef;
  case EInstrKindCopyToRef:   return InstrKindCopyToRef;
  case EInstrKindCopyFromRef: return InstrKindCopyFromRef;
  case EInstrKindStoreNull:   return InstrKindStore;
  case EInstrKindInlineAsm:   return InstrKindInlineAsm;
  case EInstrKindStoreData:   return InstrKindStoreData;
  case EInstrKindCast:        return InstrKindConvert;
  case EInstrKindLenOf:       return InstrKindStore;
  }

  return 0;
}

static void sb_push_type_hash(StringBuilder *sb, EType *type) {
  sb_push_u32(sb, type->kind);
  if (type->kind == ETypeKindPtr)
    sb_push_type_hash(sb, type->ptr_target);
  else if (type->kind == ETypeKindArray)
    sb_push_type_hash(sb, type->array_element);
  else if (type->kind == ETypeKindStruct)
    sb_push_u64(sb, str_hash(type->name));
}

static Str mangle_proc_name(EProc *proc) {
  StringBuilder sb = {0};
  sb_push_str(&sb, proc->name);
  sb_push_u32(&sb, proc->args.len);
  for (u32 i = 0; i < proc->args.len; ++i)
    sb_push_type_hash(&sb, &proc->args.items[i].type);
  return sb_to_str(sb);
}

static Str mangle_callee_name(Str name, Indices *arg_indices, Vars *vars) {
  StringBuilder sb = {0};
  sb_push_str(&sb, name);
  sb_push_u32(&sb, arg_indices->len);
  for (u32 i = 0; i < arg_indices->len; ++i) {
    Var *var = vars->items + arg_indices->items[i];
    sb_push_type_hash(&sb, &var->type);
  }
  return sb_to_str(sb);
}

static void encode_procs_no_len(FILE *stream, EProcs *procs,
                                EStructs *structs, Varss *varss,
                                u32 data_base) {
  for (u32 i = 0; i < procs->len; ++i) {
    EProc *proc = procs->items + i;

    if (str_eq(proc->name, STR_LIT("main"))) {
      encode_str(stream, proc->name);
    } else {
      Str mangled_name = mangle_proc_name(proc);
      encode_str(stream, mangled_name);
      free(mangled_name.ptr);
    }

    fwrite(&proc->args.len, sizeof(proc->args.len), 1, stream);
    for (u32 j = 0; j < proc->args.len; ++j) {
      u32 arg_size = get_type_size(structs, &proc->args.items[j].type);
      ValueKind arg_kind = e_type_kind_to_evm_value_kind(proc->args.items[j].type.kind);
      fwrite(&arg_size, sizeof(arg_size), 1, stream);
      fwrite(&arg_kind, 1, 1, stream);
    }

    u32 return_size = get_type_size(structs, &proc->return_type);
    ValueKind return_kind = e_type_kind_to_evm_value_kind(proc->return_type.kind);
    fwrite(&return_size, sizeof(return_size), 1, stream);
    fwrite(&return_kind, 1, 1, stream);

    fwrite(&proc->instrs.len, sizeof(proc->instrs.len), 1, stream);

    for (u32 j = 0; j < proc->instrs.len; ++j) {
      EInstr *instr = proc->instrs.items + j;

      InstrKind instr_kind = e_instr_kind_to_evm_instr_kind(instr->kind);
      fwrite(&instr_kind, 1, 1, stream);

      switch (instr->kind) {
      case EInstrKindAlloc: {
        fwrite(&instr->as.alloc.index, sizeof(instr->as.alloc.index), 1, stream);
        u32 size = get_type_size(structs, &varss->items[i].items[instr->as.alloc.index].type);
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
        Str mangled_name = mangle_callee_name(instr->as.call.name, &instr->as.call.arg_indices, varss->items + i);
        encode_str(stream, mangled_name);
        free(mangled_name.ptr);

        fwrite(&instr->as.call.arg_indices.len, sizeof(instr->as.call.arg_indices.len), 1, stream);
        for (u32 k = 0; k < instr->as.call.arg_indices.len; ++k) {
          u32 arg_index = instr->as.call.arg_indices.items[k];
          fwrite(&arg_index, sizeof(arg_index), 1, stream);
        }
      } break;

      case EInstrKindCallAssign: {
        fwrite(&instr->as.call_assign.dest_index, sizeof(instr->as.call_assign.dest_index), 1, stream);

        Str mangled_name = mangle_callee_name(instr->as.call_assign.name, &instr->as.call_assign.arg_indices, varss->items + i);
        encode_str(stream, mangled_name);
        free(mangled_name.ptr);

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
      } break;

      case EInstrKindCopyFromRef: {
        fwrite(&instr->as.copy_from_ref.dest_index, sizeof(instr->as.copy_from_ref.dest_index), 1, stream);
        fwrite(&instr->as.copy_from_ref.src_index, sizeof(instr->as.copy_from_ref.src_index), 1, stream);
        fwrite(&instr->as.copy_from_ref.has_offset, 1, 1, stream);
        if (instr->as.copy_from_ref.has_offset)
          fwrite(&instr->as.copy_from_ref.src_offset_index, sizeof(instr->as.copy_from_ref.src_offset_index), 1, stream);

        Var *src = varss->items[i].items + instr->as.copy_from_ref.src_index;
        ValueKind src_target_kind = e_type_kind_to_evm_value_kind(src->type.ptr_target->kind);
        u32 src_target_size = get_type_size(structs, src->type.ptr_target);
        fwrite(&src_target_kind, 1, 1, stream);
        fwrite(&src_target_size, sizeof(src_target_size), 1, stream);
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

      case EInstrKindStoreData: {
        u32 data_index = instr->as.store_data.data_index + data_base;
        fwrite(&instr->as.store_data.index, sizeof(instr->as.store_data.index), 1, stream);
        fwrite(&data_index, sizeof(data_index), 1, stream);
      } break;

      case EInstrKindCast: {
        fwrite(&instr->as.cast.dest_index, sizeof(instr->as.cast.dest_index), 1, stream);

        ValueKind dest__kind = e_type_kind_to_evm_value_kind(instr->as.cast.dest_type.kind);
        u32 dest_size = get_type_size(structs, &instr->as.cast.dest_type);
        fwrite(&dest__kind, 1, 1, stream);
        fwrite(&dest_size, sizeof(dest_size), 1, stream);

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
      }
    }
  }
}

static void encode_data_no_len(FILE *stream, EData *data) {
  for (u32 i = 0; i < data->len; ++i) {
    EDataEntry *entry = data->items + i;

    fwrite(&entry->len, sizeof(entry->len), 1, stream);
    fwrite(entry->data, 1, entry->len, stream);
  }
}

static void traverse_deps(EIr *ir, u32 *procs_len, u32 *data_len) {
  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    *procs_len += ir->module_deps.items[i].ir.procs.len;
    *data_len += ir->module_deps.items[i].ir.data.len;
    traverse_deps(&ir->module_deps.items[i].ir, procs_len, data_len);
  }
}

static void encode_deps_procs(FILE *stream, EIr *ir, u32 *data_base) {
  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;
    encode_procs_no_len(stream, &dep->ir.procs, &dep->ir.structs, &dep->varss, *data_base);
    *data_base += dep->ir.data.len;
    encode_deps_procs(stream, &dep->ir, data_base);
  }
}

static void encode_deps_data(FILE *stream, EIr *ir) {
  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    EModuleDep *dep = ir->module_deps.items + i;
    encode_data_no_len(stream, &dep->ir.data);
    encode_deps_data(stream, &dep->ir);
  }
}

void encode_ir_as_evm_ir(FILE *stream, EIr *ir, Varss *varss, bool include_deps) {
  u32 procs_len = ir->procs.len;
  u32 data_len = ir->data.len;
  if (include_deps)
    traverse_deps(ir, &procs_len, &data_len);

  fwrite(&procs_len, sizeof(procs_len), 1, stream);
  encode_procs_no_len(stream, &ir->procs, &ir->structs, varss, 0);
  if (include_deps) {
    u32 data_base = ir->data.len;
    encode_deps_procs(stream, ir, &data_base);
  }

  fwrite(&data_len, sizeof(data_len), 1, stream);
  encode_data_no_len(stream, &ir->data);
  if (include_deps)
    encode_deps_data(stream, ir);
}
