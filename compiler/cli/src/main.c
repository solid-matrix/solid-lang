/**
 * @file main.c
 * @brief CLI entry point, orchestrates the compilation pipeline.
 * @author solid-matrix
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "irgen.h"
#include "xmem.h"

/**
 * Link stage (draft cli.md §1): the seed emits its object file itself; the
 * link dialect follows the libc knob — `static`/`dynamic` go through the
 * clang driver with lld as the only executor (the driver brings the
 * platform crt files and libc), `none` drives `ld.lld` directly (no crt,
 * no libc). Tools are resolved from PATH by candidate name (bare then
 * version-suffixed); the resolved lld path is handed over explicitly so
 * clang's own discovery is never a second source of truth.
 */
static const char *const k_clang_candidates[] = {
    "clang", "clang-22", "clang-21", "clang-20", "clang-19", "clang-18",
};
static const char *const k_lld_candidates[] = {
    "ld.lld", "ld.lld-22", "ld.lld-21", "ld.lld-20", "ld.lld-19", "ld.lld-18",
};

/** Copies the first PATH entry holding an executable @p name into @p out. */
static bool find_in_path(const char *name, char *out, size_t out_size) {
  const char *path = getenv("PATH");
  if (path == NULL) {
    return false;
  }
  for (const char *p = path; *p != '\0';) {
    const char *end = strchr(p, ':');
    size_t len = (end != NULL) ? (size_t)(end - p) : strlen(p);
    int written = snprintf(out, out_size, "%.*s/%s", (int)len, p, name);
    if (written > 0 && (size_t)written < out_size && access(out, X_OK) == 0) {
      return true;
    }
    if (end == NULL) {
      break;
    }
    p = end + 1;
  }
  return false;
}

/** Prints the missed-tool diagnostic: searched candidates + install hint. */
static void report_missing(const char *role, const char *const *names, size_t count) {
  fprintf(stderr, "solid: no %s on PATH; searched:", role);
  for (size_t i = 0; i < count; i++) {
    fprintf(stderr, " %s", names[i]);
  }
  fprintf(stderr, "\nsolid: install the LLVM toolchain, e.g. apt install clang lld\n");
}

/** Resolves the first executable candidate on PATH into @p out. */
static bool resolve_tool(const char *const *names, size_t count, char *out, size_t out_size) {
  for (size_t i = 0; i < count; i++) {
    if (find_in_path(names[i], out, out_size)) {
      return true;
    }
  }
  return false;
}

static int usage(void) {
  fprintf(stderr, "usage: solid build -o <output> [--target=<triple>]\n");
  return 1;
}

/**
 * Maps a triple's os/env components onto the seed kind (draft cli.md §1):
 * the env/os component `none` means no libc (freestanding); `musl` means
 * static libc; `gnu`/`msvc` mean dynamic libc. The seed only lowers
 * freestanding linux (raw exit syscall) and bare metal (halt) so far.
 */
static bool seed_kind_from_target(const char *triple, IrgenSeedKind *kind) {
  enum { OS_ABSENT, OS_LINUX, OS_WINDOWS, OS_MACOS, OS_OSLESS };
  int os = OS_ABSENT;
  bool no_libc = false;
  char component[64];
  for (const char *p = triple; *p != '\0';) {
    const char *end = strchr(p, '-');
    size_t len = (end != NULL) ? (size_t)(end - p) : strlen(p);
    if (len > 0 && len < sizeof(component)) {
      memcpy(component, p, len);
      component[len] = '\0';
      if (strcmp(component, "none") == 0) {
        no_libc = true;
      } else if (strcmp(component, "linux") == 0) {
        os = OS_LINUX;
      } else if (strcmp(component, "windows") == 0) {
        os = OS_WINDOWS;
      } else if (strcmp(component, "macos") == 0) {
        os = OS_MACOS;
      } else if (strcmp(component, "osless") == 0) {
        os = OS_OSLESS;
      }
    }
    if (end == NULL) {
      break;
    }
    p = end + 1;
  }

  if (!no_libc) {
    *kind = IRGEN_SEED_HOSTED;
    return true;
  }
  if (os == OS_ABSENT || os == OS_OSLESS) {
    *kind = IRGEN_SEED_FREESTANDING_BARE;
    return true;
  }
  if (os == OS_LINUX) {
    *kind = IRGEN_SEED_FREESTANDING_LINUX;
    return true;
  }
  fprintf(stderr, "solid: seed does not lower %s freestanding targets yet\n", triple);
  return false;
}

/**
 * Links @p object into @p output. Hosted: the clang driver with @p lld as
 * executor. Freestanding: @p lld directly, entry `_start`, no crt/libc.
 * Returns 0 on success; driver diagnostics pass through untouched.
 */
static int link_executable(const char *clang, const char *lld, const char *object,
                           const char *output, bool hosted) {
  fflush(stdout);
  fflush(stderr);
  pid_t pid = fork();
  if (pid < 0) {
    fprintf(stderr, "solid: fork: %s\n", strerror(errno));
    return 1;
  }
  if (pid == 0) {
    if (hosted) {
      char fuse_ld[4608];
      snprintf(fuse_ld, sizeof(fuse_ld), "-fuse-ld=%s", lld);
      execl(clang, clang, object, fuse_ld, "-o", output, (char *)NULL);
    } else {
      execl(lld, lld, "--entry", "_start", object, "-o", output, (char *)NULL);
    }
    fprintf(stderr, "solid: exec linker driver: %s\n", strerror(errno));
    _exit(127);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    fprintf(stderr, "solid: waitpid: %s\n", strerror(errno));
    return 1;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "solid: link failed\n");
    return 1;
  }
  return 0;
}

int main(int argc, char **argv) {
  const char *output = NULL;
  const char *target = NULL;
  IrgenSeedKind kind = IRGEN_SEED_HOSTED;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "build") == 0) {
      continue;
    }
    if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
      output = argv[++i];
      continue;
    }
    if (strncmp(argv[i], "--target=", 9) == 0) {
      target = argv[i] + 9;
      continue;
    }
    return usage();
  }
  if (output == NULL) {
    return usage();
  }
  if (target != NULL && !seed_kind_from_target(target, &kind)) {
    return 1;
  }

  bool hosted = (kind == IRGEN_SEED_HOSTED);
  char clang[4096];
  char lld[4096];
  if (hosted) {
    if (!resolve_tool(k_clang_candidates, sizeof(k_clang_candidates) / sizeof(k_clang_candidates[0]),
                      clang, sizeof(clang))) {
      report_missing("clang driver", k_clang_candidates,
                     sizeof(k_clang_candidates) / sizeof(k_clang_candidates[0]));
      return 1;
    }
  }
  if (!resolve_tool(k_lld_candidates, sizeof(k_lld_candidates) / sizeof(k_lld_candidates[0]), lld,
                    sizeof(lld))) {
    report_missing("lld linker", k_lld_candidates,
                   sizeof(k_lld_candidates) / sizeof(k_lld_candidates[0]));
    return 1;
  }

  if (irgen_init() != 0) {
    return 1;
  }
  IrgenModule *module = irgen_build_min_module(kind);
  if (module == NULL) {
    return 1;
  }

  size_t object_size = strlen(output) + sizeof(".o");
  char *object = xmalloc(object_size);
  snprintf(object, object_size, "%s.o", output);

  int failed = irgen_emit_object(module, object) != 0;
  irgen_dispose_module(module);
  if (!failed) {
    failed = link_executable(clang, lld, object, output, hosted);
  }
  if (!failed) {
    // Keep the object around for post-mortem when a stage fails.
    unlink(object);
  }
  xfree(object);
  return failed ? 1 : 0;
}
