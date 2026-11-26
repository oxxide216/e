#ifndef EVM_IR_H
#define EVM_IR_H

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
  InstrKindJumpIf,
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
  InstrKindStoreUninit,
  InstrKindNot,
} InstrKind;

// TODO: specify ValueKind in alloc
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
  u32       dest_index;
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
} InstrJumpIf;

typedef struct {
  u32 cond_index;
  u32 target;
} InstrJumpIfNot;

typedef struct {
  u32 dest_index;
  u32 src_index;
} InstrRef;

typedef struct {
  u32  dest_index;
  u32  dest_offset_index;
  u32  src_index;
  bool deref; // Basically true for pointers and false for arrays
} InstrCopyToRef;

typedef struct {
  u32       dest_index;
  u32       src_index;
  u32       src_offset_index;
  ValueKind src_target_kind;
  bool      deref; // Basically true for pointers and false for arrays
  bool      take_ref;
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
  i32      dest_offset;
  u32      src_index;
  bool     deref; // Basically true for pointers and false for structures
} InstrCopyToRefFixed;

typedef struct {
  u32       dest_index;
  u32       src_index;
  i32       src_offset;
  ValueKind src_target_kind;
  bool      deref; // Basically true for pointers and false for structures
  bool      take_ref;
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
  ValueKind return_kind;
  u32       index;
  Indices   arg_indices;
} InstrCallRefAssign;

typedef struct {
  u32 index;
  u32 uninit_index;
} InstrStoreUninit;

typedef struct {
  u32 dest_index;
  u32 src_index;
} InstrNot;

typedef union {
  InstrAlloc            alloc;
  InstrStore            store;
  InstrCopy             copy;
  InstrBinOp            bin_op;
  InstrCall             call;
  InstrCallAssign       call_assign;
  InstrRetVal           ret_val;
  InstrJump             jump;
  InstrJumpIf           jump_if;
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
  InstrStoreUninit      store_uninit;
  InstrNot              not;
} InstrAs;

typedef struct {
  InstrKind kind;
  InstrAs   as;
  bool      has_label;
} Instr;

typedef Da(Instr) Instrs;

typedef struct {
  ValueKind kind;
  u32       size;
} Arg;

typedef Da(Arg) Args;

typedef struct {
  Str       name;
  Args      args;
  ValueKind return_kind;
  u32       return_size;
  Instrs    instrs;
  Instrs    last_instrs;
} Proc;

typedef Da(Proc) Procs;

typedef struct {
  u8  *data;
  u32  len;
} DataEntry;

typedef Da(DataEntry) Data;

typedef struct {
  u32 size;
} Uninit;

typedef Da(Uninit) Uninits;

typedef Da(Str) Imports;

typedef struct {
  Procs   procs;
  Data    data;
  Uninits uninits;
  Imports imports;
} Ir;

typedef enum {
  Arch_X86_64 = 0,
} Arch;

typedef struct {
  Arch arch;
  u32  size;
  u32  max_field_size;
} StructSizeCalc;

static inline u32 alignment_func(Arch arch, u32 size) {
  switch (arch) {
  case Arch_X86_64: {
    if (size <= 8)
      return size;
    return 8;
  }
  }

  return 0;
}

static inline i32 align(i32 value, i32 base) {
  if (value % base == 0)
    return value;
  return value + base - value % base;
}

static inline StructSizeCalc ssc_make(Arch arch) {
  StructSizeCalc result = {0};
  result.arch = arch;
  return result;
}

static inline u32 ssc_append_field(StructSizeCalc *ssc, u32 size) {
  if (ssc->max_field_size < size)
    ssc->max_field_size = size;

  u32 offset = align(ssc->size, alignment_func(ssc->arch, size));
  ssc->size = offset + size;

  return offset;
}

static inline u32 ssc_get_result(StructSizeCalc *ssc) {
  return align(ssc->size, alignment_func(ssc->arch, ssc->max_field_size));
}

#endif // EVM_IR_H
