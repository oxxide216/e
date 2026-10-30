#include "io.h"
#include "ir.h"
#include "assembler.h"
#include "shl/shl-defs.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"
#include "shl/shl-log.h"

typedef enum {
  FormatELF64 = 0,
} Format;

typedef struct {
  char   *input_path;
  char   *output_path;
  Format  format;
  bool    is_output_path_malloced;
} Config;

static void print_usage(char *program_name) {
  fprintf(stderr, "Usage: %s <options...> <input file>\n\n", program_name);
  fprintf(stderr, "Options:\n");
  fprintf(stderr, "       -o <output file>          Specify output file\n");
  fprintf(stderr, "       -f <output format>        Specify output format\n");
}

static char *make_output_path(char *input_path) {
  u32 len = strlen(input_path);
  if (len > 2 && strcmp(input_path + len - 2, ".s") == 0) {
    char *result = malloc(len - 2 + 2 + 1);
    memcpy(result, input_path, len - 2);
    strcpy(result + len - 2, ".o");
    result[len - 2 + 2] = '\0';
    return result;
  } else if (len > 4 &&
             (strcmp(input_path + len - 4, ".asm") == 0 ||
              strcmp(input_path + len - 4, ".esm") == 0)) {
    char *result = malloc(len - 4 + 2 + 1);
    memcpy(result, input_path, len - 2);
    strcpy(result + len - 4, ".o");
    result[len - 4 + 2] = '\0';
    return result;
  } else {
    char *result = malloc(len + 2 + 1);
    memcpy(result, input_path, len);
    strcpy(result + len, ".o");
    result[len + 2] = '\0';
    return result;
  }
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
    } else if (strcmp(argv[i], "-f") == 0) {
      if (i + 1 == (u32) argc) {
        print_usage(argv[0]);
        ERROR("Option %s requires an argument\n", argv[i]);
        exit(1);
      }

      if (strcmp(argv[i + 1], "elf64") == 0) {
        config.format = FormatELF64;
      } else {
        ERROR("Invalid format: %s\n", argv[i + 1]);
        INFO("Available formats:\n");
        INFO("  elf64\n");
        exit(1);
      }

      ++i;
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

  return config;
}

static void config_destroy(Config *config) {
  if (config->is_output_path_malloced)
    free(config->output_path);
}

i32 main(i32 argc, char **argv) {
  Config config = config_create(argc, argv);

  Str content = read_file(config.input_path);
  if (content.len == (u32) -1) {
    ERROR("Could not read %s\n", config.input_path);
    config_destroy(&config);
    return 1;
  }

  EsmIr ir = {0};
  if (!str_to_esm_ir(content, config.input_path, &ir)) {
    config_destroy(&config);
    return 1;
  }

  remove(config.output_path);
  FILE *output_file = fopen(config.output_path, "wb");
  if (!output_file) {
    ERROR("Could not write %s\n", config.output_path);
    esm_ir_free(&ir);
    config_destroy(&config);
    return 1;
  }

  switch (config.format) {
  case FormatELF64: {
    if (!esm_assemble_elf_x86_64(output_file, &ir)) {
      fclose(output_file);
      esm_ir_free(&ir);
      config_destroy(&config);
      return 1;
    }
  } break;
  }

  fclose(output_file);
  esm_ir_free(&ir);
  config_destroy(&config);

  return 0;
}
