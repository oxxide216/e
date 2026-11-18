#include "shl/shl-defs.h"
#include "shl/shl-log.h"
#include "io.h"
#include "parser.h"
#include "arena.h"
#include "deps.h"
#include "checker.h"
#include "evm-encoder.h"
#include "utils.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"
#ifndef NDEBUG
#include "eir-print.h"
#endif

#define EVM_PREFIX  "./evm -o "
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
  bool  run;
  bool  print_help;
  Strs  include_paths;
} Config;

static void print_usage(FILE *stream, char *program_name) {
  fprintf(stream, "Usage: %s <options...> <input file>\n\n", program_name);
  fprintf(stream, "Options:\n");
  fprintf(stream, "       -h --help                 Print this message and exit immediately\n");
  fprintf(stream, "       -o <output file>          Specify output file\n");
  fprintf(stream, "       -C <cache path>           Specify cache path (enables incremental compilation)\n");
  fprintf(stream, "       -I <search paths>         Specify included module search path\n");
  fprintf(stream, "       -r                        Immediately run compiled file\n");
}

static char *make_ir_path(char *input_path, bool add_prefix) {
  u32 len = strlen(input_path);
  u32 begin = len;
  while (begin > 0 && input_path[begin - 1] != '/')
    --begin;

  input_path += begin;
  len -= begin;

  u32 prefix_len = add_prefix ? 5 : 0;
  if (len > 2 && strcmp(input_path + len - 2, ".e") == 0) {
    char *result = malloc(prefix_len + len - 2 + 4 + 1);
    if (add_prefix)
    strcpy(result, "/tmp/");
    memcpy(result + prefix_len, input_path, len - 2);
    strcpy(result + prefix_len + len - 2, ".eir");
    result[prefix_len + len - 2 + 4] = '\0';
    return result;
  } else {
    char *result = malloc(prefix_len + len + 5 + 1);
    if (add_prefix)
      strcpy(result, "/tmp/");
    strcpy(result + prefix_len, input_path);
    strcpy(result + prefix_len + len, ".eir");
    result[prefix_len + len + 5] = '\0';
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
        print_usage(stderr, argv[0]);
        ERROR("Option %s requires an argument\n", argv[i]);
        exit(1);
      }

      config.output_path = argv[++i];
    } else if (strcmp(argv[i], "-C") == 0) {
      if (i + 1 == (u32) argc) {
        print_usage(stderr, argv[0]);
        ERROR("Option %s requires an argument\n", argv[i]);
        exit(1);
      }

      config.cache_path = argv[++i];
    } else if (strcmp(argv[i], "-I") == 0) {
      if (i + 1 == (u32) argc) {
        print_usage(stderr, argv[0]);
        ERROR("Option %s requires an argument\n", argv[i]);
        exit(1);
      }

      Str include_path = str_new(argv[++i]);
      DA_APPEND(config.include_paths, include_path);
    } else if (strcmp(argv[i], "-r") == 0) {
      config.run = true;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      config.print_help = true;
    } else if (argv[i][0] == '-') {
      print_usage(stderr, argv[0]);
      ERROR("Unknown option: %s\n", argv[i]);
      exit(1);
    } else {
      if (config.input_path) {
        print_usage(stderr, argv[0]);
        ERROR("More than one input file was provided\n");
        exit(1);
      }

      config.input_path = argv[i];
    }
  }

  if (config.print_help) {
    if (config.include_paths.items)
      free(config.include_paths.items);
    print_usage(stdout, argv[0]);
    exit(0);
  }

  if (!config.input_path) {
    print_usage(stderr, argv[0]);
    ERROR("Input file was not provided\n");
    exit(1);
  }

  if (!config.output_path) {
    config.output_path = make_output_path(config.input_path);
    config.is_output_path_malloced = true;
  }

  config.ir_path = make_ir_path(config.input_path, !config.cache_path);
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

#ifndef _WIN32
  DA_APPEND(config.include_paths, STR_LIT("/usr/include/e/libe"));
#endif

  return config;
}

static void config_destroy(Config *config) {
  if (config->is_output_path_malloced)
    free(config->output_path);
  free(config->ir_path);
  free(config->asm_path);
  if (config->is_obj_path_malloced)
    free(config->obj_path);
  if (config->include_paths.items)
    free(config->include_paths.items);
}

static bool compile_object_file(StringBuilder *sb,
                                char *ir_path,
                                char *asm_path,
                                char *obj_path,
                                EIr *ir, Varss *varss,
                                EModulePath *path,
                                bool include_deps,
                                bool import_deps) {
  remove(ir_path);
  FILE *ir_file = fopen(ir_path, "wb");
  if (!ir_file) {
    ERROR("Could not write %s\n", ir_path);
    return false;
  }

  encode_ir_as_evm_ir(ir_file, ir, varss, path, include_deps, import_deps);

  fclose(ir_file);

  sb->len = 0;

  sb_push(sb, EVM_PREFIX);
  sb_push(sb, asm_path);
  sb_push_char(sb, ' ');
  sb_push(sb, ir_path);
  sb_push_char(sb, '\0');

  if (system(sb->buffer) != 0)
    return false;

  sb->len = 0;

  sb_push(sb, YASM_PREFIX);
  sb_push(sb, obj_path);
  sb_push_char(sb, ' ');
  sb_push(sb, asm_path);
  sb_push_char(sb, '\0');

  if (system(sb->buffer) != 0)
    return false;

  return true;
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
  Arena arena = {0};
  Str input_path_str = str_new(config.input_path);
  if (!parse(&ir, &varss, &arena, code, input_path_str)) {
    arena_free(&arena);
    varss_destroy(&varss);
    ir_destroy(&ir);
    free(code.ptr);
    config_destroy(&config);
    return 1;
  }

  free(code.ptr);

  Str cache_path_str = { NULL, (u32) -1 };
  if (config.cache_path)
    cache_path_str = str_new(config.cache_path);

  IncludedDeps included_deps = {0};
  EModulePath path = {0};
  if (!load_module_deps(&ir, &arena, input_path_str,
                        cache_path_str, &config.include_paths,
                        &included_deps, &path, STR_LIT(".o"))) {
    if (included_deps.items)
      free(included_deps.items);
    arena_free(&arena);
    varss_destroy(&varss);
    ir_destroy(&ir);
    config_destroy(&config);
    return 1;
  }

  i32 result = 0;
  StringBuilder sb = {0};

  if (config.cache_path) {
    if (config.link_only) {
      if (needs_recompilation(config.input_path, config.output_path)) {
        if (!check_ir(&ir, &varss, &arena, !config.link_only)) {
          result = 1;
          goto end;
        }

#ifndef NDEBUG
        for (u32 i = 0; i < ir.procs.len; ++i)
          proc_print(ir.procs.items + i);
#endif

        Str ir_path = prefix_module_path_with_other_path_zero_term(cache_path_str, str_new(config.ir_path), STR_LIT(""), &arena, false);
        Str asm_path = prefix_module_path_with_other_path_zero_term(cache_path_str, str_new(config.asm_path), STR_LIT(""), &arena, false);
        Str obj_path = prefix_module_path_with_other_path_zero_term(cache_path_str, str_new(config.obj_path), STR_LIT(""), &arena, false);

        EModulePath module_path = {0};
        if (!compile_object_file(&sb, ir_path.ptr, asm_path.ptr, obj_path.ptr,
                                 &ir, &varss, &module_path, false, true)) {
          result = 1;
          goto end;
        }

        if (!copy_file(obj_path.ptr, config.output_path)) {
          ERROR("Could not copy %s to %s\n", obj_path.ptr, config.output_path);
          result = 1;
          goto end;
        }
      }
    } else {
      bool relink = false;

      Str ir_path = prefix_module_path_with_other_path_zero_term(cache_path_str, str_new(config.ir_path), STR_LIT(""), &arena, false);
      Str asm_path = prefix_module_path_with_other_path_zero_term(cache_path_str, str_new(config.asm_path), STR_LIT(""), &arena, false);
      Str obj_path = prefix_module_path_with_other_path_zero_term(cache_path_str, str_new(config.obj_path), STR_LIT(""), &arena, false);

      if (needs_recompilation(config.input_path, obj_path.ptr)) {
        relink = true;

        if (!check_ir(&ir, &varss, &arena, !config.link_only)) {
          result = 1;
          goto end;
        }

#ifndef NDEBUG
        for (u32 i = 0; i < ir.procs.len; ++i)
          proc_print(ir.procs.items + i);
#endif

        EModulePath module_path = {0};
        if (!compile_object_file(&sb, ir_path.ptr, asm_path.ptr, obj_path.ptr,
                                 &ir, &varss, &module_path, false, true)) {
          result = 1;
          goto end;
        }
      }

      for (u32 i = 0; i < included_deps.len; ++i) {
        if (included_deps.items[i].dep->ir.is_copy ||
            included_deps.items[i].dep->ir.is_from_cache)
          continue;

        relink = true;

        Str path = merge_module_path(&included_deps.items[i].dep->path, &arena);
        Str ir_path = prefix_module_path_with_other_path_zero_term(cache_path_str, path, STR_LIT(".eir"), &arena, false);
        Str asm_path = prefix_module_path_with_other_path_zero_term(cache_path_str, path, STR_LIT(".s"), &arena, false);
        Str obj_path = prefix_module_path_with_other_path_zero_term(cache_path_str, path, STR_LIT(".o"), &arena, false);

        if (!compile_object_file(&sb, ir_path.ptr, asm_path.ptr, obj_path.ptr,
                                 &included_deps.items[i].dep->ir,
                                 &included_deps.items[i].dep->varss,
                                 &included_deps.items[i].dep->path,
                                 false, true)) {
          result = 1;
          goto end;
        }
      }

      if (relink) {
        sb.len = 0;

        sb_push(&sb, LD_PREFIX);
        sb_push(&sb, config.output_path);
        sb_push_char(&sb, ' ');
        sb_push_str(&sb, obj_path);
        for (u32 i = 0; i < included_deps.len; ++i) {
          if (included_deps.items[i].dep->ir.is_copy)
            continue;

          Str path = merge_module_path(&included_deps.items[i].dep->path, &arena);
          Str obj_path = prefix_module_path_with_other_path_zero_term(cache_path_str, path, STR_LIT(".o"), &arena, false);

          sb_push_char(&sb, ' ');
          sb_push_str(&sb, obj_path);
        }
        sb_push_char(&sb, '\0');

        if (system(sb.buffer) != 0) {
          result = 1;
          goto end;
        }
      }
    }
  } else {
    if (!check_ir(&ir, &varss, &arena, !config.link_only)) {
      result = 1;
      goto end;
    }

#ifndef NDEBUG
    for (u32 i = 0; i < ir.procs.len; ++i)
      proc_print(ir.procs.items + i);
#endif

    EModulePath module_path = {0};
    if (!compile_object_file(&sb, config.ir_path, config.asm_path,
                             config.obj_path, &ir, &varss, &module_path,
                             !config.link_only, config.link_only)) {
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
  }

  if (config.run) {
    char *run_path = config.output_path;
    bool malloced = true;

    u32 len = strlen(run_path);
    for (u32 i = 0; i < len; ++i) {
      if (run_path[i] == '/') {
        malloced = false;
        break;
      }
    }

    if (malloced) {
      char *temp_path = malloc(2 + len);
      temp_path[0] = '.';
      temp_path[1] = '/';
      strcpy(temp_path + 2, run_path);
      run_path = temp_path;
    }

    result = system(run_path);

    if (malloced)
      free(run_path);
  }

end:
  if (sb.buffer)
    free(sb.buffer);
  if (included_deps.items)
    free(included_deps.items);
  arena_free(&arena);
  varss_destroy(&varss);
  ir_destroy(&ir);
  config_destroy(&config);

  return result;
}
