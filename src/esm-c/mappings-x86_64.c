#include "assembler.h"
#include "shl/shl-log.h"

static bool mov_r64i64(FILE *stream, EsmInstrArg *args) {
  u8 opcode[10] = {
    (4 << 4) | 8,
    0xB8 | (args[0].as.reg.kind % EsmRegKindR8),
  };

  for (u32 i = 0; i < 8; ++i)
    opcode[i + 2] = ((u8 *) &args[1].as.imm)[i];

  if (args[0].as.reg.kind >= EsmRegKindR8)
    opcode[0] |= 1;

  fwrite(opcode, 1, ARRAY_LEN(opcode), stream);

  return true;
}

static bool syscall(FILE *stream, EsmInstrArg *args) {
  (void) args;

  u8 opcode[] = { 0x0F, 0x05 };
  fwrite(opcode, 1, ARRAY_LEN(opcode), stream);

  return true;
}

static bool lea_r64m64(FILE *stream, EsmInstrArg *args) {
  if (args[1].as.mem.has_index &&
      (args[1].as.mem.index == EsmRegKindRSP ||
       args[1].as.mem.index == EsmRegKindR12))
    return false;

  if (args[1].as.mem.base.kind == EsmInstrArgBaseKindLabel) {
    ERROR("TODO: support for label as a base\n");
    return false;
  }

  u8 opcode[8] = {
    (4 << 4) | 8,
    0x8D,
    (args[0].as.reg.kind % EsmRegKindR8) << 3, // ModRM
    0,
  };
  u32 len = 3;

  if (args[0].as.reg.kind >= EsmRegKindR8)
    opcode[0] |= 4;

  bool has_sib =
    args[1].as.mem.has_index ||
    args[1].as.mem.base.as.reg == EsmRegKindRSP ||
    args[1].as.mem.base.as.reg == EsmRegKindR12;

  if (has_sib) {
    if (args[1].as.mem.base.as.reg >= EsmRegKindR8)
      opcode[0] |= 1;
    if (args[1].as.mem.has_index && args[1].as.mem.index >= EsmRegKindR8)
      opcode[0] |= 2;

    opcode[2] |= 4;

    if (args[1].as.mem.has_index) {
      opcode[len] |= args[1].as.mem.scale << 6;
      opcode[len] |= (args[1].as.mem.index % EsmRegKindR8) << 3;
    } else {
      opcode[len] |= 4 << 3;
    }
    opcode[len] |= args[1].as.mem.base.as.reg % EsmRegKindR8;

    ++len;
  } else {
    if (args[1].as.mem.base.as.reg >= EsmRegKindR8)
      opcode[0] |= 1;

    opcode[2] |= args[1].as.mem.base.as.reg % EsmRegKindR8;
  }

  if (args[1].as.mem.displacement != 0) {
    if (args[1].as.mem.displacement >= -128 &&
        args[1].as.mem.displacement <= 127) {
      opcode[2] |= 1 << 6;

      opcode[len++] = ((u8 *) &args[1].as.mem.displacement)[0];
    } else {
      opcode[2] |= 2 << 6;

      for (u32 i = 0; i < 4; ++i)
        opcode[len++] = ((u8 *) &args[1].as.mem.displacement)[i];
    }
  } else if (!has_sib &&
             (args[1].as.mem.base.as.reg == EsmRegKindRBP ||
              args[1].as.mem.base.as.reg == EsmRegKindR13)) {
      opcode[2] |= 1 << 6;

      opcode[len++] = 0;
  }

  fwrite(opcode, 1, len, stream);

  return true;
}

InstrMapping mappings_x86_64[] = {
  {
    EsmInstrKind_mov,
    2,
    { EsmInstrArgKindReg, EsmInstrArgKindImm },
    { 8, 8 },
    mov_r64i64,
  },
  {
    EsmInstrKind_syscall,
    0,
    {},
    {},
    syscall,
  },
  {
    EsmInstrKind_lea,
    2,
    { EsmInstrArgKindReg, EsmInstrArgKindMem },
    { 8, 8 },
    lea_r64m64,
  },
};

u32 mappings_len_x86_64 = ARRAY_LEN(mappings_x86_64);
