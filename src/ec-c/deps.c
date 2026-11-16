#include "deps.h"
#include "io.h"
#include "utils.h"
#include "cache-decoder.h"
#include "parser.h"
#include "checker.h"
#include "cache-encoder.h"
#include "shl/shl-log.h"

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

static EIr *get_ir(IncludedDeps *deps, u64 hash) {
  for (u32 i = 0; i < deps->len; ++i)
    if (deps->items[i].hash == hash)
      return &deps->items[i].dep->ir;

  return NULL;
}

bool load_module_deps(EIr *ir, Arena *arena, Str input_path,
                      Str cache_path, Strs *include_paths,
                      IncludedDeps *included_deps,
                      EModulePath *module_path, Str obj_ext) {
  EModulePath temp_path = *module_path;
  if (temp_path.len > 0)
    --temp_path.len;

  for (u32 i = 0; i < ir->module_deps.len; ++i) {
    if (ir->module_deps.items[i].ir.is_copy)
      continue;

    EModulePath new_path;
    if (ir->module_deps.items[i].is_submodule)
      new_path = merge_module_paths(&temp_path, &ir->module_deps.items[i].path);
    else
      new_path = dup_module_path(&ir->module_deps.items[i].path);

    free(ir->module_deps.items[i].path.items);
    ir->module_deps.items[i].path = new_path;
    ir->module_deps.items[i].is_submodule = false;

    Str path = merge_module_path(&new_path, arena);
    Str full_module_path = prefix_module_path_with_other_path_zero_term(input_path, path, STR_LIT(".e"), arena, true);
    Str full_cache_path = {0};

    bool loaded_cache = false;

    if (cache_path.len != (u32) -1) {
      full_cache_path = prefix_module_path_with_other_path_zero_term(cache_path, path, STR_LIT(".ecache"), arena, false);
      Str obj_path = prefix_module_path_with_other_path_zero_term(cache_path, path, obj_ext, arena, false);


      Str found_path = full_module_path;
      bool recompile =
        needs_recompilation(found_path.ptr, full_cache_path.ptr) ||
        needs_recompilation(obj_path.ptr, full_cache_path.ptr);

      for (u32 j = 0; j < include_paths->len && recompile; ++j) {
        found_path = prefix_module_path_with_other_path_zero_term(include_paths->items[j], path, STR_LIT(".e"), arena, false);
        obj_path = prefix_module_path_with_other_path_zero_term(cache_path, path, obj_ext, arena, false);
        recompile =
          needs_recompilation(found_path.ptr, full_cache_path.ptr) ||
          needs_recompilation(obj_path.ptr, full_cache_path.ptr);
      }

      if (!recompile) {
        Str content = read_file_arena(full_cache_path.ptr, arena);
        if (content.len != (u32) -1) {
          u64 hash;
          if (decode_cache(&ir->module_deps.items[i].ir, arena,
                           (u8 *) content.ptr, content.len, &hash)) {
            if (get_ir(included_deps, hash))
              ir->module_deps.items[i].ir.is_copy = true;

            IncludedDep dep = { hash, ir->module_deps.items + i };
            DA_APPEND(*included_deps, dep);

            if (!load_module_deps(&ir->module_deps.items[i].ir, arena,
                                  found_path, cache_path, include_paths,
                                  included_deps, &new_path, obj_ext))
              return false;

            if (!populate_type_struct_ptrs(&ir->module_deps.items[i].ir))
              return false;
  
            loaded_cache = true;

            // TODO: remove this
            printf("Loaded "STR_FMT"\n", STR_ARG(full_cache_path));
        } else {
            ERROR("Invalid cache in %s\n", full_cache_path.ptr);
            ir->module_deps.items[i].ir = (EIr) {0};
          }
        }
      }
    }

    if (!loaded_cache) {
      Str found_path = full_module_path;
      Str code = read_file(found_path.ptr);
      for (u32 j = 0; j < include_paths->len && code.len == (u32) -1; ++j) {
        found_path = prefix_module_path_with_other_path_zero_term(include_paths->items[j], path, STR_LIT(".e"), arena, false);
        code = read_file(found_path.ptr);
      }
      if (code.len == (u32) -1) {
        ERROR("Could not find module ");
        for (u32 j = 0; j < new_path.len; ++j) {
          if (j > 0)
            fprintf(stderr, "::");
          fprintf(stderr, STR_FMT, STR_ARG(new_path.items[j]));
        }
        fprintf(stderr, "\n");
        return false;
      }

      // TODO: remove this
      printf("Compiling "STR_FMT"\n", STR_ARG(found_path));

      u64 hash = str_hash(code);

      EIr *included_ir = get_ir(included_deps, hash);
      if (included_ir) {
        ir->module_deps.items[i].ir = *included_ir;
        ir->module_deps.items[i].ir.is_copy = true;
        free(code.ptr);
        continue;
      }

      if (!parse(&ir->module_deps.items[i].ir,
                 &ir->module_deps.items[i].varss,
                 arena, code, found_path)) {
        free(code.ptr);
        return false;
      }

      free(code.ptr);

      if (!load_module_deps(&ir->module_deps.items[i].ir, arena,
                            found_path, cache_path, include_paths,
                            included_deps, &new_path, obj_ext))
        return false;

      if (!check_ir(&ir->module_deps.items[i].ir,
                    &ir->module_deps.items[i].varss,
                    arena, false))
        return false;

      IncludedDep dep = { hash, ir->module_deps.items + i };
      DA_APPEND(*included_deps, dep);

      if (cache_path.len != (u32) -1) {
        remove(full_cache_path.ptr);
        make_directory(full_cache_path, true);
        FILE *output_file = fopen(full_cache_path.ptr, "wb");
        if (!output_file) {
          ERROR("Could not write %s\n", full_cache_path.ptr);
          return false;
        }

        encode_cache(output_file, &ir->module_deps.items[i].ir, hash);

        fclose(output_file);
      }
    }
  }

  return true;
}
