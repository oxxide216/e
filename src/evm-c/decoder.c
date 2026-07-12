#include "decoder.h"

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

typedef struct {
  Ir    *ir;
  Arena *arena;
  u8    *data;
  u32    len, decoded;
} Decoder;

static bool decode_buffer_impl(Decoder *decoder, void *buffer, u32 len) {
  if (decoder->decoded + len > decoder->len)
    return false;

  for (u32 i = 0; i < len; ++i)
    ((u8 *) buffer)[i] = decoder->data[decoder->decoded + i];

  decoder->decoded += len;

  return true;
}

static bool decode_str_impl(Decoder *decoder, Str *str) {
  decode_buffer(decoder, &str->len, sizeof(str->len));
  str->ptr = arena_alloc(decoder->arena, str->len);
  decode_buffer(decoder, str->ptr, str->len);

  return true;
}

bool decode_ir(Ir *ir, Arena *arena, u8 *data, u32 data_len) {
  Decoder decoder = { ir, arena, data, data_len, 0 };

  decode_buffer(&decoder, &ir->procs.len, sizeof(ir->procs.len));
  ir->procs.cap = ir->procs.len;
  ir->procs.items = arena_alloc(arena, ir->procs.cap * sizeof(Proc));

  for (u32 i = 0; i < ir->procs.len; ++i) {
    Proc *proc = ir->procs.items + i;

    decode_str(&decoder, &proc->name);

    decode_buffer(&decoder, &proc->args.len, sizeof(proc->args.len));
    proc->args.cap = proc->args.len;
    proc->args.items = arena_alloc(arena, proc->args.cap * sizeof(Arg));

    for (u32 j = 0; j < proc->args.len; ++j) {
      decode_buffer(&decoder, &proc->args.items[j].size, sizeof(proc->args.items[j].size));
      u8 arg_kind;
      decode_buffer(&decoder, &arg_kind, 1);
      proc->args.items[j].kind = arg_kind;
    }

    decode_buffer(&decoder, &proc->return_size, sizeof(proc->return_size));
    u8 return_kind;
    decode_buffer(&decoder, &return_kind, 1);
    proc->return_kind = return_kind;

    decode_buffer(&decoder, &proc->instrs.len, sizeof(proc->instrs.len));
    proc->instrs.cap = proc->instrs.len;
    proc->instrs.items = arena_alloc(arena, proc->instrs.cap * sizeof(Instr));

    for (u32 j = 0; j < proc->instrs.len; ++j) {
      Instr *instr = proc->instrs.items + j;

      u8 kind;
      decode_buffer(&decoder, &kind, 1);
      instr->kind = kind;

      switch (kind) {
      case InstrKindAlloc: {
        decode_buffer(&decoder, &instr->as.alloc.index, sizeof(instr->as.alloc.index));
        decode_buffer(&decoder, &instr->as.alloc.size, sizeof(instr->as.alloc.size));
      } break;

      case InstrKindStore: {
        decode_buffer(&decoder, &instr->as.store.index, sizeof(instr->as.store.index));
        u8 value_kind;
        decode_buffer(&decoder, &value_kind, 1);
        instr->as.store.value.kind = value_kind;
        switch (instr->as.store.value.kind) {
        case ValueKindSigned: {
          decode_buffer(&decoder, &instr->as.store.value.as._signed, sizeof(instr->as.store.value.as._signed));
        } break;

        case ValueKindUnsigned: {
          decode_buffer(&decoder, &instr->as.store.value.as._unsigned, sizeof(instr->as.store.value.as._unsigned));
        } break;
        }
      } break;

      case InstrKindCopy: {
        decode_buffer(&decoder, &instr->as.copy.dest_index, sizeof(instr->as.copy.dest_index));
        decode_buffer(&decoder, &instr->as.copy.src_index, sizeof(instr->as.copy.src_index));
      } break;

      case InstrKindBinOp: {
        decode_buffer(&decoder, &instr->as.bin_op.dest_index, sizeof(instr->as.bin_op.dest_index));
        decode_buffer(&decoder, &instr->as.bin_op.src0_index, sizeof(instr->as.bin_op.src0_index));
        decode_buffer(&decoder, &instr->as.bin_op.src1_index, sizeof(instr->as.bin_op.src1_index));

        u8 op_kind;
        decode_buffer(&decoder, &op_kind, 1);
        instr->as.bin_op.kind = op_kind;
      } break;

      case InstrKindCall: {
        decode_str(&decoder, &instr->as.call.name);

        decode_buffer(&decoder, &instr->as.call.arg_indices.len, sizeof(instr->as.call.arg_indices.len));
        instr->as.call.arg_indices.cap = instr->as.call.arg_indices.len;
        instr->as.call.arg_indices.items = arena_alloc(arena, instr->as.call.arg_indices.cap * sizeof(u32));
        for (u32 k = 0; k < instr->as.call.arg_indices.len; ++k) {
          u32 *arg_index = instr->as.call.arg_indices.items + k;
          decode_buffer(&decoder, arg_index, sizeof(*arg_index));
        }
      } break;

      case InstrKindCallAssign: {
        decode_buffer(&decoder, &instr->as.call_assign.dest_index, sizeof(instr->as.call_assign.dest_index));

        decode_str(&decoder, &instr->as.call_assign.name);

        decode_buffer(&decoder, &instr->as.call_assign.arg_indices.len, sizeof(instr->as.call_assign.arg_indices.len));
        instr->as.call_assign.arg_indices.cap = instr->as.call_assign.arg_indices.len;
        instr->as.call_assign.arg_indices.items = arena_alloc(arena, instr->as.call_assign.arg_indices.cap * sizeof(u32));
        for (u32 k = 0; k < instr->as.call_assign.arg_indices.len; ++k) {
          u32 *arg_index = instr->as.call_assign.arg_indices.items + k;
          decode_buffer(&decoder, arg_index, sizeof(*arg_index));
        }
      } break;

      case InstrKindRet: break;

      case InstrKindRetVal: {
        decode_buffer(&decoder, &instr->as.ret_val.index, sizeof(instr->as.ret_val.index));
      } break;

      case InstrKindJump: {
        decode_buffer(&decoder, &instr->as.jump.target, sizeof(instr->as.jump.target));
      } break;

      case InstrKindJumpIfNot: {
        decode_buffer(&decoder, &instr->as.jump_if_not.cond_index, sizeof(instr->as.jump_if_not.cond_index));
        decode_buffer(&decoder, &instr->as.jump_if_not.target, sizeof(instr->as.jump_if_not.target));
      } break;
      }
    }
  }

  return true;
}
