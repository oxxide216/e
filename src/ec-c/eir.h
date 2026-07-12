#ifndef EIR_H
#define EIR_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"
#include "ir.h"

typedef enum {
  ETypeKindUnit = 0,
  ETypeKindS8,
  ETypeKindS16,
  ETypeKindS32,
  ETypeKindS64,
  ETypeKindU8,
  ETypeKindU16,
  ETypeKindU32,
  ETypeKindU64,
  ETypeKindBool,
  ETypeKindStruct,
  ETypeKindPtr,
} ETypeKind;

typedef struct {
  Str file_path;
  u32 row, col;
} ETypeLoc;

typedef struct EType EType;

struct EType {
  ETypeKind  kind;
  Str        name;
  EType     *ptr_target;
  ETypeLoc   loc;
};

typedef Da(EType) ETypes;

typedef union {
  i64  _signed;
  u64  _unsigned;
  bool _bool;
} EValueAs;

typedef struct {
  ETypeKind kind;
  EValueAs  as;
} EValue;

typedef enum {
  EInstrKindAlloc = 0,
  EInstrKindStore,
  EInstrKindCopy,
  EInstrKindBinOp,
  EInstrKindCall,
  EInstrKindCallAssign,
  EInstrKindRet,
  EInstrKindRetVal,
  EInstrKindJump,
  EInstrKindJumpIfNot,
  EInstrKindRef,
  EInstrKindCopyToRef,
  EInstrKindCopyFromRef,
  EInstrKindStoreNull,
  EInstrKindInlineAsm,
  EInstrKindStoreData,
  EInstrKindCast,
} EInstrKind;

typedef struct {
  Str name;
  u32 index;
} EInstrAlloc;

typedef struct {
  Str    name;
  u32    index;
  EValue value;
} EInstrStore;

typedef struct {
  Str dest_name;
  u32 dest_index;
  Str src_name;
  u32 src_index;
} EInstrCopy;

typedef enum {
  EBinOpKindAdd = 0,
  EBinOpKindSub,
  EBinOpKindMul,
  EBinOpKindDiv,
  EBinOpKindRem,
  EBinOpKindAnd,
  EBinOpKindOr,
  EBinOpKindXor,
  EBinOpKindLShift,
  EBinOpKindRShift,
  EBinOpKindEq,
  EBinOpKindNe,
  EBinOpKindLs,
  EBinOpKindLe,
  EBinOpKindGt,
  EBinOpKindGe,
} EBinOpKind;

typedef struct {
  Str        dest_name;
  u32        dest_index;
  Str        src0_name;
  u32        src0_index;
  Str        src1_name;
  u32        src1_index;
  EBinOpKind kind;
} EInstrBinOp;

typedef struct {
  Str     name;
  Indices arg_indices;
} EInstrCall;

typedef struct {
  Str     dest_name;
  u32     dest_index;
  Str     name;
  Indices arg_indices;
} EInstrCallAssign;

typedef struct {
  u32 index;
} EInstrRetVal;

typedef struct {
  u32 target;
} EInstrJump;

typedef struct {
  u32 cond_index;
  u32 target;
} EInstrJumpIfNot;

typedef struct {
  Str dest_name;
  u32 dest_index;
  Str src_name;
  u32 src_index;
} EInstrRef;

typedef struct {
  Str dest_name;
  u32 dest_index;
  u32 dest_offset;
  Str src_name;
  u32 src_index;
} EInstrCopyToRef;

typedef struct {
  Str dest_name;
  u32 dest_index;
  Str src_name;
  u32 src_index;
  u32 src_offset;
} EInstrCopyFromRef;

typedef struct {
  Str name;
  u32 index;
} EInstrStoreNull;

typedef enum {
  EAsmSegmentKindStr = 0,
  EAsmSegmentKindVar,
} EAsmSegmentKind;

typedef struct {
  EAsmSegmentKind kind;
  Str             value;
  u32             value_index;
} EAsmSegment;

typedef Da(EAsmSegment) EAsmSegments;

typedef struct {
  EAsmSegments segments;
} EInstrInlineAsm;

typedef struct {
  Str name;
  u32 index;
  u32 data_index;
} EInstrStoreData;

typedef struct {
  Str   dest_name;
  u32   dest_index;
  EType dest_type;
  Str   src_name;
  u32   src_index;
} EInstrCast;

typedef union {
  EInstrAlloc          alloc;
  EInstrStore          store;
  EInstrCopy           copy;
  EInstrBinOp          bin_op;
  EInstrCall           call;
  EInstrCallAssign     call_assign;
  EInstrRetVal         ret_val;
  EInstrJump           jump;
  EInstrJumpIfNot      jump_if_not;
  EInstrRef            ref;
  EInstrCopyToRef      copy_to_ref;
  EInstrCopyFromRef    copy_from_ref;
  EInstrStoreNull      store_null;
  EInstrInlineAsm      inline_asm;
  EInstrStoreData      store_data;
  EInstrCast           cast;
} EInstrAs;

typedef ETypeLoc EInstrLoc;

typedef struct {
  EInstrKind kind;
  EInstrAs   as;
  EInstrLoc  loc;
} EInstr;

typedef Da(EInstr) EInstrs;

typedef struct {
  Str   name;
  EType type;
} EArg;

typedef Da(EArg) EArgs;

typedef struct {
  Str     name;
  EArgs   args;
  EType   return_type;
  EInstrs instrs;
} EProc;

typedef Da(EProc) EProcs;

typedef EArg EField;

typedef Da(EField) EFields;

typedef struct {
  Str     name;
  EFields fields;
} EStruct;

typedef Da(EStruct) EStructs;

typedef struct {
  Str    name;
  EValue value;
} EConst;

typedef Da(EConst) EConsts;

typedef struct {
  u8  *data;
  u32  len;
} EDataEntry;

typedef Da(EDataEntry) EData;

typedef Da(Str) EModulePath;

typedef struct EModuleDep EModuleDep;

typedef Da(EModuleDep) EModuleDeps;

typedef struct {
  EProcs      procs;
  EStructs    structs;
  EConsts     consts;
  EData       data;
  EModuleDeps module_deps;
} EIr;

struct EModuleDep {
  EModulePath  path;
  EIr          ir;
};

EType type_clone(EType *type);
bool type_eq(EType *a, EType *b);
void type_free(EType *type);

EStruct *get_struct(EStructs *structs, Str name);

#endif // EIR_H
