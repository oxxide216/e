#ifndef EIR_H
#define EIR_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"
#include "evm/ir.h"

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
  ETypeKindArray,
  ETypeKindTuple,
  ETypeKindStr,
  ETypeKindPtr,
} ETypeKind;

typedef struct {
  Str file_path;
  u32 row, col;
} ETypeLoc;

typedef struct EType EType;

typedef Da(EType) ETypes;
typedef Da(EType *) ETypeRefs;

typedef struct EStruct EStruct;

struct EType {
  ETypeKind    kind;
  union {
    struct {
      Str      name;
      EStruct *_struct;
    };
    EType     *ptr_target;
    struct {
      EType   *array_element;
      u32      array_len;
    };
    ETypes     tuple_types;
  };
  ETypeLoc     loc;
  u8          *moved_members;
  u32          moved_members_len;
  bool         is_implicit_ptr;
};

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
  EInstrKindStoreStr,
  EInstrKindCast,
  EInstrKindLenOf,
  EInstrKindCopyToField,
  EInstrKindCopyFromField,
  EInstrKindTuple,
  EInstrKindCopyToOffset,
  EInstrKindCopyFromOffset,
  EInstrKindSizeOf,
  EInstrKindStaticRef,
  EInstrKindCopyToStatic,
  EInstrKindCopyFromStatic,
} EInstrKind;

typedef struct {
  Str name;
  u32 index;
} EInstrAlloc;

typedef struct {
  Str       name;
  u32       index;
  EValue    value;
  bool      is_const;
  ETypeKind const_kind;
} EInstrStore;

typedef struct {
  Str  dest_name;
  u32  dest_index;
  Str  src_name;
  u32  src_index;
  bool is_explicit;
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

typedef struct EProc EProc;

typedef struct {
  Str      dest_name;
  u32      dest_index;
  Str      name;
  EProc   *callee;
  Indices  arg_indices;
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
  Str  dest_name;
  u32  dest_index;
  Str  src_name;
  u32  src_index;
  bool is_implicit_drop_arg;
} EInstrRef;

typedef struct {
  Str  dest_name;
  u32  dest_index;
  bool has_offset;
  Str  dest_offset_name;
  u32  dest_offset_index;
  Str  src_name;
  u32  src_index;
} EInstrCopyToRef;

typedef struct {
  Str  dest_name;
  u32  dest_index;
  Str  src_name;
  u32  src_index;
  bool has_offset;
  Str  src_offset_name;
  u32  src_offset_index;
  bool take_ref;
  bool is_ref_explicit;
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
} EInstrStoreStr;

typedef struct {
  Str   dest_name;
  u32   dest_index;
  EType dest_type;
  Str   src_name;
  u32   src_index;
} EInstrCast;

typedef struct {
  Str dest_name;
  u32 dest_index;
  Str src_name;
  u32 src_index;
} EInstrLenOf;

typedef struct {
  Str dest_name;
  u32 dest_index;
  Str dest_field_name;
  Str src_name;
  u32 src_index;
} EInstrCopyToField;

typedef struct {
  Str  dest_name;
  u32  dest_index;
  Str  src_name;
  u32  src_index;
  Str  src_field_name;
  bool take_ref;
  bool is_ref_explicit;
} EInstrCopyFromField;

typedef struct {
  Str     dest_name;
  u32     dest_index;
  Indices field_indices;
} EInstrTuple;

typedef struct {
  Str dest_name;
  u32 dest_index;
  u32 dest_offset;
  Str src_name;
  u32 src_index;
} EInstrCopyToOffset;

typedef struct {
  Str  dest_name;
  u32  dest_index;
  Str  src_name;
  u32  src_index;
  u32  src_offset;
  bool take_ref;
  bool is_ref_explicit;
} EInstrCopyFromOffset;

typedef struct {
  Str   dest_name;
  u32   dest_index;
  EType type;
} EInstrSizeOf;

typedef struct {
  u32 dest_index;
  Str static_name;
  u32 static_index;
} EInstrStaticRef;

typedef struct {
  u32 dest_index;
  u32 src_index;
} EInstrCopyToStatic;

typedef struct {
  Str dest_name;
  u32 dest_index;
  u32 src_index;
} EInstrCopyFromStatic;

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
  EInstrStoreStr       store_str;
  EInstrCast           cast;
  EInstrLenOf          len_of;
  EInstrCopyToField    copy_to_field;
  EInstrCopyFromField  copy_from_field;
  EInstrTuple          tuple;
  EInstrCopyToOffset   copy_to_offset;
  EInstrCopyFromOffset copy_from_offset;
  EInstrSizeOf         size_of;
  EInstrStaticRef      static_ref;
  EInstrCopyToStatic   copy_to_static;
  EInstrCopyFromStatic copy_from_static;
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

struct EProc {
  Str     name;
  EArgs   args;
  EType   return_type;
  EInstrs instrs;
};

typedef Da(EProc) EProcs;

typedef struct {
  Str   name;
  EType type;
} EField;

typedef Da(EField) EFields;

struct EStruct {
  Str     name;
  EFields fields;
};

typedef Da(EStruct) EStructs;

typedef struct {
  Str    name;
  EType  type;
  EValue value;
} EConst;

typedef Da(EConst) EConsts;

typedef struct {
  u8  *data;
  u32  len;
} EDataEntry;

typedef Da(EDataEntry) EData;

typedef Da(Str) EModulePath;
typedef Da(Str) EModuleImports;

typedef struct EModuleDep EModuleDep;

typedef Da(EModuleDep) EModuleDeps;

typedef struct {
  Str   name;
  EType type;
} EStaticVar;

typedef Da(EStaticVar) EStaticVars;

typedef struct {
  EProcs      procs;
  EStructs    structs;
  EConsts     consts;
  EData       data;
  EStaticVars static_vars;
  EModuleDeps module_deps;
  bool        is_copy;
} EIr;

typedef struct {
  Str        name;
  EType      type;
  bool       moved;
  EInstrLoc  moved_loc;
  u32        scope_end_instr;
  bool       returned;
} Var;

typedef Da(Var) Vars;
typedef Da(Vars) Varss;

struct EModuleDep {
  EModulePath    path;
  EModuleImports imports;
  EIr            ir;
  Varss          varss;
  bool           is_submodule;
};

EType type_clone(EType *type);
bool type_eq(EType *a, EType *b);
void type_free(EType *type);

u32 get_type_size(EIr *ir, EType *type);

EProc       *get_proc(EIr *ir, Str name, ETypeRefs *arg_types);
EModulePath  get_proc_path(EIr *ir, Str name,
                           Indices *arg_indices,
                           Vars *vars,
                           EModulePath *path);
EStruct     *get_struct(EIr *ir, Str name);
EIr         *get_struct_ir(EIr *ir, Str name);
EField      *get_field(EStruct *_struct, Str name);
EStaticVar  *get_static_var(EIr *ir, Str name);

void varss_destroy(Varss *varss);
bool proc_destroy(EIr *ir, Varss *varss, EProc *proc);
void ir_destroy(EIr *ir);

#endif // EIR_H
