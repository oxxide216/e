#ifndef EVM_ENCODER_H
#define EVM_ENCODER_H

#include <stdio.h>

#include "eir.h"
#include "parser.h"

void encode_ir_as_evm_ir(FILE *stream, EIr *ir, Varss *varss,
                         EModulePath *path,
                         bool include_deps, bool import_deps);

#endif // EVM_ENCODER_H
