#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "ir.h"

bool esm_assemble_elf_x86_64(FILE *stream, EsmIr *ir);

#endif // ASSEMBLER_H
