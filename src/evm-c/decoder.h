#ifndef DECODER_H
#define DECODER_H

#include "ir.h"
#include "arena.h"

bool decode_ir(Ir *ir, Arena *arena, u8 *data, u32 data_len);

#endif // DECODER_H
