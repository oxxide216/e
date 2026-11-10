#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "ir.h"

typedef bool (*InstrGenFunc)(FILE *stream, EsmInstrArg *args);

typedef struct {
  EsmInstrKind    instr_kind;
  u8              args_len;
  EsmInstrArgKind arg_kinds[3];
  u8              arg_sizes[3];
  InstrGenFunc    gen;
} InstrMapping;

bool esm_assemble_elf_x86_64(FILE *stream, char *file_path, EsmIr *ir);

#endif // ASSEMBLER_H
