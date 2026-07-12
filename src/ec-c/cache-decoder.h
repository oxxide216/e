#ifndef CACHE_DECODER_H
#define CACHE_DECODER_H

#include "eir.h"
#include "arena.h"

bool decode_cache(EIr *ir, Arena *arena, u8 *cache, u32 len);

#endif // CACHE_DECODER_H
