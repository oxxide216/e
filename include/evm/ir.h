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
  InstrKindInlineAsm,
  InstrKindStoreData,
  InstrKindConvert,
  InstrKindCopyToRefFixed,
  InstrKindCopyFromRefFixed,
  InstrKindRefProc,
  InstrKindCallRef,
  InstrKindCallRefAssign,
} InstrKind;

typedef struct {
  i32 offset;
  u32 size;
} AlignedSegment;

typedef Da(AlignedSegment) Segments;

typedef struct {
  u32      index;
  Segments segments;
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
  u32       dest_index;
  u32       return_size;
  ValueKind return_kind;
  Str       name;
  Indices   arg_indices;
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
  u32 dest_offset_index;
  u32 src_index;
} InstrCopyToRef;

typedef struct {
  u32       dest_index;
  u32       src_index;
  u32       src_offset_index;
  ValueKind src_target_kind;
  u32       src_target_size;
} InstrCopyFromRef;

typedef enum {
  AsmSegmentKindStr = 0,
  AsmSegmentKindVar,
} AsmSegmentKind;

typedef struct {
  AsmSegmentKind kind;
  Str            value;
  u32            index;
} AsmSegment;

typedef Da(AsmSegment) AsmSegments;

typedef struct {
  AsmSegments segments;
} InstrInlineAsm;

typedef struct {
  u32 index;
  u32 data_index;
} InstrStoreData;

typedef struct {
  u32       dest_index;
  ValueKind dest_kind;
  u32       dest_size;
  u32       src_index;
} InstrConvert;

typedef struct {
  u32      dest_index;
  Segments dest_segments;
  u32      src_index;
} InstrCopyToRefFixed;

typedef struct {
  u32       dest_index;
  u32       src_index;
  Segments  src_segments;
  ValueKind src_target_kind;
  u32       src_target_size;
} InstrCopyFromRefFixed;

typedef struct {
  u32 dest_index;
  Str proc_name;
} InstrRefProc;

typedef struct {
  u32     index;
  Indices arg_indices;
} InstrCallRef;

typedef struct {
  u32       dest_index;
  u32       return_size;
  ValueKind return_kind;
  u32       index;
  Indices   arg_indices;
} InstrCallRefAssign;

typedef union {
  InstrAlloc            alloc;
  InstrStore            store;
  InstrCopy             copy;
  InstrBinOp            bin_op;
  InstrCall             call;
  InstrCallAssign       call_assign;
  InstrRetVal           ret_val;
  InstrJump             jump;
  InstrJumpIfNot        jump_if_not;
  InstrRef              ref;
  InstrCopyToRef        copy_to_ref;
  InstrCopyFromRef      copy_from_ref;
  InstrInlineAsm        inline_asm;
  InstrStoreData        store_data;
  InstrConvert          convert;
  InstrCopyToRefFixed   copy_to_ref_fixed;
  InstrCopyFromRefFixed copy_from_ref_fixed;
  InstrRefProc          ref_proc;
  InstrCallRef          call_ref;
  InstrCallRefAssign    call_ref_assign;
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
  u8  *data;
  u32  len;
} DataEntry;

typedef Da(DataEntry) Data;

typedef Da(Str) Imports;

typedef struct {
  Procs   procs;
  Data    data;
  Imports imports;
} Ir;

#endif // IR_H
