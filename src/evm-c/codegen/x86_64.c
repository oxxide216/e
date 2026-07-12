#include <assert.h>

#include "codegen.h"
#include "utils.h"
#include "shl/shl-log.h"

// 8 bytes for base pointer + 8 for return address
#define DEFAULT_STACK_SIZE 16
// 8 bytes for return address
#define ARGS_STACK_OFFSET  8

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

static Str get_return_reg(VarLoc *loc) {
  switch (loc->size) {
  case 1:  return STR_LIT("al");
  case 2:  return STR_LIT("ax");
  case 4:  return STR_LIT("eax");
  case 8:  return STR_LIT("rax");

  default: {
    ERROR("Size %u\n", loc->size);
    return (Str) {0};
  }
  }
}

static Str get_remainder_reg(VarLoc *loc) {
  switch (loc->size) {
  case 1:  return STR_LIT("dl");
  case 2:  return STR_LIT("dx");
  case 4:  return STR_LIT("edx");
  case 8:  return STR_LIT("rdx");

  default: {
    ERROR("Size %u\n", loc->size);
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
    if (loc->value >= 0)
      fprintf(stream, STR_FMT, STR_ARG(get_arg_regs(loc->size)[loc->value]));
    else
      fprintf(stream, STR_FMT"[rbp+%d]", STR_ARG(get_mem_prefix(loc->size)), -loc->value);
  } else {
    if (loc->value >= 0)
      fprintf(stream, STR_FMT, STR_ARG(get_scratch_regs(size)[loc->value]));
    else
      fprintf(stream, STR_FMT"[rbp-%d]", STR_ARG(get_mem_prefix(size)), -loc->value);
  }
}

static void write_loc(FILE *stream, VarLoc *loc) {
  write_loc_of_size(stream, loc, loc->size);
}

static void write_loc_of_size_ensure_in_reg(FILE *stream, VarLoc *loc, u32 size, u32 temp_reg_index) {
  assert(temp_reg_index < ARRAY_LEN(temp_regs8));

  if (loc->value >= 0)
    write_loc_of_size(stream, loc, size);
  else
    fprintf(stream, STR_FMT, STR_ARG(get_temp_regs(size)[temp_reg_index]));
}

static void write_loc_ensure_in_reg(FILE *stream, VarLoc *loc, u32 temp_reg_index) {
  write_loc_of_size_ensure_in_reg(stream, loc, loc->size, temp_reg_index);
}

static void write_loc_part_of_size(FILE *stream, VarLoc *loc, u32 offset, u32 size) {
  if (loc->is_arg) {
    if (loc->value >= 0) {
      fprintf(stream, STR_FMT, STR_ARG(get_arg_regs(size)[loc->value + offset / size]));
    } else {
      write_str(stream, get_mem_prefix(size));
      write_cstr(stream, "[");
      fprintf(stream, "rbp+%u]", ARGS_STACK_OFFSET + -loc->value + offset);
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

static void write_basic_op(FILE *stream, VarLocs *locs, u32 dest_index,
                           u32 src0_index, u32 src1_index, char *op) {
  locs->items[dest_index].kind = locs->items[src0_index].kind;
  locs->items[dest_index].size = locs->items[src0_index].size;

  if (locs->items[dest_index].value !=
      locs->items[src0_index].value) {
    write_cstr(stream, "  mov ");
    write_loc(stream, locs->items + dest_index);
    write_cstr(stream, ",");
    if (locs->items[dest_index].value < 0)
      write_loc_ensure_in_reg(stream, locs->items + src0_index, 0);
    else
      write_loc(stream, locs->items + src0_index);
    write_cstr(stream, "\n");
  }

  write_cstr(stream, "  ");
  write_cstr(stream, op);
  write_cstr(stream, " ");
  write_loc(stream, locs->items + dest_index);
  write_cstr(stream, ",");
  write_loc_ensure_in_reg(stream, locs->items + src1_index, 0);
  write_cstr(stream, "\n");
}

static void ensure_in_reg_of_size(FILE *stream, VarLoc *loc, u32 size, u32 temp_reg_index) {
  if (loc->value < 0) {
    fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(size)[temp_reg_index]));
    write_loc(stream, loc);
    write_cstr(stream, "\n");
  }
}

static void ensure_in_reg(FILE *stream, VarLoc *loc, u32 temp_reg_index) {
  ensure_in_reg_of_size(stream, loc, loc->size, temp_reg_index);
}

void write_ir_as_asm_yasm_x86_64(FILE *stream, Ir *ir) {
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

    if (locs.cap < proc->args.len) {
      u32 new_cap = locs.cap;
      if (new_cap < proc->args.len)
        new_cap = proc->args.len;
      if (locs.items)
        locs.items = realloc(locs.items, new_cap * sizeof(VarLoc));
      else
        locs.items = malloc(new_cap * sizeof(VarLoc));
      memset(locs.items + locs.cap, 0, (new_cap - locs.cap) * sizeof(VarLoc));
      locs.cap = new_cap;
    }

    SpaceUsed args_space = {0};
    args_space.stack_size = ARGS_STACK_OFFSET;
    for (u32 j = 0; j < proc->args.len; ++j) {
      u32 size = proc->args.items[j].size;
      i32 value;
      if (size <= 8) {
        if (args_space.regs < ARRAY_LEN(arg_regs8)) {
          value = args_space.regs++;
        } else {
          args_space.stack_size += size;
          value = -args_space.stack_size;
        }
      } else if (size <= 16) {
        if (args_space.regs + 1 < ARRAY_LEN(arg_regs8)) {
          value = args_space.regs;
          args_space.regs += 2;
        } else {
          args_space.stack_size += size;
          value = -args_space.stack_size;
        }
      } else {
        args_space.stack_size += size;
        value = -args_space.stack_size;
      }

      VarLoc loc = {
        value, proc->args.items[j].kind,
        size, 0, 0, 0, false, true,
      };
      locs.items[j] = loc;
    }

    add_var_locs(&locs, proc);
    promote_lifetimes_of_pre_loop_vars_to_ends_of_loops(proc, &locs);
    SpaceUsed space_used = var_locs_set_values(proc, &locs,
                                               ARRAY_LEN(scratch_regs8),
                                               DEFAULT_STACK_SIZE);
    u32 total_space_used = space_used.stack_size + space_used.regs * 8;

    write_cstr(stream, "$");
    write_str(stream, proc->name);
    write_cstr(stream, ":\n");

    if (space_used.stack_size > DEFAULT_STACK_SIZE) {
      write_cstr(stream, "  push rbp\n");
      write_cstr(stream, "  mov rbp,rsp\n");
      fprintf(stream, "  sub rsp,%u\n", space_used.stack_size - DEFAULT_STACK_SIZE);
    }

    for (u32 j = 0; j < space_used.regs; ++j)
      fprintf(stream, "  push "STR_FMT"\n", STR_ARG(scratch_regs8[j]));

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
        locs.items[instr->as.copy.dest_index].kind = locs.items[instr->as.copy.src_index].kind;
        locs.items[instr->as.copy.dest_index].size = locs.items[instr->as.copy.src_index].size;

        if (locs.items[instr->as.copy.dest_index].value !=
            locs.items[instr->as.copy.src_index].value) {
          if (locs.items[instr->as.copy.dest_index].value < 0)
            ensure_in_reg(stream, locs.items + instr->as.copy.src_index, 0);
          write_cstr(stream, "  mov ");
          write_loc(stream, locs.items + instr->as.copy.dest_index);
          write_cstr(stream, ",");
          if (locs.items[instr->as.copy.dest_index].value < 0)
            write_loc_ensure_in_reg(stream, locs.items + instr->as.copy.src_index, 0);
          else
            write_loc(stream, locs.items + instr->as.copy.src_index);
          write_cstr(stream, "\n");
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
          write_str(stream, get_return_reg(locs.items + instr->as.bin_op.dest_index));
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
          write_str(stream, get_return_reg(locs.items + instr->as.bin_op.dest_index));
          write_cstr(stream, "\n");

        } break;

        case BinOpKindDivInt:
        case BinOpKindRem: {
          locs.items[instr->as.bin_op.dest_index].kind = locs.items[instr->as.bin_op.src0_index].kind;
          locs.items[instr->as.bin_op.dest_index].size = locs.items[instr->as.bin_op.src0_index].size;

          write_cstr(stream, "  mov ");
          write_str(stream, get_return_reg(locs.items + instr->as.bin_op.dest_index));
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
            write_str(stream, get_return_reg(locs.items + instr->as.bin_op.dest_index));
          else
            write_str(stream, get_remainder_reg(locs.items + instr->as.bin_op.dest_index));
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
              locs.items[instr->as.bin_op.src0_index].value) {
            if (locs.items[instr->as.bin_op.dest_index].value < 0)
              ensure_in_reg(stream, locs.items + instr->as.bin_op.src0_index, 0);
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

          ensure_in_reg(stream, locs.items + instr->as.bin_op.src1_index, 0);

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
            ensure_in_reg(stream, locs.items + instr->as.bin_op.src1_index, 0);

            write_cstr(stream, "  mov ");
            write_loc(stream, locs.items + instr->as.bin_op.src0_index);
            write_cstr(stream, ",");
            write_loc_ensure_in_reg(stream, locs.items + instr->as.bin_op.src1_index, 0);
            write_cstr(stream, "\n");

            u32 index = (instr->as.bin_op.kind - BinOpKindEqInt) *
                        ((locs.items[instr->as.bin_op.dest_index].kind == ValueKindUnsigned) + 1);

            write_cstr(stream, "  ");
            write_cstr(stream, cmp_mnemonics[index]);
            write_cstr(stream, " ");
            write_loc(stream, locs.items + instr->as.bin_op.dest_index);
            write_cstr(stream, ",1\n");
          }
        } break;
        }
      } break;

      case InstrKindCall:
      case InstrKindCallAssign: {
        Str name;
        Indices *arg_indices;
        if (instr->kind == InstrKindCall) {
          name = instr->as.call.name;
          arg_indices = &instr->as.call.arg_indices;
        } else {
          name = instr->as.call_assign.name;
          arg_indices = &instr->as.call_assign.arg_indices;
        }

        SpaceUsed args_space = {0};
        for (u32 k = arg_indices->len; k > 0; --k) {
          VarLoc *loc = locs.items + arg_indices->items[k - 1];

          if (loc->size <= 8) {
            if (args_space.regs < ARRAY_LEN(arg_regs8))
              ++args_space.regs;
            else
              args_space.stack_size += loc->size;
          } else if (loc->size <= 16) {
            if (args_space.regs + 1 < ARRAY_LEN(arg_regs8))
              args_space.regs += 2;
            else
              args_space.stack_size += loc->size;
          } else {
            args_space.stack_size += loc->size;
          }
        }

        u32 aligned = align(args_space.stack_size + total_space_used, 16) - total_space_used;
        if (aligned > 0)
          fprintf(stream, "  sub rsp,%u\n", aligned);

        args_space = (SpaceUsed) {0};
        for (u32 k = arg_indices->len; k > 0; --k) {
          VarLoc *loc = locs.items + arg_indices->items[k - 1];

          if (loc->size <= 8) {
            if (args_space.regs < ARRAY_LEN(arg_regs8)) {
              write_cstr(stream, "  mov ");
              write_str(stream, get_arg_regs(loc->size)[args_space.regs++]);
              write_cstr(stream, ",");
              write_loc(stream, loc);
              write_cstr(stream, "\n");
            } else {
              args_space.stack_size += loc->size;
              ensure_in_reg(stream, loc, 0);
              fprintf(stream, "  mov [rsp+%u],", aligned - args_space.stack_size);
              write_loc_ensure_in_reg(stream, loc, 0);
              write_cstr(stream, "\n");
            }
          } else if (loc->size <= 16) {
            if (args_space.regs + 1 < ARRAY_LEN(arg_regs8)) {
              write_cstr(stream, "  mov ");
              write_str(stream, get_arg_regs(loc->size)[args_space.regs++]);
              write_cstr(stream, ",");
              write_loc(stream, loc);
              write_cstr(stream, "\n");

              write_cstr(stream, "  mov ");
              write_str(stream, get_arg_regs(loc->size)[args_space.regs++]);
              write_cstr(stream, ",");
              write_loc(stream, loc);
              write_cstr(stream, "\n");
            } else {
              fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(8)[0]));
              write_loc_part_of_size(stream, loc, 0, 8);
              write_cstr(stream, "\n");
              args_space.stack_size += 8;
              fprintf(stream, "  mov [rsp+%u],", aligned - args_space.stack_size);
              fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(8)[0]));
              write_cstr(stream, "\n");

              fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(8)[0]));
              write_loc_part_of_size(stream, loc, 8, 8);
              write_cstr(stream, "\n");
              args_space.stack_size += 8;
              fprintf(stream, "  mov [rsp+%u],", aligned - args_space.stack_size);
              fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(8)[0]));
              write_cstr(stream, "\n");
            }
          } else {
            u32 arg_size = 0;
            while (arg_size < loc->size) {
              u32 part_size = loc->size;
              if (part_size == 0)
                part_size = 8;
              args_space.stack_size += part_size;

              fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(8)[0]));
              write_loc_part_of_size(stream, loc, arg_size, part_size);
              write_cstr(stream, "\n");
              args_space.stack_size += part_size;
              fprintf(stream, "  mov [rsp+%u],", aligned - args_space.stack_size);
              fprintf(stream, "  mov "STR_FMT",", STR_ARG(get_temp_regs(8)[0]));
              write_cstr(stream, "\n");

              arg_size += part_size;
            }
          }
        }

        fprintf(stream, "  call $"STR_FMT"\n", STR_ARG(name));

        if (aligned > 0)
          fprintf(stream, "  add rsp,%u\n", aligned);

        if (instr->kind == InstrKindCallAssign) {
          get_proc_return_kind_and_size(&ir->procs, instr->as.call_assign.name,
                                        &locs.items[instr->as.bin_op.dest_index].kind,
                                        &locs.items[instr->as.bin_op.dest_index].size);

          write_cstr(stream, "  mov ");
          write_loc(stream, locs.items + instr->as.bin_op.dest_index);
          write_cstr(stream, ",");
          write_str(stream, get_return_reg(locs.items + instr->as.bin_op.dest_index));
          write_cstr(stream, "\n");
        }
      } break;

      case InstrKindRet: {
        if (j + 1 < proc->instrs.len)
          write_cstr(stream, "  jmp .end\n");
      } break;

      case InstrKindRetVal: {
        write_cstr(stream, "  mov ");
        write_str(stream, get_return_reg(locs.items + instr->as.copy.src_index));
        write_cstr(stream, ",");
        write_loc(stream, locs.items + instr->as.copy.src_index);
        write_cstr(stream, "\n");
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

          u32 index = (jump_optimization_bin_op_kind - BinOpKindEqInt) *
                      ((jump_optimization_value_kind == ValueKindUnsigned) + 1);

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
        ensure_in_reg(stream, locs.items + instr->as.copy_to_ref.dest_index, 0);
        ensure_in_reg(stream, locs.items + instr->as.copy_to_ref.src_index, 1);

        write_cstr(stream, "  mov [");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_to_ref.dest_index, 0);
        if (instr->as.copy_to_ref.dest_offset > 0)
          fprintf(stream, "+%u", instr->as.copy_to_ref.dest_offset);
        write_cstr(stream, "],");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_to_ref.src_index, 1);
        write_cstr(stream, "\n");
      } break;

      case InstrKindCopyFromRef: {
        ensure_in_reg(stream, locs.items + instr->as.copy_from_ref.src_index, 0);

        write_cstr(stream, "  mov ");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_from_ref.dest_index, 0);
        write_cstr(stream, ",[");
        write_loc_ensure_in_reg(stream, locs.items + instr->as.copy_from_ref.src_index, 0);
        if (instr->as.copy_from_ref.src_offset > 0)
          fprintf(stream, "+%u", instr->as.copy_from_ref.src_offset);
        write_cstr(stream, "]\n");

        if (locs.items[instr->as.copy_from_ref.dest_index].value < 0) {
          write_cstr(stream, "  mov ");
          write_loc(stream, locs.items + instr->as.copy_from_ref.dest_index);
          write_cstr(stream, ",");
          write_str(stream, get_temp_regs(locs.items[instr->as.copy_from_ref.dest_index].size)[0]);
          write_cstr(stream, "\n");
        }
      } break;
      }
    }

    write_cstr(stream, ".end:\n");

    for (u32 i = space_used.regs; i > 0; --i)
      fprintf(stream, "  pop "STR_FMT"\n", STR_ARG(scratch_regs8[i - 1]));

    if (space_used.stack_size > DEFAULT_STACK_SIZE) {
      fprintf(stream, "  add rsp,%u\n", space_used.stack_size - DEFAULT_STACK_SIZE);
      write_cstr(stream, "  leave\n");
    }

    write_cstr(stream, "  ret\n");

    if (labels)
      free(labels);
    memset(locs.items, 0, locs.cap * sizeof(VarLoc));
  }

  if (locs.items)
    free(locs.items);
}
