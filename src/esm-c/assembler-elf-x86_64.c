// Information on ELF format was taken from:
// - https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
// - https://gist.github.com/x0nu11byt3/bcb35c3de461e5fb66173071a2379779
//
// Information on X86_64 instruction set was taken from:
// - https://www.felixcloutier.com/x86

#include <elf.h>
#include <stddef.h>

#include "assembler.h"
#include "shl/shl-log.h"

typedef struct {
  u8  e_ident[EI_NIDENT];
  u16 e_type;
  u16 e_machine;
  u32 e_version;
  u64 e_entry;
  u64 e_phoff;
  u64 e_shoff;
  u32 e_flags;
  u16 e_ehsize;
  u16 e_phentsize;
  u16 e_phnum;
  u16 e_shentsize;
  u16 e_shnum;
  u16 e_shstrndx;
} __attribute__((packed)) ExecutableHeader;

typedef struct {
  u32 sh_name;
  u32 sh_type;
  u64 sh_flags;
  u64 sh_addr;
  u64 sh_offset;
  u64 sh_size;
  u32 sh_link;
  u32 sh_info;
  u64 sh_addralign;
  u64 sh_entsize;
} __attribute__((packed)) SectionHeader;

typedef struct {
  u32 st_name;
  u8  st_info;
  u8  st_other;
  u16 st_shndx;
  u64 st_value;
  u64 st_size;
} __attribute__((packed)) Symbol;

extern InstrMapping mappings_x86_64[];
extern u32 mappings_len_x86_64;

static u32 section_kind_to_sh_type(EsmSectionKind kind) {
  switch (kind) {
  case EsmSectionKindText:   return SHT_PROGBITS;
  case EsmSectionKindData:   return SHT_PROGBITS;
  case EsmSectionKindsCount: return 0;
  }

  return 0;
}

static u64 section_kind_to_sh_flags(EsmSectionKind kind) {
  switch (kind) {
  case EsmSectionKindText:   return SHF_EXECINSTR | SHF_ALLOC;
  case EsmSectionKindData:   return 0;
  case EsmSectionKindsCount: return 0;
  }

  return 0;
}

static char *section_kind_to_name(EsmSectionKind kind) {
  switch (kind) {
  case EsmSectionKindText:   return ".text";
  case EsmSectionKindData:   return ".data";
  case EsmSectionKindsCount: return NULL;
  }

  return NULL;
}

bool esm_assemble_elf_x86_64(FILE *stream, char *file_path, EsmIr *ir) {
  ExecutableHeader eh = {
    .e_ident = {
      [EI_MAG0] = 0x7F,
      [EI_MAG1] = 'E',
      [EI_MAG2] = 'L',
      [EI_MAG3] = 'F',
      [EI_CLASS] = 2, // 64 bit
      [EI_DATA] = 1, // LSB
      [EI_VERSION] = EV_CURRENT,
      [EI_OSABI] = 0x00, // System V ABI
      [EI_ABIVERSION] = 0, // Ignored
    },
    .e_type = ET_REL,
    .e_machine = EM_X86_64,
    .e_version = EV_CURRENT,
    .e_entry = 0,
    .e_phoff = 0,
    .e_shoff = 0,
    .e_flags = 0,
    .e_ehsize = sizeof(ExecutableHeader),
    .e_phentsize = 0,
    .e_phnum = 0,
    .e_shentsize = sizeof(SectionHeader),
    .e_shnum = 3,
    .e_shstrndx = 1,
  };

  for (u32 i = 0; i < EsmSectionKindsCount; ++i)
    if (ir->sections[i].entries.len > 0)
      ++eh.e_shnum;

  fwrite(&eh, sizeof(eh), 1, stream);

  u64 section_offsets[2 + EsmSectionKindsCount];
  u64 section_sizes[2 + EsmSectionKindsCount] = {0};

  u32 names_len = eh.e_shnum - 1 + ir->exports.len;
  u64 *name_offsets = alloca(names_len * sizeof(*name_offsets));
  memset(name_offsets, 0, names_len * sizeof(*name_offsets));

  section_offsets[0] = ftell(stream);

  char zero = '\0';
  fwrite(&zero, 1, 1, stream);
  for (u32 i = 0; i < names_len; ++i)
    ++name_offsets[i];

  char *name = ".shstrtab";
  u32 len = strlen(name) + 1;
  fwrite(name, 1, len, stream);
  for (u32 i = 1; i < names_len; ++i)
    name_offsets[i] += len;

  name = ".symtab";
  len = strlen(name) + 1;
  fwrite(name, 1, len, stream);
  for (u32 i = 2; i < names_len; ++i)
    name_offsets[i] += len;

  u32 name_index = 1;
  for (u32 i = 0; i < EsmSectionKindsCount; ++i) {
    if (ir->sections[i].entries.len > 0) {
      char *name = section_kind_to_name(i);
      u32 len = strlen(name) + 1;
      fwrite(name, 1, len, stream);
      for (u32 j = name_index++ + 2; j < names_len; ++j)
        name_offsets[j] += len;
    }
  }

  for (u32 i = 0; i < ir->exports.len; ++i) {
    Str name = ir->exports.items[i].name;
    char zero = '\0';

    fwrite(name.ptr, 1, name.len, stream);
    fwrite(&zero, 1, 1, stream);

    for (u32 j = i + eh.e_shnum; j < names_len; ++j)
      name_offsets[j] += name.len + 1;
  }

  section_sizes[0] = ftell(stream) - section_offsets[0];

  for (u32 i = 0; i < EsmSectionKindsCount; ++i) {
    section_offsets[i + 2] = ftell(stream);

    for (u32 j = 0; j < ir->sections[i].entries.len; ++j) {
      EsmEntry *entry = ir->sections[i].entries.items + j;

      switch (entry->kind) {
      case EsmEntryKindInstr: {
        InstrMapping *mapping = NULL;

        for (u32 k = 0; k < mappings_len_x86_64; ++k) {
          InstrMapping *_mapping = mappings_x86_64 + k;

          if (_mapping->instr_kind == entry->as.instr.kind &&
              _mapping->args_len == entry->as.instr.args_len) {
            bool all = true;

            for (u32 l = 0; l < entry->as.instr.args_len; ++l) {
              if (_mapping->arg_kinds[l] != entry->as.instr.args[l].kind) {
                all = false;
                break;
              }

              if (entry->as.instr.args[l].kind == EsmInstrArgKindReg) {
                if (_mapping->arg_sizes[l] != entry->as.instr.args[l].as.reg.size) {
                  all = false;
                  break;
                }
              } else if (entry->as.instr.args[l].kind == EsmInstrArgKindMem) {
                if (entry->as.instr.args[l].as.mem.size != 0 &&
                    _mapping->arg_sizes[l] != entry->as.instr.args[l].as.mem.size) {
                  all = false;
                  break;
                }
              }
            }

            if (all) {
              mapping = _mapping;
              break;
            }
          }
        }

        if (!mapping ||
            !mapping->gen(stream, entry->as.instr.args)) {
          ERROR("%s:%u: invalid combination of mnemonic and operands\n", file_path, entry->row);
          return false;
        }
      } break;

      case EsmEntryKindData: {
        ERROR("TODO: codegen for EsmEntryKindData\n");
        exit(1);
      } break;
      }

      for (u32 k = 0; k < ir->sections[i].labels.len; ++k)
        if (ir->sections[i].labels.items[k].entry_index == j)
          ir->sections[i].labels.items[k].offset =
            ftell(stream) - section_offsets[i + 2];
    }

    section_sizes[i + 2] = ftell(stream) - section_offsets[i + 2];
  }

  section_offsets[1] = ftell(stream);

  Symbol symbol = {0};
  fwrite(&symbol, sizeof(symbol), 1, stream);

  for (u32 i = 0; i < ir->exports.len; ++i) {
    u32 index = ir->exports.items[i].section_index;
    for (u32 j = 0; j < ir->sections[index].labels.len; ++j)
      if (str_eq(ir->sections[index].labels.items[j].name,
                 ir->exports.items[i].name))
        ir->exports.items[i].section_offset =
          ir->sections[index].labels.items[j].offset;

    Symbol symbol = {
      .st_name = name_offsets[eh.e_shnum + i - 1],
      .st_info = ELF64_ST_INFO(STB_GLOBAL, STT_NOTYPE),
      .st_other = STV_DEFAULT,
      .st_shndx = ir->exports.items[i].section_index + 3,
      .st_value = ir->exports.items[i].section_offset,
      .st_size = 0,
    };
    fwrite(&symbol, sizeof(symbol), 1, stream);
  }

  section_sizes[1] = ftell(stream) - section_offsets[1];

  eh.e_shoff = ftell(stream);
  fseek(stream, offsetof(ExecutableHeader, e_shoff), SEEK_SET);
  fwrite(&eh.e_shoff, sizeof(eh.e_shoff), 1, stream);
  fseek(stream, 0, SEEK_END);

  SectionHeader sh = {0};
  sh.sh_type = SHT_NULL;
  fwrite(&sh, sizeof(sh), 1, stream);

  sh = (SectionHeader) {
    .sh_name = name_offsets[0],
    .sh_type = SHT_STRTAB,
    .sh_flags = 0,
    .sh_addr = 0,
    .sh_offset = section_offsets[0],
    .sh_size = section_sizes[0],
    .sh_link = 0,
    .sh_info = 0,
    .sh_addralign = 1,
    .sh_entsize = 0,
  };
  fwrite(&sh, sizeof(sh), 1, stream);

  sh = (SectionHeader) {
    .sh_name = name_offsets[1],
    .sh_type = SHT_SYMTAB,
    .sh_flags = 0,
    .sh_addr = 0,
    .sh_offset = section_offsets[1],
    .sh_size = section_sizes[1],
    .sh_link = 1,
    .sh_info = 1,
    .sh_addralign = 1,
    .sh_entsize = sizeof(Symbol),
  };
  fwrite(&sh, sizeof(sh), 1, stream);

  name_index = 2;
  for (u32 i = 0; i < EsmSectionKindsCount; ++i) {
    if (ir->sections[i].entries.len > 0) {
      SectionHeader sh = {
        .sh_name = name_offsets[name_index++],
        .sh_type = section_kind_to_sh_type(i),
        .sh_flags = section_kind_to_sh_flags(i),
        .sh_addr = 0,
        .sh_offset = section_offsets[i + 2],
        .sh_size = section_sizes[i + 2],
        .sh_link = 0,
        .sh_info = 0,
        .sh_addralign = 1,
        .sh_entsize = 0,
      };
      fwrite(&sh, sizeof(sh), 1, stream);
    }
  }

  return true;
}
