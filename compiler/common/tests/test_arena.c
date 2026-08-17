/**
 * @file test_arena.c
 * @brief Tests for the Arena allocator behavior.
 * @author solid-matrix
 * @version 0.0.5
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "arena.h"

static int g_failures;

#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
      g_failures++;                                                    \
    }                                                                  \
  } while (0)

int main(void)
{
  // Init + alignment: all allocations are 16-byte aligned on x64
  Arena a;
  CHECK(arena_init(&a, 64));
  for (size_t n = 1; n <= 32; n++) {
    void *p = arena_alloc(&a, n);
    CHECK((uintptr_t)p % 16 == 0);
  }

  // Old pointers stay valid across growth
  char *p1 = arena_alloc(&a, 50);
  memset(p1, 'A', 50);
  void *big = arena_alloc(&a, 300);
  CHECK(big != NULL);
  for (int i = 0; i < 50; i++) {
    CHECK(p1[i] == 'A');
  }

  // Zeroed allocation
  unsigned char *z = arena_alloc_zeroed(&a, 10);
  for (int i = 0; i < 10; i++) {
    CHECK(z[i] == 0);
  }

  // Size 0 returns a valid pointer
  CHECK(arena_alloc(&a, 0) != NULL);

  // Reset makes memory reusable
  arena_reset(&a);
  for (int i = 0; i < 1000; i++) {
    CHECK(arena_alloc(&a, (size_t)(i % 200) + 1) != NULL);
  }

  arena_destroy(&a);

  // Default initial block + a large cross-block allocation (verifies
  // doubling growth and data integrity)
  Arena b;
  CHECK(arena_init(&b, 0));
  char *huge = arena_alloc(&b, 20000);
  CHECK(huge != NULL);
  for (int i = 0; i < 20000; i++) {
    huge[i] = (char)(i & 0xFF);
  }
  for (int i = 0; i < 20000; i++) {
    CHECK(huge[i] == (char)(i & 0xFF));
  }
  arena_destroy(&b);

  if (g_failures == 0) {
    printf("test_arena: all ok\n");
    return 0;
  }
  fprintf(stderr, "test_arena: %d failure(s)\n", g_failures);
  return 1;
}
