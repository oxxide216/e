#include "deps.h"
#include "io.h"
#include "utils.h"
#include "cache-decoder.h"
#include "parser.h"
#include "checker.h"
#include "cache-encoder.h"
#include "shl/shl-log.h"

static Str merge_module_path(EModulePath path) {
  StringBuilder sb = {0};
  for (u32 i = 0; i < path.len; ++i) {
    if (str_eq(path.items[i], STR_LIT("super"))) {
      while (sb.len > 0 && sb.buffer[sb.len - 1] != '/')
        --sb.len;
      if (sb.len > 0)
        --sb.len;
    } else {
      if (i > 0)
        sb_push_char(&sb, '/');
      sb_push_str(&sb, path.items[i]);
    }
  }
  return sb_to_str(sb);
}

static Str prefix_module_path_with_other_path_zero_term(Str prefix_path, Str module_path, Str ext, bool truncate_prefix_path) {
  if (truncate_prefix_path)
    while (prefix_path.len > 0 && prefix_path.ptr[prefix_path.len - 1] != '/')
      --prefix_path.len;

  bool has_trailing_slash = prefix_path.len > 0;

  Str result;
  result.len = prefix_path.len + has_trailing_slash + module_path.len + ext.len;
  result.ptr = malloc(result.len + 1);
  memcpy(result.ptr, prefix_path.ptr, prefix_path.len);
  if (has_trailing_slash)
    result.ptr[prefix_path.len] = '/';
  memcpy(result.ptr + prefix_path.len + has_trailing_slash, module_path.ptr, module_path.len);
  memcpy(result.ptr + prefix_path.len + has_trailing_slash + module_path.len, ext.ptr, ext.len);
  result.ptr[result.len] = '\0';
  return result;
}

static Str read_file_arena(char *path, Arena *arena) {
  Str content;

  FILE *file = fopen(path, "r");
  if (!file)
    return (Str) { NULL, (unsigned int) -1 };

  fseek(file, 0, SEEK_END);
  content.len = ftell(file);
  content.ptr = arena_alloc(arena, content.len);
  fseek(file, 0, SEEK_SET);
  fread(content.ptr, 1, content.len, file);
  fclose(file);

  return content;
}

void varss_destroy(Varss *varss) {
  for (u32 i = 0; i < varss->len; ++i) {
    Vars *vars = varss->items + i;

    for (u32 j = 0; j < vars->len; ++j)
      if (vars->items[j].type.kind == ETypeKindPtr)
        type_free(vars->items[j].type.ptr_target);
    if (vars->items)
      free(vars->items);
  }
  if (varss->items)
    free(varss->items);
}

static bool contains_hash(Hashes *hashes, u64 hash) {
  for (u32 i = 0; i < hashes->len; ++i)
    if (hashes->items[i] == hash)
      return true;

  return false;
}

bool load_module_deps(EIr *ir, Arena *arena, Str input_path,
                      Str cache_path, Strs *include_paths,
                      Hashes *included_hashes) {
  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    Str path = merge_module_path(ir->module_deps.items[i].path);
    Str full_module_path = prefix_module_path_with_other_path_zero_term(input_path, path, STR_LIT(".e"), true);
    Str full_cache_path = {0};

    bool loaded_cache = false;
    if (cache_path.len != (u32) -1) {
      full_cache_path = prefix_module_path_with_other_path_zero_term(cache_path, path, STR_LIT(".eir"), false);

      if (!needs_recompilation(full_module_path.ptr, full_cache_path.ptr)) {
        Str content = read_file_arena(full_cache_path.ptr, arena);
        if (content.len != (u32) -1) {
          u64 hash;
          if (decode_cache(&ir->module_deps.items[i].ir, arena, (u8 *) content.ptr, content.len, &hash)) {
            loaded_cache = true;
          } else {
            ERROR("Invalid cache in %s\n", full_cache_path.ptr);
            ir->module_deps.items[i].ir = (EIr) {0};
          }

          if (contains_hash(included_hashes, hash))
            ir->module_deps.items[i].ir = (EIr) {0};
        }
      }
    }

    if (!loaded_cache) {
      Str found_path = full_module_path;
      Str code = read_file(found_path.ptr);
      for (u32 j = 0; j < include_paths->len && code.len == (u32) -1; ++j) {
        free(found_path.ptr);
        found_path = prefix_module_path_with_other_path_zero_term(include_paths->items[j], path, STR_LIT(".e"), false);
        code = read_file(found_path.ptr);
      }
      if (code.len == (u32) -1) {
        ERROR("Could not find module ");
        for (u32 j = 0; j < ir->module_deps.items[i].path.len; ++j) {
          if (j > 0)
            fprintf(stderr, "::");
          fprintf(stderr, STR_FMT, STR_ARG(ir->module_deps.items[i].path.items[j]));
        }
        fprintf(stderr, "\n");
        free(full_cache_path.ptr);
        free(found_path.ptr);
        free(path.ptr);
        return false;
      }

      u64 hash = str_hash(code);

      if (contains_hash(included_hashes, hash)) {
        free(code.ptr);
        free(full_cache_path.ptr);
        free(found_path.ptr);
        free(path.ptr);
        return true;
      }

      DA_APPEND(*included_hashes, hash);

      if (!parse(&ir->module_deps.items[i].ir,
                 &ir->module_deps.items[i].varss,
                 code, found_path)) {
        free(code.ptr);
        free(full_cache_path.ptr);
        free(found_path.ptr);
        free(path.ptr);
        return false;
      }

      if (!load_module_deps(&ir->module_deps.items[i].ir, arena,
                            found_path, cache_path,
                            include_paths, included_hashes)) {
        free(code.ptr);
        free(full_cache_path.ptr);
        free(found_path.ptr);
        free(path.ptr);
        return false;
      }

      if (!check_ir(&ir->module_deps.items[i].ir,
                    &ir->module_deps.items[i].varss,
                    false)) {
        free(code.ptr);
        free(full_cache_path.ptr);
        free(found_path.ptr);
        free(path.ptr);
        return false;
      }

      if (cache_path.len != (u32) -1) {
        remove(full_cache_path.ptr);
        make_directory(full_cache_path, true);
        FILE *output_file = fopen(full_cache_path.ptr, "wb");
        if (!output_file) {
          ERROR("Could not write %s\n", full_cache_path.ptr);
          free(code.ptr);
          free(full_cache_path.ptr);
          free(found_path.ptr);
          free(path.ptr);
          return false;
        }

        encode_cache(output_file, &ir->module_deps.items[i].ir, hash);

        fclose(output_file);
      }
    } else {
      free(full_module_path.ptr);
    }

    free(full_cache_path.ptr);
    free(path.ptr);
  }

  return true;
}
