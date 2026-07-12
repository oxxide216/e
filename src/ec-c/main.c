#include "shl/shl-defs.h"
#include "shl/shl-log.h"
#include "io.h"
#include "parser.h"
#include "arena.h"
#include "deps.h"
#include "checker.h"
#include "evm-encoder.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"

#define EVM_PREFIX  "./evm-c -o "
#define YASM_PREFIX "yasm -felf64 -o "
#define LD_PREFIX   "ld -o "

typedef struct {
  char *input_path;
  char *output_path;
  char *cache_path;
  char *ir_path;
  char *asm_path;
  char *obj_path;
  bool  is_output_path_malloced;
  bool  is_obj_path_malloced;
  bool  link_only;
} Config;

static void print_usage(char *program_name) {
  fprintf(stderr, "Usage: %s <options...> <input file>\n\n", program_name);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "       -o <output file>          Specify output file\n");
  fprintf(stderr, "       -c <cache path>           Specify IR cache path\n");}


static char *make_ir_path(char *input_path) {
  u32 len = strlen(input_path);
  u32 begin = len;
  while (begin > 0 && input_path[begin - 1] != '/')
    --begin;

  input_path += begin;
  len -= begin;

  if (len > 2 && strcmp(input_path + len - 2, ".e") == 0) {
    char *result = malloc(5 + len - 2 + 4 + 1);
    strcpy(result, "/tmp/");
    memcpy(result + 5, input_path, len - 2);
    strcpy(result + 5 + len - 2, ".eir");
    result[5 + len - 2 + 4] = '\0';
    return result;
  } else {
    char *result = malloc(5 + len + 5 + 1);
    strcpy(result, "/tmp/");
    strcpy(result + 5, input_path);
    strcpy(result + 5 + len, ".eir");
    result[5 + len + 5] = '\0';
    return result;
  }
}

static char *make_output_path(char *input_path) {
  u32 len = strlen(input_path);
  u32 begin = len;
  while (begin > 0 && input_path[begin - 1] != '/')
    --begin;

  input_path += begin;
  len -= begin;

  if (len > 2 && strcmp(input_path + len - 2, ".e") == 0) {
    char *result = malloc(len - 2 + 1);
    memcpy(result, input_path, len - 2);
    result[len - 2] = '\0';
    return result;
  } else {
    char *result = malloc(len + 1);
    memcpy(result, input_path, len);
    result[len] = '\0';
    return result;
  }
}

static char *make_asm_path(char *ir_path) {
  u32 len = strlen(ir_path);
  char *result = malloc(len - 4 + 2 + 1);
  memcpy(result, ir_path, len - 4);
  strcpy(result + len - 4, ".s");
  result[len - 4 + 2] = '\0';
  return result;
}

static char *make_obj_path(char *asm_path) {
  u32 len = strlen(asm_path);
  char *result = malloc(len + 1);
  memcpy(result, asm_path, len - 1);
  result[len - 1] = 'o';
  result[len] = '\0';
  return result;
}

static Config config_create(i32 argc, char **argv) {
  Config config = {0};

  for (u32 i = 1; i < (u32) argc; ++i) {
    if (strcmp(argv[i], "-o") == 0) {
      if (i + 1 == (u32) argc) {
        print_usage(argv[0]);
        ERROR("Option %s requires an argument\n", argv[i]);
        exit(1);
      }

      config.output_path = argv[++i];
    } else if (strcmp(argv[i], "-c") == 0) {
      if (i + 1 == (u32) argc) {
        print_usage(argv[0]);
        ERROR("Option %s requires an argument\n", argv[i]);
        exit(1);
      }

      config.cache_path = argv[++i];
    } else if (argv[i][0] == '-') {
      print_usage(argv[0]);
      ERROR("Unknown option: %s\n", argv[i]);
      exit(1);
    } else {
      if (config.input_path) {
        print_usage(argv[0]);
        ERROR("More than one input file was provided\n");
        exit(1);
      }

      config.input_path = argv[i];
    }
  }

  if (!config.input_path) {
    print_usage(argv[0]);
    ERROR("Input file was not provided\n");
    exit(1);
  }

  if (!config.output_path) {
    config.output_path = make_output_path(config.input_path);
    config.is_output_path_malloced = true;
  }

  config.ir_path = make_ir_path(config.input_path);
  config.asm_path = make_asm_path(config.ir_path);
  u32 output_path_len = strlen(config.output_path);
  if (output_path_len > 2 &&
      strcmp(config.output_path + output_path_len - 2, ".o") == 0) {
    config.link_only = true;
    config.obj_path = config.output_path;
  } else {
    config.obj_path = make_obj_path(config.asm_path);
    config.is_obj_path_malloced = true;
  }

  return config;
}

static void config_destroy(Config *config) {
  if (config->is_output_path_malloced)
    free(config->output_path);
  free(config->ir_path);
  free(config->asm_path);
  if (config->is_obj_path_malloced)
    free(config->obj_path);
}

static void ir_destroy(EIr *ir) {
  for (u32 i = 0; i < ir->procs.len; ++i) {
    EProc *proc = ir->procs.items + i;

    for (u32 j = 0; j < proc->args.len; ++j)
      if (proc->args.items[j].type.kind == ETypeKindPtr)
        type_free(proc->args.items[j].type.ptr_target);
    if (proc->args.items)
      free(proc->args.items);

    if (proc->return_type.kind == ETypeKindPtr)
      type_free(proc->return_type.ptr_target);

    if (proc->instrs.items)
      free(proc->instrs.items);
  }
  if (ir->procs.items)
    free(ir->procs.items);

  for (u32 i = 0; i < ir->structs.len; ++i) {
    EStruct *_struct = ir->structs.items + i;

    for (u32 j = 0; j < _struct->fields.len; ++j)
      if (_struct->fields.items[j].type.kind == ETypeKindPtr)
        type_free(_struct->fields.items[j].type.ptr_target);
    if (_struct->fields.items)
      free(_struct->fields.items);
  }
  if (ir->structs.items)
    free(ir->structs.items);

  if (ir->consts.items)
    free(ir->consts.items);

  for (u32 i = 0; i < ir->data.len; ++i) {
    EDataEntry *entry = ir->data.items + i;

    if (entry->data)
      free(entry->data);
  }
  if (ir->data.items)
    free(ir->data.items);
}

i32 main(i32 argc, char **argv) {
  Config config = config_create(argc, argv);

  Str code = read_file(config.input_path);
  if (code.len == (u32) -1) {
    ERROR("Could not read %s\n", config.input_path);
    config_destroy(&config);
    return 1;
  }

  EIr ir = {0};
  Varss varss = {0};
  Str input_path_str = str_new(config.input_path);
  if (!parse(&ir, &varss, code, input_path_str)) {
    varss_destroy(&varss);
    ir_destroy(&ir);
    free(code.ptr);
    config_destroy(&config);
    return 1;
  }

  Arena arena = {0};
  Str cache_path_str = { NULL, (u32) -1 };
  if (config.cache_path)
    cache_path_str = str_new(config.cache_path);

  if (!load_module_deps(&ir, &arena, input_path_str, cache_path_str)) {
    arena_free(&arena);
    varss_destroy(&varss);
    ir_destroy(&ir);
    free(code.ptr);
    config_destroy(&config);
    return 1;
  }

  if (!check_ir(&ir, &varss)) {
    arena_free(&arena);
    varss_destroy(&varss);
    ir_destroy(&ir);
    free(code.ptr);
    config_destroy(&config);
    return 1;
  }

  remove(config.ir_path);
  FILE *ir_file = fopen(config.ir_path, "wb");
  if (!ir_file) {
    ERROR("Could not write %s\n", config.ir_path);
    arena_free(&arena);
    varss_destroy(&varss);
    ir_destroy(&ir);
    free(code.ptr);
    config_destroy(&config);
    return 1;
  }

  encode_ir_as_evm_ir(ir_file, &ir, &varss);

  fclose(ir_file);

  i32 result = 0;
  StringBuilder sb = {0};

  sb_push(&sb, EVM_PREFIX);
  sb_push(&sb, config.asm_path);
  sb_push_char(&sb, ' ');
  sb_push(&sb, config.ir_path);
  sb_push_char(&sb, '\0');

  if (system(sb.buffer) != 0) {
    result = 1;
    goto end;
  }

  sb.len = 0;

  sb_push(&sb, YASM_PREFIX);
  sb_push(&sb, config.obj_path);
  sb_push_char(&sb, ' ');
  sb_push(&sb, config.asm_path);
  sb_push_char(&sb, '\0');

  if (system(sb.buffer) != 0) {
    result = 1;
    goto end;
  }

  if (!config.link_only) {
    sb.len = 0;

    sb_push(&sb, LD_PREFIX);
    sb_push(&sb, config.output_path);
    sb_push_char(&sb, ' ');
    sb_push(&sb, config.obj_path);
    sb_push_char(&sb, '\0');

    if (system(sb.buffer) != 0) {
      result = 1;
      goto end;
    }
  }

end:
  free(sb.buffer);
  arena_free(&arena);
  varss_destroy(&varss);
  ir_destroy(&ir);
  free(code.ptr);
  config_destroy(&config);

  return result;
}
