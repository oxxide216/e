#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define NDEBUG
#include "shl/shl-defs.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"
#include "shl/shl-log.h"

#define BUILD_PATH "./build"
#define TESTS_PATH "./tests"

typedef struct {
  char *name;
  bool  expect_compilation_failure;
  i32   expected_status;
  char *expected_output;
} Test;

static Test tests[] = {
  { "arrays",                false, 3,   "" },
  { "asm",                   false, 0,   "Hello, World!\n" },
  { "break-continue",        false, 0,   "" },
  { "const-struct",          false, 6,   "" },
  { "drop-tuples",           false, 0,   "Foo drop!\nFoo drop!\nFoo drop!\n" },
  { "explicit-copy",         false, 0,   "Foo copied!\nFoo dropped!\nFoo dropped!\n" },
  { "hello",                 false, 10,  "" },
  { "implicit-copy",         false, 0,   "Foo dropped!\nFoo dropped!\n" },
  { "land-lor",              false, 0,   "This should happen\ntrue\nfalse\ntrue\nThis should happen\ntrue\n" },
  { "move",                  true,  0,   "" },
  { "nested-structs",        false, 17,  "A drop!\nB drop!\n" },
  { "nobreak",               false, 0,  "This should happen\n" },
  { "pointers",              false, 6,   "" },
  { "scope",                 true,  0,   "" },
  { "sized-arrays",          false, 14,  "" },
  { "sizeof",                false, 144, "" },
  { "static",                false, 4,   "" },
  { "struct-field-copy",     false, 10,  "" },
  { "struct-return-no-dest", false, 0,  "" },
  { "structs",               false, 9,   "drop!\n" },
  { "temp-ref",              false, 4,   "" },
  { "tuples",                false, 138, "" },
  { "vec-invalid0",          true,  0,   "" },
  { "vec-invalid1",          true,  0,   "" },
  { "vec-valid",             false, 13,  "" },
};

typedef struct {
  u32           test_index;
  pid_t         pid;
  i32           output_fd;
  char          path[512];
  StringBuilder output_sb;
} Process;

typedef Da(Process) Processes;

typedef enum {
  StatusOk = 0,
  StatusFail,
} Status;

typedef struct {
  Processes compilations;
  Processes running;
  Status    statuses[ARRAY_LEN(tests)];
  bool      debug;
} Tester;

static void tester_test(Tester *tester, u32 index) {
  Test *test = tests + index;

  Process process;
  process.test_index = index;

  snprintf(process.path, sizeof(process.path),
           BUILD_PATH"/%s", test->name);

  process.pid = fork();
  if (process.pid == 0) {
    if (!tester->debug) {
      i32 dev_null = open("/dev/null", O_WRONLY);
      dup2(dev_null, 1);
      dup2(dev_null, 2);
    }

    char input_path[512];
    snprintf(input_path, sizeof(input_path),
             TESTS_PATH"/%s.e", test->name);

    char *args[] = { "./ec", "-I", ".", "-o",
                     process.path, input_path, NULL };
    execvp(args[0], args);
  }

  DA_APPEND(tester->compilations, process);
}

static void tester_finalize_test_on_compilation_fail(Tester *tester, u32 index) {
  Test *test = tests + index;

  Status status = test->expect_compilation_failure ? StatusOk : StatusFail;
  tester->statuses[index] = status;

  if (status != StatusOk)
    INFO("%s: Unexpected compilation failure\n", test->name);
}

static void tester_finalize_test(Tester *tester, Process *process,
                                 i32 status, Str output) {
  Test *test = tests + process->test_index;

  Status _status =
    (test->expected_status == status &&
     str_eq(output, str_new(test->expected_output))) ?
    StatusOk :
    StatusFail;
  tester->statuses[process->test_index] = _status;

  if (_status != StatusOk) {
    if (test->expected_status != status)
      INFO("%s: Expected exit code %d, got %d\n", test->name, test->expected_status, status);
    else
      INFO("%s: Expected output:\n%s\nGot:\n"STR_FMT"\n", test->name, test->expected_output, STR_ARG(output));
  }
}

static bool tester_step(Tester *tester) {
  if (tester->compilations.len == 0 &&
      tester->running.len == 0)
    return false;

  for (u32 i = 0; i < tester->compilations.len; ++i) {
    i32 status;
    pid_t pid = waitpid(tester->compilations.items[i].pid, &status, WNOHANG);
    if (pid > 0) {
      if (WEXITSTATUS(status) == 0) {
        Test *test = tests + tester->compilations.items[i].test_index;

        if (test->expect_compilation_failure) {
          Status status = test->expect_compilation_failure ? StatusFail : StatusOk;
          tester->statuses[tester->compilations.items[i].test_index] = status;

          if (status != StatusOk)
            INFO("%s: Unexpected compilation success\n", test->name);
        } else {
          i32 pipe_fds[2];
          pipe(pipe_fds);

          pid_t pid = fork();
          if (pid == 0) {
            close(pipe_fds[0]);
            dup2(pipe_fds[1], 1);
            dup2(pipe_fds[1], 2);
            char *args[] = { tester->compilations.items[i].path, NULL };
            if (execvp(args[0], args) < 0)
              ERROR("exec failed: %s\n", strerror(errno));
            exit(0);
          }

          close(pipe_fds[1]);
          i32 flags = fcntl(pipe_fds[0], F_GETFL, 0);
          fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);

          Process process = {0};
          process.test_index = tester->compilations.items[i].test_index;
          process.pid = pid;
          process.output_fd = pipe_fds[0];
          strcpy(process.path, tester->compilations.items[i].path);
          DA_APPEND(tester->running, process);
        }
      } else {
        tester_finalize_test_on_compilation_fail(tester, tester->compilations.items[i].test_index);
      }

      DA_REMOVE_AT(tester->compilations, i);
      --i;
    } else if (pid < 0) {
      DA_REMOVE_AT(tester->compilations, i);
      --i;
    }
  }

  for (u32 i = 0; i < tester->running.len; ++i) {
    char buffer[1024];
    i32 len;
    while ((len = read(tester->running.items[i].output_fd, buffer, sizeof(buffer))) > 0)
      sb_push_str(&tester->running.items[i].output_sb, STR(buffer, len));

    i32 status;
    pid_t pid = waitpid(tester->running.items[i].pid, &status, WNOHANG);
    if (pid > 0) {
      tester_finalize_test(tester, tester->running.items + i, WEXITSTATUS(status),
                           sb_to_str(tester->running.items[i].output_sb));
      if (tester->running.items[i].output_sb.buffer)
        free(tester->running.items[i].output_sb.buffer);
      DA_REMOVE_AT(tester->running, i);
      --i;
    } else if (pid < 0) {
      if (tester->running.items[i].output_sb.buffer)
        free(tester->running.items[i].output_sb.buffer);
      DA_REMOVE_AT(tester->running, i);
      --i;
    }
  }

  return
    tester->compilations.len > 0 ||
    tester->running.len > 0;
}

static void tester_print_statues(Tester *tester) {
  if (tester->debug)
    putc('\n', stdout);

  u32 passed = 0;

  for (u32 i = 0; i < ARRAY_LEN(tests); ++i) {
    printf("%-24s  [%s]\n", tests[i].name,
           tester->statuses[i] == StatusOk ?
           "\e[1;32m Ok \e[0m" : "\e[1;31mFail\e[0m");

    if (tester->statuses[i] == StatusOk)
      ++passed;
  }

  putc('\n', stdout);
  INFO("Passed: %u/%u\n", passed, (u32) ARRAY_LEN(tests));
}

static void tester_destroy(Tester *tester) {
  if (tester->compilations.items)
    free(tester->compilations.items);
  if (tester->running.items)
    free(tester->running.items);
}

i32 main(i32 argc, char **argv) {
  Tester tester = {0};
  tester.debug = argc > 1 && strcmp(argv[1], "--debug") == 0;

  for (u32 i = 0; i < ARRAY_LEN(tests); ++i)
    tester_test(&tester, i);

  while (tester_step(&tester));

  tester_print_statues(&tester);

  tester_destroy(&tester);

  return 0;
}
