#ifndef CACHE_ENCODER_H
#define CACHE_ENCODER_H

#include <stdio.h>

#include "eir.h"

void encode_cache(FILE *stream, EIr *ir, u64 code_hash);

#endif // CACHE_ENCODER_H
