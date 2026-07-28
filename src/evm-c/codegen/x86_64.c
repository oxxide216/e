#include <assert.h>

#include "codegen.h"
#include "utils.h"
#include "shl/shl-log.h"
#ifndef NDEBUG
#include "../ir-print.h"
#endif

// 8 bytes for base pointer + 8 for return address
#define DEFAULT_STACK_SIZE 16
// 8 bytes for base pointer + 8 for return address
#define ARGS_STACK_OFFSET  16

static Str scratch_regs1[] = {
  STR_LIT("bl"),
  STR_LIT("r12b"),
  STR_LIT("r13b"),
  STR_LIT("r14b"),
  STR_LIT("r15b"),
};

static Str scratch_regs2[] = {
  STR_LIT("bx"),
  STR_LIT("r12w"),
  STR_LIT("r13w"),
  STR_LIT("r14w"),
  STR_LIT("r15w"),
};

static Str scratch_regs4[] = {
  STR_LIT("ebx"),
  STR_LIT("r12d"),
  STR_LIT("r13d"),
  STR_LIT("r14d"),
  STR_LIT("r15d"),
};

static Str scratch_regs8[] = {
  STR_LIT("rbx"),
  STR_LIT("r12"),
  STR_LIT("r13"),
  STR_LIT("r14"),
  STR_LIT("r15"),
};

static Str arg_regs1[] = {
  STR_LIT("dil"),
  STR_LIT("sil"),
  STR_LIT("dl"),
  STR_LIT("cl"),
  STR_LIT("r8b"),
  STR_LIT("r9b"),
};

static Str arg_regs2[] = {
  STR_LIT("di"),
  STR_LIT("si"),
  STR_LIT("dx"),
  STR_LIT("cx"),
  STR_LIT("r8w"),
  STR_LIT("r9w"),
};

static Str arg_regs4[] = {
  STR_LIT("edi"),
  STR_LIT("esi"),
  STR_LIT("edx"),
  STR_LIT("ecx"),
  STR_LIT("r8d"),
  STR_LIT("r9d"),
};

static Str arg_regs8[] = {
  STR_LIT("rdi"),
  STR_LIT("rsi"),
  STR_LIT("rdx"),
  STR_LIT("rcx"),
  STR_LIT("r8"),
  STR_LIT("r9"),
};

static Str temp_regs1[] = {
  STR_LIT("al"),
  STR_LIT("r10b"),
  STR_LIT("r11b"),
};

static Str temp_regs2[] = {
  STR_LIT("ax"),
  STR_LIT("r10w"),
  STR_LIT("r11w"),
};

static Str temp_regs4[] = {
  STR_LIT("eax"),
  STR_LIT("r10d"),
  STR_LIT("r11d"),
};

static Str temp_regs8[] = {
  STR_LIT("rax"),
  STR_LIT("r10"),
  STR_LIT("r11"),
};

static Str get_return_reg(u32 size) {
  switch (size) {
  case 1:  return STR_LIT("al");
  case 2:  return STR_LIT("ax");
  case 4:  return STR_LIT("eax");
  case 8:  return STR_LIT("rax");

  default: {
    ERROR("Size %u\n", size);
    return (Str) {0};
  }
  }
}

static Str get_remainder_reg(u32 size) {
  switch (size) {
  case 1:  return STR_LIT("dl");
  case 2:  return STR_LIT("dx");
  case 4:  return STR_LIT("edx");
  case 8:  return STR_LIT("rdx");

  default: {
    ERROR("Size %u\n", size);
    return (Str) {0};
  }
  }
}

static Str *get_scratch_regs(u32 size) {
  switch (size) {
  case 1:  return scratch_regs1;
  case 2:  return scratch_regs2;
  case 4:  return scratch_regs4;
  case 8:  return scratch_regs8;

  default: {
    ERROR("Size %u\n", size);
    return NULL;
  }
  }
}

static Str *get_arg_regs(u32 size) {
  switch (size) {
  case 1:  return arg_regs1;
  case 2:  return arg_regs2;
  case 4:  return arg_regs4;
  case 8:  return arg_regs8;

  default: {
    ERROR("Size %u\n", size);
    return NULL;
  }
  }
}

static Str *get_temp_regs(u32 size) {
  switch (size) {
  case 1:  return temp_regs1;
  case 2:  return temp_regs2;
  case 4:  return temp_regs4;
  case 8:  return temp_regs8;

  default: {
    ERROR("Size %u\n", size);
    return NULL;
  }
  }
}

static Str get_mem_prefix(u32 size) {
  switch (size) {
  case 1:  return STR_LIT("byte");
  case 2:  return STR_LIT("word");
  case 4:  return STR_LIT("dword");
  case 8:  return STR_LIT("qword");

  default: {
    ERROR("Size %u\n", size);
    return (Str) {0};
  }
  }
}

static void write_loc_of_size(FILE *stream, VarLoc *loc, u32 size) {
  if (loc->is_arg) {
    if (loc->value >= 0) {
      fprintf(stream, STR_FMT, STR_ARG(get_arg_regs(size)[loc->value]));
    } else {
      u32 reg_size = size <= 8 ? size : 8;
      fprintf(stream, STR_FMT"[rbp+%d]", STR_ARG(get_mem_prefix(reg_size)), -loc->value);
    }
  } else {
    if (loc->value >= 0) {
      fprintf(stream, STR_FMT, STR_ARG(get_scratch_regs(size)[loc->value]));
    } else {
      u32 reg_size = size <= 8 ? size : 8;
      fprintf(stream, STR_FMT"[rbp-%d]", STR_ARG(get_mem_prefix(reg_size)), -loc->value);
    }
  }
}

static void write_loc(FILE *stream, VarLoc *loc) {
  write_loc_of_size(stream, loc, loc->size);
}

static void write_loc_of_size_ensure_in_reg(FILE *stream, VarLoc *loc, u32 size, u32 temp_reg_index) {
  assert(temp_reg_index < ARRAY_LEN(temp_regs8));

  if (loc->value >= 0) {
    write_loc_of_size(stream, loc, size);
  } else {
    u32 reg_size = size <= 8 ? size : 8;
    fprintf(stream, STR_FMT, STR_ARG(get_temp_regs(reg_size)[temp_reg_index]));
  }
}

static void write_loc_ensure_in_reg(FILE *stream, VarLoc *loc, u32 temp_reg_index) {
  write_loc_of_size_ensure_in_reg(stream, loc, loc->size, temp_reg_index);
}

static void write_loc_part_of_size(FILE *stream, VarLoc *loc, u32 offset, u32 size) {
  if (loc->value >= 0) {
    if (offset != 0) {
      ERROR("Invalid offset for an in-register variable\n");
      exit(1);
    }

    write_loc_of_size(stream, loc, size);
  } else {
    if (loc->is_arg) {
      if (loc->value >= 0) {
        fprintf(stream, STR_FMT, STR_ARG(get_arg_regs(size)[loc->value + offset / size]));
      } else {
        write_str(stream, get_mem_prefix(size));
        write_cstr(stream, "[");
        fprintf(stream, "rbp+%u]", -loc->value + offset);
      }
    } else {
      if (loc->value >= 0) {
        fprintf(stream, STR_FMT, STR_ARG(get_scratch_regs(size)[loc->value + offset / size]));
      } else {
        write_str(stream, get_mem_prefix(size));
        write_cstr(stream, "[");
        fprintf(stream, "rbp-%u]", -loc->value - offset);
      }
    }
  }
}

static void ensure_in_reg_of_size(FILE *stream, VarLoc *loc, u32 size, u32 temp_reg_index, bool ref) {
  if (loc->value < 0) {
    if (ref)
      fprintf(stream, "  lea "STR_FMT",", STR_ARG(get_temp_regs(8)[temp_reg_index]));
    else
      fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(size)[temp_reg_index]));
    write_loc_of_size(stream, loc, size);
    write_cstr(stream, "\n");
  }
}

static void ensure_in_reg(FILE *stream, VarLoc *loc, u32 temp_reg_index, bool ref) {
  ensure_in_reg_of_size(stream, loc, loc->size, temp_reg_index, ref);
}

static void write_basic_op(FILE *stream, VarLocs *locs, u32 dest_index,
                           u32 src0_index, u32 src1_index, char *op) {
  locs->items[dest_index].kind = locs->items[src0_index].kind;
  locs->items[dest_index].size = locs->items[src0_index].size;

  if (locs->items[dest_index].value !=
      locs->items[src0_index].value) {
    if (locs->items[dest_index].value < 0)
      ensure_in_reg(stream, locs->items + src0_index, 0, false);
    write_cstr(stream, "  mov ");
    write_loc(stream, locs->items + dest_index);
    write_cstr(stream, ",");
    if (locs->items[dest_index].value < 0)
      write_loc_ensure_in_reg(stream, locs->items + src0_index, 0);
    else
      write_loc(stream, locs->items + src0_index);
    write_cstr(stream, "\n");
  }

  ensure_in_reg(stream, locs->items + src1_index, 0, false);

  write_cstr(stream, "  ");
  write_cstr(stream, op);
  write_cstr(stream, " ");
  write_loc(stream, locs->items + dest_index);
  write_cstr(stream, ",");
  write_loc_ensure_in_reg(stream, locs->items + src1_index, 0);
  write_cstr(stream, "\n");
}

u32 alignment_func_x86_64(u32 size) {
  if (size <= 8)
    return size;
  return 8;
}

void write_ir_as_asm_yasm_x86_64(FILE *stream, Ir *ir) {
  write_cstr(stream, "section '.text'\n");
  write_cstr(stream, "global _start\n");
  write_cstr(stream, "_start:\n");
  write_cstr(stream, "  lea rsi,[rsp+8]\n");
  write_cstr(stream, "  mov rdi,[rsp]\n");
  write_cstr(stream, "  call $main\n");
  write_cstr(stream, "  mov rdi,rax\n");
  write_cstr(stream, "  mov rax,60\n");
  write_cstr(stream, "  syscall\n");

  VarLocs locs = {0};

  for (u32 i = 0; i < ir->procs.len; ++i) {
    Proc *proc = ir->procs.items + i;

    bool has_nested_call = get_has_function_call(proc);

    u32 target_cap = has_nested_call ? proc->args.len * 2 : proc->args.len;
    if (locs.cap < target_cap) {
      u32 new_cap = locs.cap;
      if (new_cap < target_cap)
        new_cap = target_cap;
      if (locs.items)
        locs.items = realloc(locs.items, new_cap * sizeof(VarLoc));
      else
        locs.items = malloc(new_cap * sizeof(VarLoc));
      memset(locs.items + locs.cap, 0, (new_cap - locs.cap) * sizeof(VarLoc));
      locs.cap = new_cap;
    }

    SpaceUsed args_space = {0};
    args_space.stack_size = ARGS_STACK_OFFSET;

    if (proc->return_size > 16)
      ++args_space.regs;

    for (u32 j = 0; j < proc->args.len; ++j) {
      u32 size = proc->args.items[j].size;
      i32 value;
      if (size <= 8) {
        if (args_space.regs < ARRAY_LEN(arg_regs8)) {
          value = args_space.regs++;
        } else {
          value = -args_space.stack_size;
          args_space.stack_size += size;
        }
      } else if (size == 16) {
        if (args_space.regs + 1 < ARRAY_LEN(arg_regs8)) {
          value = args_space.regs;
          args_space.regs += 2;
        } else {
          value = -args_space.stack_size;
          args_space.stack_size += size;
        }
      } else {
        value = -args_space.stack_size;
        args_space.stack_size += size;
      }

      bool is_arg = !has_nested_call || j >= ARRAY_LEN(arg_regs8);
      VarLoc loc = {
        value, proc->args.items[j].kind,
        size, 0, 0, 0, false, is_arg,
      };
      locs.items[j] = loc;
    }

    align_fixed_offsets(proc, alignment_func_x86_64);
    add_var_locs(&locs, proc);
    promote_lifetimes_of_pre_loop_vars_to_ends_of_loops(proc, &locs);
    SpaceUsed space_used = var_locs_set_values(&locs, ARRAY_LEN(scratch_regs8));
    u32 total_space_used = DEFAULT_STACK_SIZE + space_used.regs * 8 +
                                                space_used.stack_size;

#ifndef NDEBUG
    proc_print(proc, &locs);
#endif

    write_cstr(stream, "$");
    write_str(stream, proc->name);
    write_cstr(stream, ":\n");
    write_cstr(stream, "  push rbp\n");
    write_cstr(stream, "  mov rbp,rsp\n");

    if (space_used.stack_size > 0)
      fprintf(stream, "  sub rsp,%u\n", space_used.stack_size);

    for (u32 j = 0; j < space_used.regs; ++j)
      fprintf(stream, "  push "STR_FMT"\n", STR_ARG(scratch_regs8[j]));

    write_cstr(stream, ".begin:\n");

    if (has_nested_call) {
      for (u32 j = 0; j < proc->args.len; ++j) {
        VarLoc *loc = locs.items + j;

        if (loc->is_arg)
          break;

        write_cstr(stream, "  mov ");
        write_loc_of_size(stream, loc, 8);
        write_cstr(stream, ",");
        write_str(stream, arg_regs8[j]);
        write_cstr(stream, "\n");
      }
    }

    u8 *labels = malloc(proc->instrs.len);
    memset(labels, 0, proc->instrs.len);
    for (u32 j = 0; j < proc->instrs.len; ++j) {
      Instr *instr = proc->instrs.items + j;

      if (instr->kind == InstrKindJump && instr->as.jump.target < proc->instrs.len)
        labels[instr->as.jump.target] = 1;
      else if (instr->kind == InstrKindJumpIfNot && instr->as.jump_if_not.target < proc->instrs.len)
        labels[instr->as.jump_if_not.target] = 1;
    }

    bool jump_optimization_applied = false;
    BinOpKind jump_optimization_bin_op_kind = 0;
    ValueKind jump_optimization_value_kind = 0;

    for (u32 j = 0; j < proc->instrs.len; ++j) {
      Instr *instr = proc->instrs.items + j;

      if (labels[j])
        fprintf(stream, ".l%u:\n", j);

      switch (instr->kind) {
      case InstrKindAlloc: break;

      case InstrKindStore: {
        locs.items[instr->as.store.index].kind = instr->as.store.value.kind;

        write_cstr(stream, "  mov ");
        write_loc(stream, locs.items + instr->as.store.index);
        write_cstr(stream, ",");
        write_value(stream, &instr->as.store.value);
        write_cstr(stream, "\n");
      } break;

      case InstrKindCopy: {
        VarLoc *dest_loc = locs.items + instr->as.copy.dest_index;
        VarLoc *src_loc = locs.items + instr->as.copy.src_index;

        dest_loc->kind = src_loc->kind;
        dest_loc->size = src_loc->size;

        if (dest_loc->value != src_loc->value || dest_loc->is_arg != src_loc->is_arg) {
          if (dest_loc->size <= 8) {
            if (dest_loc->value < 0)
              ensure_in_reg(stream, locs.items + instr->as.copy.src_index, 0, false);
            write_cstr(stream, "  mov ");
            write_loc(stream, locs.items + instr->as.copy.dest_index);
            write_cstr(stream, ",");
            if (dest_loc->value < 0)
              write_loc_ensure_in_reg(stream, locs.items + instr->as.copy.src_index, 0);
            else
              write_loc(stream, locs.items + instr->as.copy.src_index);
            write_cstr(stream, "\n");
          } else {
            u32 size = 0;
            while (size < dest_loc->size) {
              u32 part_size = 8;
              if (size + part_size > dest_loc->size)
                part_size = dest_loc->size - size;

              fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(8)[0]));
              write_loc_part_of_size(stream, src_loc, size, part_size);
              write_cstr(stream, "\n");
              write_loc_part_of_size(stream, dest_loc, size, part_size);
              write_str(stream, get_temp_regs(8)[0]);
              write_cstr(stream, "\n");

              size += part_size;
            }
          }
        }
      } break;

      case InstrKindBinOp: {
        switch (instr->as.bin_op.kind) {
        case BinOpKindAddInt: {
          write_basic_op(stream, &locs, instr->as.bin_op.dest_index,
                         instr->as.bin_op.src0_index, instr->as.bin_op.src1_index,
                         "add");
        } break;

        case BinOpKindSubInt: {
          write_basic_op(stream, &locs, instr->as.bin_op.dest_index,
                         instr->as.bin_op.src0_index, instr->as.bin_op.src1_index,
                         "sub");
        } break;

        case BinOpKindMulInt: {
          locs.items[instr->as.bin_op.dest_index].kind = locs.items[instr->as.bin_op.src0_index].kind;
          locs.items[instr->as.bin_op.dest_index].size = locs.items[instr->as.bin_op.src0_index].size;

          write_cstr(stream, "  mov ");
          write_str(stream, get_return_reg(locs.items[instr->as.bin_op.dest_index].size));
          write_cstr(stream, ",");
          write_loc(stream, locs.items + instr->as.bin_op.src0_index);
          write_cstr(stream, "\n");

          if (locs.items[instr->as.bin_op.dest_index].kind == ValueKindSigned)
            write_cstr(stream, "  imul ");
          else
            write_cstr(stream, "  mul ");
          write_loc(stream, locs.items + instr->as.bin_op.src1_index);
          write_cstr(stream, "\n");

          write_cstr(stream, "  mov ");
          write_loc(stream, locs.items + instr->as.bin_op.dest_index);
          write_cstr(stream, ",");
          write_str(stream, get_return_reg(locs.items[instr->as.bin_op.dest_index].size));
          write_cstr(stream, "\n");

        } break;

        case BinOpKindDivInt:
        case BinOpKindRem: {
          locs.items[instr->as.bin_op.dest_index].kind = locs.items[instr->as.bin_op.src0_index].kind;
          locs.items[instr->as.bin_op.dest_index].size = locs.items[instr->as.bin_op.src0_index].size;

          write_cstr(stream, "  mov ");
          write_str(stream, get_return_reg(locs.items[instr->as.bin_op.dest_index].size));
          write_cstr(stream, ",");
          write_loc(stream, locs.items + instr->as.bin_op.src0_index);
          write_cstr(stream, "\n");

          if (locs.items[instr->as.bin_op.dest_index].kind == ValueKindSigned)
            write_cstr(stream, "  idiv ");
          else
            write_cstr(stream, "  div ");
          write_loc(stream, locs.items + instr->as.bin_op.src1_index);
          write_cstr(stream, "\n");

          write_cstr(stream, "  mov ");
          write_loc(stream, locs.items + instr->as.bin_op.dest_index);
          write_cstr(stream, ",");
          if (instr->as.bin_op.kind == BinOpKindDivInt)
            write_str(stream, get_return_reg(locs.items[instr->as.bin_op.dest_index].size));
          else
            write_str(stream, get_remainder_reg(locs.items[instr->as.bin_op.dest_index].size));
          write_cstr(stream, "\n");
        } break;

        case BinOpKindAnd: {
          write_basic_op(stream, &locs, instr->as.bin_op.dest_index,
                         instr->as.bin_op.src0_index, instr->as.bin_op.src1_index,
                         "and");
        } break;

        case BinOpKindOr: {
          write_basic_op(stream, &locs, instr->as.bin_op.dest_index,
                         instr->as.bin_op.src0_index, instr->as.bin_op.src1_index,
                         "or");
        } break;

        case BinOpKindXor: {
          write_basic_op(stream, &locs, instr->as.bin_op.dest_index,
                         instr->as.bin_op.src0_index, instr->as.bin_op.src1_index,
                         "xor");
        } break;

        case BinOpKindLShift:
        case BinOpKindRShift: {
          locs.items[instr->as.bin_op.dest_index].kind = locs.items[instr->as.bin_op.src0_index].kind;
          locs.items[instr->as.bin_op.dest_index].size = locs.items[instr->as.bin_op.src0_index].size;

          if (locs.items[instr->as.bin_op.dest_index].value !=
              locs.items[instr->as.bin_op.src0_index].value ||
              locs.items[instr->as.bin_op.dest_index].is_arg !=
              locs.items[instr->as.bin_op.src0_index].is_arg) {
            if (locs.items[instr->as.bin_op.dest_index].value < 0)
              ensure_in_reg(stream, locs.items + instr->as.bin_op.src0_index, 0, false);
            write_cstr(stream, "  mov ");
            write_loc(stream, locs.items + instr->as.bin_op.dest_index);
            write_cstr(stream, ",");
            if (locs.items[instr->as.bin_op.dest_index].value < 0)
              write_loc_ensure_in_reg(stream, locs.items + instr->as.bin_op.src0_index, 0);
            else
              write_loc(stream, locs.items + instr->as.bin_op.src0_index);
            write_cstr(stream, "\n");
          }

          write_cstr(stream, "  mov cl,");
          write_loc_of_size(stream, locs.items + instr->as.bin_op.src1_index, 1);
          write_cstr(stream, "\n");

          if (instr->as.bin_op.kind == BinOpKindLShift) {
            if (locs.items[instr->as.bin_op.dest_index].kind == ValueKindSigned)
              write_cstr(stream, "  sal ");
            else
              write_cstr(stream, "  shl ");
          } else {
            if (locs.items[instr->as.bin_op.dest_index].kind == ValueKindSigned)
              write_cstr(stream, "  sar ");
            else
              write_cstr(stream, "  shr ");
          }
          write_loc(stream, locs.items + instr->as.bin_op.dest_index);
          write_cstr(stream, ",cl\n");
        } break;

        case BinOpKindEqInt:
        case BinOpKindNeInt:
        case BinOpKindLsInt:
        case BinOpKindLeInt:
        case BinOpKindGtInt:
        case BinOpKindGeInt: {
          static char *cmp_mnemonics[] = {
            "cmove",
            "cmovne",
            "cmovl",
            "cmovle",
            "cmovg",
            "cmovge",
            "cmove",
            "cmovne",
            "cmovb",
            "cmovbe",
            "cmova",
            "cmovae",
          };

          locs.items[instr->as.bin_op.dest_index].kind = ValueKindUnsigned;
          locs.items[instr->as.bin_op.dest_index].size = 1;

          ensure_in_reg(stream, locs.items + instr->as.bin_op.src1_index, 0, false);

          write_cstr(stream, "  cmp ");
          write_loc(stream, locs.items + instr->as.bin_op.src0_index);
          write_cstr(stream, ",");
          write_loc_ensure_in_reg(stream, locs.items + instr->as.bin_op.src1_index, 0);
          write_cstr(stream, "\n");

          jump_optimization_applied =
            j + 1 < proc->instrs.len && instr[1].kind == InstrKindJumpIfNot;
          if (jump_optimization_applied) {
            jump_optimization_bin_op_kind = instr->as.bin_op.kind;
            jump_optimization_value_kind = locs.items[instr->as.bin_op.dest_index].kind;
          } else {
            ensure_in_reg(stream, locs.items + instr->as.bin_op.src1_index, 0, false);

            write_cstr(stream, "  mov ");
            write_loc(stream, locs.items + instr->as.bin_op.src0_index);
            write_cstr(stream, ",");
            write_loc_ensure_in_reg(stream, locs.items + instr->as.bin_op.src1_index, 0);
            write_cstr(stream, "\n");

            write_cstr(stream, "  mov ");
            write_str(stream, temp_regs8[0]);
            write_cstr(stream, ",1\n");

            u32 index = (instr->as.bin_op.kind - BinOpKindEqInt) *
                        ((locs.items[instr->as.bin_op.dest_index].kind == ValueKindUnsigned) + 1);
           u32 size = locs.items[instr->as.bin_op.dest_index].size;
           if (size == 1)
             size = 2;

            write_cstr(stream, "  ");
            write_cstr(stream, cmp_mnemonics[index]);
            write_cstr(stream, " ");
            write_loc_of_size(stream, locs.items + instr->as.bin_op.dest_index, size);
            write_cstr(stream, ",");
            write_str(stream, get_temp_regs(size)[0]);
            write_cstr(stream, "\n");
          }
        } break;
        }
      } break;

      case InstrKindCall:
      case InstrKindCallAssign:
      case InstrKindCallRef:
      case InstrKindCallRefAssign: {
        Str name = {0};
        u32 index = (u32) -1;
        Indices *arg_indices;
        if (instr->kind == InstrKindCall) {
          name = instr->as.call.name;
          arg_indices = &instr->as.call.arg_indices;
        } else if (instr->kind == InstrKindCallAssign) {
          name = instr->as.call_assign.name;
          arg_indices = &instr->as.call_assign.arg_indices;
        } else if (instr->kind == InstrKindCallRef) {
          index = instr->as.call_ref.index;
          arg_indices = &instr->as.call.arg_indices;
        } else if (instr->kind == InstrKindCallRefAssign) {
          index = instr->as.call_ref_assign.index;
          arg_indices = &instr->as.call_assign.arg_indices;
        }

        bool is_tail = str_eq(name, proc->name) &&
                       (j + 1 == proc->instrs.len ||
                        instr[1].kind == InstrKindRet ||
                        instr[1].kind == InstrKindRetVal);

        if (!is_tail) {
          if (instr->kind == InstrKindCallAssign) {
            VarLoc *loc = locs.items + instr->as.call_assign.dest_index;
            loc->size = instr->as.call_assign.return_size;
            loc->kind = instr->as.call_assign.return_kind;

            if (loc->size > 16) {
              write_cstr(stream, "  lea rdi,");
              write_loc(stream, loc);
              write_cstr(stream, "\n");
            }
          } else if (instr->kind == InstrKindCallRefAssign) {
            VarLoc *loc = locs.items + instr->as.call_ref_assign.dest_index;
            loc->size = instr->as.call_ref_assign.return_size;
            loc->kind = instr->as.call_ref_assign.return_kind;

            if (loc->size > 16) {
              write_cstr(stream, "  lea rdi,");
              write_loc(stream, loc);
              write_cstr(stream, "\n");
            }
          }
        }

        SpaceUsed args_space = {0};
        for (u32 k = 0; k < arg_indices->len; ++k) {
          VarLoc *loc = locs.items + arg_indices->items[k];

          if (loc->size <= 8) {
            if (args_space.regs < ARRAY_LEN(arg_regs8))
              ++args_space.regs;
            else
              args_space.stack_size += loc->size;
          } else if (loc->size == 16) {
            if (args_space.regs + 1 < ARRAY_LEN(arg_regs8))
              args_space.regs += 2;
            else
              args_space.stack_size += loc->size;
          } else {
            args_space.stack_size += loc->size;
          }
        }

        u32 aligned = 0;
        if (args_space.stack_size > 0)
          aligned = align(args_space.stack_size + total_space_used, 16) - total_space_used;

        if (!is_tail && aligned > 0)
          fprintf(stream, "  sub rsp,%u\n", aligned);

        args_space.regs = 0;
        for (u32 k = 0; k < arg_indices->len; ++k) {
          VarLoc *loc = locs.items + arg_indices->items[k];

          if (loc->size <= 8) {
            if (args_space.regs < ARRAY_LEN(arg_regs8)) {
              write_cstr(stream, "  mov ");
              write_str(stream, get_arg_regs(loc->size)[args_space.regs++]);
              write_cstr(stream, ",");
              write_loc(stream, loc);
              write_cstr(stream, "\n");
            } else {
              ensure_in_reg(stream, loc, 0, false);

              write_cstr(stream, "  mov ");
              if (is_tail)
                write_loc(stream, locs.items + k);
              else
                fprintf(stream, "[rsp+%u]", aligned - args_space.stack_size);
              write_cstr(stream, ",");
              write_loc_ensure_in_reg(stream, loc, 0);
              write_cstr(stream, "\n");
              args_space.stack_size -= loc->size;
            }
          } else if (loc->size == 16) {
            if (args_space.regs + 1 < ARRAY_LEN(arg_regs8)) {
              write_cstr(stream, "  mov ");
              write_str(stream, get_arg_regs(8)[args_space.regs++]);
              write_cstr(stream, ",");
              write_loc_part_of_size(stream, loc, 0, 8);
              write_cstr(stream, "\n");

              write_cstr(stream, "  mov ");
              write_str(stream, get_arg_regs(8)[args_space.regs++]);
              write_cstr(stream, ",");
              write_loc_part_of_size(stream, loc, 8, 8);
              write_cstr(stream, "\n");
            } else {
              fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(8)[0]));
              write_loc_part_of_size(stream, loc, 0, 8);
              write_cstr(stream, "\n");
              write_cstr(stream, "  mov ");
              if (is_tail)
                write_loc(stream, locs.items + k);
              else
                fprintf(stream, "[rsp+%u]", aligned - args_space.stack_size);
              write_cstr(stream, ",");
              write_str(stream, get_temp_regs(8)[0]);
              write_cstr(stream, "\n");
              args_space.stack_size -= 8;

              fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(8)[0]));
              write_loc_part_of_size(stream, loc, 8, 8);
              write_cstr(stream, "\n");
              write_cstr(stream, "  mov ");
              if (is_tail)
                write_loc(stream, locs.items + k);
              else
                fprintf(stream, "[rsp+%u]", aligned - args_space.stack_size);
              write_cstr(stream, ",");
              write_str(stream, get_temp_regs(8)[0]);
              write_cstr(stream, "\n");
              args_space.stack_size -= 8;
            }
          } else {
            u32 arg_size = 0;
            while (arg_size < loc->size) {
              u32 part_size = 8;
              if (arg_size + part_size > loc->size)
                part_size = loc->size - arg_size;

              fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(part_size)[0]));
              write_loc_part_of_size(stream, loc, arg_size, part_size);
              write_cstr(stream, "\n");
              write_cstr(stream, "  mov ");
              if (is_tail)
                write_loc(stream, locs.items + k);
              else
                fprintf(stream, "[rsp+%u]", aligned - args_space.stack_size);
              write_cstr(stream, ",");
              write_str(stream, get_temp_regs(part_size)[0]);
              write_cstr(stream, "\n");

              args_space.stack_size -= part_size;
              arg_size += part_size;
            }
          }
        }

        if (is_tail) {
          write_cstr(stream, "  jmp .begin\n");
        } else {
          if (index == (u32) -1) {
            fprintf(stream, "  call $"STR_FMT"\n", STR_ARG(name));
          } else {
            write_cstr(stream, "  call ");
            write_loc(stream, locs.items + index);
            write_cstr(stream, "\n");
          }
        }

        if (!is_tail && aligned > 0)
          fprintf(stream, "  add rsp,%u\n", aligned);

        if (!is_tail && instr->kind == InstrKindCallAssign) {
          VarLoc *loc = locs.items + instr->as.bin_op.dest_index;

          if (loc->size <= 8 || loc->size == 16) {
            u32 size = loc->size == 16 ? 8 : loc->size;

            write_cstr(stream, "  mov ");
            write_loc(stream, loc);
            write_cstr(stream, ",");
            write_str(stream, get_return_reg(size));
            write_cstr(stream, "\n");
          }

          if (loc->size == 16) {
            write_cstr(stream, "  mov ");
            write_loc_part_of_size(stream, loc, 8, 8);
            write_cstr(stream, ",");
            write_str(stream, get_remainder_reg(8));
            write_cstr(stream, "\n");
          }
        }
      } break;

      case InstrKindRet: {
        if (j + 1 < proc->instrs.len)
          write_cstr(stream, "  jmp .end\n");
      } break;

      case InstrKindRetVal: {
        VarLoc *loc = locs.items + instr->as.ret_val.index;

        if (loc->size <= 8 || loc->size == 16) {
          u32 size = loc->size == 16 ? 8 : loc->size;

          write_cstr(stream, "  mov ");
          write_str(stream, get_return_reg(size));
          write_cstr(stream, ",");
          write_loc_of_size(stream, loc, size);
          write_cstr(stream, "\n");
        }

        if (loc->size == 16) {
          write_cstr(stream, "  mov ");
          write_str(stream, get_remainder_reg(8));
          write_cstr(stream, ",");
          write_loc_part_of_size(stream, loc, 8, 8);
          write_cstr(stream, "\n");
        }

        if (loc->size > 16) {
          u32 arg_size = 0;
          while (arg_size < loc->size) {
            u32 part_size = 8;
            if (part_size + arg_size > loc->size)
              part_size = loc->size - arg_size;

            fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(part_size)[0]));
            write_loc_part_of_size(stream, loc, arg_size, part_size);
            write_cstr(stream, "\n");
            args_space.stack_size += part_size;
            fprintf(stream, "  mov [rdi+%u],", arg_size);
            write_str(stream, get_temp_regs(part_size)[0]);
            write_cstr(stream, "\n");

            arg_size += part_size;
          }
        }

        if (j + 1 < proc->instrs.len)
          write_cstr(stream, "  jmp .end\n");
      } break;

      case InstrKindJump: {
        fprintf(stream, "  jmp .l%u\n", instr->as.jump.target);
      } break;

      case InstrKindJumpIfNot: {
        if (jump_optimization_applied) {
          static char *cmp_mnemonics[] = {
            "jne",
            "je",
            "jge",
            "jg",
            "jle",
            "jl",
            "jne",
            "je",
            "jae",
            "ja",
            "jbe",
            "jb",
          };

          u32 index = jump_optimization_bin_op_kind - BinOpKindEqInt +
                      6 * (jump_optimization_value_kind == ValueKindUnsigned);

          write_cstr(stream, "  ");
          write_cstr(stream, cmp_mnemonics[index]);
          fprintf(stream, " .l%u\n", instr->as.jump_if_not.target);
        } else {
          write_cstr(stream, "  cmp ");
          write_loc(stream, locs.items + instr->as.jump_if_not.cond_index);
          fprintf(stream, ",0\n  je .l%u\n", instr->as.jump_if_not.target);
        }
      } break;

      case InstrKindRef: {
        locs.items[instr->as.ref.dest_index].kind = ValueKindUnsigned;
        locs.items[instr->as.ref.dest_index].size = 8;

        write_cstr(stream, "  lea ");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.ref.dest_index, 0);
        write_cstr(stream, ",");
        write_loc(stream, locs.items + instr->as.ref.src_index);
        write_cstr(stream, "\n");

        if (locs.items[instr->as.ref.dest_index].value < 0) {
          write_cstr(stream, "  mov ");
          write_loc(stream, locs.items + instr->as.ref.dest_index);
          write_cstr(stream, ",");
          write_str(stream, get_temp_regs(locs.items[instr->as.ref.dest_index].size)[0]);
          write_cstr(stream, "\n");
        }
      } break;

      case InstrKindCopyToRef: {
        ensure_in_reg(stream, locs.items + instr->as.copy_to_ref.dest_index, 0, false);
        if (instr->as.copy_to_ref.dest_offset_index != (u32) -1)
          ensure_in_reg(stream, locs.items + instr->as.copy_to_ref.dest_offset_index, 2, false);
        ensure_in_reg(stream, locs.items + instr->as.copy_to_ref.src_index, 1, false);

        write_cstr(stream, "  mov [");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_to_ref.dest_index, 0);
        if (instr->as.copy_to_ref.dest_offset_index != (u32) -1) {
          write_cstr(stream, "+");
          write_loc_of_size_ensure_in_reg(stream, locs.items + instr->as.copy_to_ref.dest_offset_index, 8, 2);
          fprintf(stream, "*%u", locs.items[instr->as.copy_to_ref.src_index].size);
        }
        write_cstr(stream, "],");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_to_ref.src_index, 1);
        write_cstr(stream, "\n");
      } break;

      case InstrKindCopyFromRef: {
        if (instr->as.copy_from_ref.take_ref) {
          locs.items[instr->as.copy_from_ref.dest_index].kind = ValueKindUnsigned;
          locs.items[instr->as.copy_from_ref.dest_index].size = 8;
        } else {
          locs.items[instr->as.copy_from_ref.dest_index].kind =
            instr->as.copy_from_ref.src_target_kind;
          locs.items[instr->as.copy_from_ref.dest_index].size =
            instr->as.copy_from_ref.src_target_size;
        }

        ensure_in_reg(stream, locs.items + instr->as.copy_from_ref.src_index, 0, false);
        if (instr->as.copy_from_ref.src_offset_index != (u32) -1)
          ensure_in_reg(stream, locs.items + instr->as.copy_from_ref.src_offset_index, 1, false);

        if (instr->as.copy_from_ref.take_ref)
          write_cstr(stream, "  lea ");
        else
          write_cstr(stream, "  mov ");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_from_ref.dest_index, 0);
        write_cstr(stream, ",[");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_from_ref.src_index, 0);
        if (instr->as.copy_from_ref.src_offset_index != (u32) -1) {
          write_cstr(stream, "+");
          write_loc_of_size_ensure_in_reg(stream, locs.items + instr->as.copy_from_ref.src_offset_index, 8, 1);
          fprintf(stream, "*%u", instr->as.copy_from_ref.src_target_size);
        }
        write_cstr(stream, "]\n");

        if (locs.items[instr->as.copy_from_ref.dest_index].value < 0) {
          write_cstr(stream, "  mov ");
          write_loc(stream, locs.items + instr->as.copy_from_ref.dest_index);
          write_cstr(stream, ",");
          write_str(stream, get_temp_regs(locs.items[instr->as.copy_from_ref.dest_index].size)[0]);
          write_cstr(stream, "\n");
        }
      } break;

      case InstrKindInlineAsm: {
        write_cstr(stream, "  ");
        for (u32 k = 0; k < instr->as.inline_asm.segments.len; ++k) {
          AsmSegment *segment = instr->as.inline_asm.segments.items + k;

          if (segment->kind == AsmSegmentKindStr)
            write_str(stream, segment->value);
          else
            write_loc(stream, locs.items + segment->index);
        }
        fputc('\n', stream);
      } break;

      case InstrKindStoreData: {
        locs.items[instr->as.store_data.index].kind = ValueKindUnsigned;
        locs.items[instr->as.store_data.index].size = 8;

        write_cstr(stream, "  lea ");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.store_data.index, 0);
        fprintf(stream, ",[data_%u]\n", instr->as.store_data.data_index);

        if (locs.items[instr->as.store_data.index].value < 0) {
          write_cstr(stream, "  mov ");
          write_loc(stream, locs.items + instr->as.store_data.index);
          write_cstr(stream, ",");
          write_str(stream, get_temp_regs(locs.items[instr->as.store_data.index].size)[0]);
          write_cstr(stream, "\n");
        }
      } break;

      case InstrKindConvert: {
        ValueKind dest_kind = instr->as.convert.dest_kind;
        u32 dest_size = instr->as.convert.dest_size;
        ValueKind src_kind = locs.items[instr->as.convert.src_index].kind;
        u32 src_size = locs.items[instr->as.convert.src_index].size;

        bool needs_convertation = dest_size != src_size && dest_size <= 8 &&
                                  src_size <= 8 && dest_size > src_size;

        if (needs_convertation) {
          if (src_kind == ValueKindSigned) {
            if (dest_size == 2 && src_size == 1)
              write_cstr(stream, "  movsx ");
            else if (dest_size == 4 && src_size == 1)
              write_cstr(stream, "  movsx ");
            else if (dest_size == 8 && src_size == 1)
              write_cstr(stream, "  movsx ");
            else if (dest_size == 4 && src_size == 2)
              write_cstr(stream, "  movsx ");
            else if (dest_size == 8 && src_size == 2)
              write_cstr(stream, "  movsx ");
            else if (dest_size == 8 && src_size == 4)
              write_cstr(stream, "  mov ");
          } else {
            if (dest_size == 2 && src_size == 1)
              write_cstr(stream, "  movzx ");
            else if (dest_size == 4 && src_size == 1)
              write_cstr(stream, "  movzx ");
            else if (dest_size == 8 && src_size == 1)
              write_cstr(stream, "  movzx ");
            else if (dest_size == 4 && src_size == 2)
              write_cstr(stream, "  movzx ");
            else if (dest_size == 8 && src_size == 2)
              write_cstr(stream, "  movzx ");
            else if (dest_size == 8 && src_size == 4)
              write_cstr(stream, "  mov ");
          }
        } else if (locs.items[instr->as.convert.dest_index].value !=
                   locs.items[instr->as.convert.src_index].value) {
          write_cstr(stream, "  mov ");
        }

        if (needs_convertation ||
            locs.items[instr->as.convert.dest_index].value !=
            locs.items[instr->as.convert.src_index].value) {
          if (!needs_convertation || (dest_size == 8 && src_size == 4))
            write_loc_of_size_ensure_in_reg(stream, locs.items + instr->as.convert.dest_index, src_size, 0);
          else
            write_loc_ensure_in_reg(stream, locs.items + instr->as.convert.dest_index, 0);
          write_cstr(stream, ",");
          write_loc(stream, locs.items + instr->as.convert.src_index);
          write_cstr(stream, "\n");

          if (locs.items[instr->as.convert.dest_index].value < 0) {
            write_cstr(stream, "  mov ");
            write_loc(stream, locs.items + instr->as.convert.dest_index);
            write_cstr(stream, ",");
            write_str(stream, get_temp_regs(locs.items[instr->as.convert.dest_index].size)[0]);
            write_cstr(stream, "\n");
          }
        }

        locs.items[instr->as.convert.dest_index].kind = dest_kind;
        locs.items[instr->as.convert.dest_index].size = dest_size;
      } break;

      case InstrKindCopyToRefFixed: {
        i32 offset = instr->as.copy_to_ref_fixed.dest_segments.items[instr->as.copy_to_ref_fixed.dest_segments.len - 1].offset;
        bool deref = instr->as.copy_to_ref_fixed.deref || offset != 0;

        if (deref) {
          ensure_in_reg(stream, locs.items + instr->as.copy_to_ref_fixed.dest_index, 0, !deref);
          ensure_in_reg(stream, locs.items + instr->as.copy_to_ref_fixed.src_index, 1, false);
        }

        write_cstr(stream, "  mov ");
        if (!deref) {
          write_loc(stream, locs.items + instr->as.copy_to_ref_fixed.dest_index);
        } else {
          write_cstr(stream, "[");
          write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_to_ref_fixed.dest_index, 0);
          if (offset >= 0)
            fprintf(stream, "+%d]", offset);
          else
            fprintf(stream, "-%d]", -offset);
        }
        write_cstr(stream, ",");

        write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_to_ref_fixed.src_index, 1);
        write_cstr(stream, "\n");
      } break;

      case InstrKindCopyFromRefFixed: {
        if (instr->as.copy_from_ref_fixed.take_ref) {
          locs.items[instr->as.copy_from_ref_fixed.dest_index].kind = ValueKindUnsigned;
          locs.items[instr->as.copy_from_ref_fixed.dest_index].size = 8;
        } else {
          locs.items[instr->as.copy_from_ref_fixed.dest_index].kind =
            instr->as.copy_from_ref_fixed.src_target_kind;
          locs.items[instr->as.copy_from_ref_fixed.dest_index].size =
            instr->as.copy_from_ref_fixed.src_target_size;
        }

        i32 offset = instr->as.copy_from_ref_fixed.src_segments.items[instr->as.copy_from_ref_fixed.src_segments.len - 1].offset;
        bool deref = instr->as.copy_to_ref_fixed.deref || offset != 0;

        if (deref)
          ensure_in_reg(stream, locs.items + instr->as.copy_from_ref_fixed.src_index, 0, false);

        if (instr->as.copy_from_ref_fixed.take_ref)
          write_cstr(stream, "  lea ");
        else
          write_cstr(stream, "  mov ");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_from_ref_fixed.dest_index, 0);
        write_cstr(stream, ",");
        if (!deref) {
          write_loc(stream, locs.items + instr->as.copy_from_ref_fixed.src_index);
        } else {
          write_cstr(stream, "[");
          write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_from_ref_fixed.src_index, 0);
          if (offset >= 0)
            fprintf(stream, "+%d]", offset);
          else
            fprintf(stream, "-%d]", -offset);
        }
        write_cstr(stream, "\n");

        if (locs.items[instr->as.copy_from_ref_fixed.dest_index].value < 0) {
          write_cstr(stream, "  mov ");
          write_loc(stream, locs.items + instr->as.copy_from_ref_fixed.dest_index);
          write_cstr(stream, ",");
          write_str(stream, get_temp_regs(locs.items[instr->as.copy_from_ref_fixed.dest_index].size)[0]);
          write_cstr(stream, "\n");
        }
      } break;

      case InstrKindRefProc: {
        locs.items[instr->as.ref_proc.dest_index].kind = ValueKindUnsigned;
        locs.items[instr->as.ref_proc.dest_index].size = 8;

        write_cstr(stream, "  lea ");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.ref_proc.dest_index, 0);
        write_cstr(stream, ",[");
        write_str(stream, instr->as.ref_proc.proc_name);
        write_cstr(stream, "]\n");

        if (locs.items[instr->as.ref_proc.dest_index].value < 0) {
          write_cstr(stream, "  mov ");
          write_loc(stream, locs.items + instr->as.ref_proc.dest_index);
          write_cstr(stream, ",");
          write_str(stream, get_temp_regs(locs.items[instr->as.ref_proc.dest_index].size)[0]);
          write_cstr(stream, "\n");
        }
      } break;
      }
    }

    write_cstr(stream, ".end:\n");

    for (u32 i = space_used.regs; i > 0; --i)
      fprintf(stream, "  pop "STR_FMT"\n", STR_ARG(scratch_regs8[i - 1]));

    if (space_used.stack_size > 0)
      fprintf(stream, "  add rsp,%u\n", space_used.stack_size);

    write_cstr(stream, "  leave\n");
    write_cstr(stream, "  ret\n");

    if (labels)
      free(labels);
    memset(locs.items, 0, locs.cap * sizeof(VarLoc));
  }

  if (locs.items)
    free(locs.items);

  for (u32 i = 0; i < ir->imports.len; ++i)
    fprintf(stream, "extern $"STR_FMT"\n", STR_ARG(ir->imports.items[i]));

  if (ir->data.len > 0)
    write_cstr(stream, "section '.data'\n");

  for (u32 i = 0; i < ir->data.len; ++i) {
    DataEntry *entry = ir->data.items + i;

    fprintf(stream, "data_%u: db ", i);
    for (u32 j = 0; j < entry->len; ++j) {
      if (j > 0)
        fputc(',', stream);
      fprintf(stream, "%u", entry->data[j]);
    }
    fputc('\n', stream);
  }
}
