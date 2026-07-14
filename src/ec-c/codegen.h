#ifndef CODEGEN_H
#define CODEGEN_H

#include "shl/shl-defs.h"
#define MATER_COMPILER
#include "ir.h"

#define emit_proc(procs, name)         \
  do {                                 \
    EProc proc = { name, {}, {}, {} }; \
    DA_APPEND(*(procs), proc);         \
  } while (0)

#define emit_arg(procs, name, type)                         \
  do {                                                      \
    EArg arg = { name, type };                              \
    DA_APPEND((procs)->items[(procs)->len - 1].args, arg);  \
  } while (0)

#define emit_return_type(procs, type)                    \
  do {                                                   \
    (procs)->items[(procs)->len - 1].return_type = type; \
  } while (0)

#define emit_instr(procs, token, kind, ...)                    \
  do {                                                         \
    EInstr instr = {                                           \
      kind,                                                    \
      { __VA_ARGS__ },                                         \
      { token.file_path, token.row, token.col },               \
    };                                                         \
    DA_APPEND((procs)->items[(procs)->len - 1].instrs, instr); \
  } while (0)

#define emit_struct(structs, name)  \
  do {                              \
    EStruct _struct = { name, {} }; \
    DA_APPEND(*(structs), _struct); \
  } while (0)

#define emit_field(structs, name, type)                            \
  do {                                                             \
    EField field = { name, type };                                 \
    DA_APPEND((structs)->items[(structs)->len - 1].fields, field); \
  } while (0)

#define emit_const(consts, name, value) \
  do {                                  \
    EConst _const = { name, value };    \
    DA_APPEND(*(consts), _const);       \
  } while (0)

#define emit_data(data, entry_data, len)    \
  do {                                      \
    EDataEntry entry = { entry_data, len }; \
    DA_APPEND(*(data), entry);              \
  } while (0)

#define emit_module_dep(module_deps, name) \
  do {                                     \
    EModuleDep dep = { name, {}, {} };     \
    DA_APPEND(*(module_deps), dep);        \
  } while (0)

#endif // CODEGEN_H
