#include "assembler.h"

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
};

u32 mappings_len_x86_64 = ARRAY_LEN(mappings_x86_64);
