#ifndef IR_H
#define IR_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"

typedef enum {
  ValueKindSigned = 0,
  ValueKindUnsigned,
} ValueKind;

typedef union {
  i64  _signed;
  u64  _unsigned;
} ValueAs;

typedef struct {
  ValueKind kind;
  ValueAs   as;
} Value;

typedef enum {
  InstrKindAlloc = 0,
  InstrKindStore,
  InstrKindCopy,
  InstrKindBinOp,
  InstrKindCall,
  InstrKindCallAssign,
  InstrKindRet,
  InstrKindRetVal,
  InstrKindJump,
  InstrKindJumpIfNot,
  InstrKindRef,
  InstrKindCopyToRef,
  InstrKindCopyFromRef,
} InstrKind;

typedef struct {
  u32 index;
  u32 size;
} InstrAlloc;

typedef struct {
  u32   index;
  Value value;
} InstrStore;

typedef struct {
  u32 dest_index;
  u32 src_index;
} InstrCopy;

typedef enum {
  BinOpKindAddInt = 0,
  BinOpKindSubInt,
  BinOpKindMulInt,
  BinOpKindDivInt,
  BinOpKindRem,
  BinOpKindAnd,
  BinOpKindOr,
  BinOpKindXor,
  BinOpKindLShift,
  BinOpKindRShift,
  BinOpKindEqInt,
  BinOpKindNeInt,
  BinOpKindLsInt,
  BinOpKindLeInt,
  BinOpKindGtInt,
  BinOpKindGeInt,
} BinOpKind;

typedef struct {
  u32       dest_index;
  u32       src0_index;
  u32       src1_index;
  BinOpKind kind;
} InstrBinOp;

typedef Da(u32) Indices;

typedef struct {
  Str     name;
  Indices arg_indices;
} InstrCall;

typedef struct {
  u32     dest_index;
  Str     name;
  Indices arg_indices;
} InstrCallAssign;

typedef struct {
  u32 index;
} InstrRetVal;

typedef struct {
  u32 target;
} InstrJump;

typedef struct {
  u32 cond_index;
  u32 target;
} InstrJumpIfNot;

typedef struct {
  u32 dest_index;
  u32 src_index;
} InstrRef;

typedef struct {
  u32 dest_index;
  u32 dest_offset;
  u32 src_index;
} InstrCopyToRef;

typedef struct {
  u32       dest_index;
  u32       src_index;
  u32       src_offset;
  ValueKind src_target_kind;
  u32       src_target_size;
} InstrCopyFromRef;

typedef union {
  InstrAlloc       alloc;
  InstrStore       store;
  InstrCopy        copy;
  InstrBinOp       bin_op;
  InstrCall        call;
  InstrCallAssign  call_assign;
  InstrRetVal      ret_val;
  InstrJump        jump;
  InstrJumpIfNot   jump_if_not;
  InstrRef         ref;
  InstrCopyToRef   copy_to_ref;
  InstrCopyFromRef copy_from_ref;
} InstrAs;

typedef struct {
  InstrKind kind;
  InstrAs   as;
} Instr;

typedef Da(Instr) Instrs;

typedef struct {
  u32       size;
  ValueKind kind;
} Arg;

typedef Da(Arg) Args;

typedef struct {
  Str       name;
  Args      args;
  u32       return_size;
  ValueKind return_kind;
  Instrs    instrs;
} Proc;

typedef Da(Proc) Procs;

typedef struct {
  Procs      procs;
} Ir;

#endif // IR_H
