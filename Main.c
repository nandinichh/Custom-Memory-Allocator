/*
 * main.c
/*

#include "allocator.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── ANSI colours for terminal output ─────────────────────────────────────── */
#define GRN "\033[32m"
#define RED "\033[31m"
#define YLW "\033[33m"
#define CYN "\033[36m"
#define RST "\033[0m"

/* ── Test helpers ─────────────────────────────────────────────────────────── */
static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(expr, msg) do { \
    tests_run++; \
    if (expr) { \
        printf(GRN "  [PASS]" RST " %s\n", msg); \
        tests_passed++; \
    } else { \
        printf(RED "  [FAIL]" RST " %s\n", msg); \
    } \
} while(0)

static void separator(const char *title) {
    printf(CYN "\n──────────────────────────────────────────\n");
    printf("  %s\n", title);
    printf("──────────────────────────────────────────\n" RST);
}

/* ── Test suites ──────────────────────────────────────────────────────────── */

void test_basic_alloc(void) {
    separator("TEST 1: Basic Allocation & Free");

    void *a = mem_alloc(32);
    void *b = mem_alloc(128);
    void *c = mem_alloc(256);

    CHECK(a != NULL,  "alloc 64 bytes");
    CHECK(b != NULL,  "alloc 128 bytes");
    CHECK(c != NULL,  "alloc 256 bytes");
    CHECK(a != b && b != c && a != c, "all pointers are distinct");

    mem_dump();

    mem_free(a);
    mem_free(b);
    mem_free(c);

    CHECK(mem_check(), "heap intact after frees");
}

void test_alignment(void) {
    separator("TEST 2: 8-Byte Alignment");

    void *p1 = mem_alloc(1);
    void *p2 = mem_alloc(3);
    void *p3 = mem_alloc(7);
    void *p4 = mem_alloc(9);

    CHECK(((size_t)p1 % 8) == 0, "1-byte alloc is 8-byte aligned");
    CHECK(((size_t)p2 % 8) == 0, "3-byte alloc is 8-byte aligned");
    CHECK(((size_t)p3 % 8) == 0, "7-byte alloc is 8-byte aligned");
    CHECK(((size_t)p4 % 8) == 0, "9-byte alloc is 8-byte aligned");

    mem_free(p1); mem_free(p2); mem_free(p3); mem_free(p4);
}

void test_coalescing(void) {
    separator("TEST 3: Coalescing Adjacent Free Blocks");

    /* Allocate three blocks side by side */
    void *x = mem_alloc(100);
    void *y = mem_alloc(100);
    void *z = mem_alloc(100);

    CHECK(x && y && z, "three 100-byte allocs succeed");

    mem_free(x);
    mem_free(y);   /* y is freed between x and z — should coalesce with x */
    mem_free(z);   /* z freed — should coalesce entire run into one block  */

    /* After full coalescing the large free block should fit a big alloc */
    void *big = mem_alloc(280);
    CHECK(big != NULL, "280-byte alloc succeeds after coalescing (proves merge worked)");
    mem_free(big);

    CHECK(mem_check(), "heap intact after coalesce test");
}

void test_realloc(void) {
    separator("TEST 4: mem_realloc");

    char *buf = mem_alloc(32);
    CHECK(buf != NULL, "initial 32-byte alloc");

    strcpy(buf, "Hello, allocator!");
    buf = mem_realloc(buf, 128);
    CHECK(buf != NULL, "realloc to 128 bytes");
    CHECK(strcmp(buf, "Hello, allocator!") == 0, "data preserved after realloc");

    buf = mem_realloc(buf, 16);
    CHECK(buf != NULL, "realloc shrink to 16 bytes");

    mem_free(buf);
    CHECK(mem_check(), "heap intact after realloc test");
}

void test_calloc(void) {
    separator("TEST 5: mem_calloc (zero initialisation)");

    int *arr = mem_calloc(10, sizeof(int));
    CHECK(arr != NULL, "calloc 10 ints");

    int all_zero = 1;
    for (int i = 0; i < 10; i++) if (arr[i] != 0) { all_zero = 0; break; }
    CHECK(all_zero, "all bytes zero-initialised");

    arr[0] = 42; arr[9] = 99;
    CHECK(arr[0] == 42 && arr[9] == 99, "can write to calloc'd memory");

    mem_free(arr);
}

void test_write_and_read(void) {
    separator("TEST 6: Write & Read");

    /* Allocate buffers and write/read integers — safe size */
    int *a = mem_alloc(sizeof(int) * 10);
    int *b = mem_alloc(sizeof(int) * 10);
    CHECK(a != NULL && b != NULL, "two int-array allocs succeed");

    for (int i = 0; i < 10; i++) { a[i] = i; b[i] = i * 2; }

    int ok = 1;
    for (int i = 0; i < 10; i++)
        if (a[i] != i || b[i] != i*2) { ok = 0; break; }

    CHECK(ok, "no overlap: a[] and b[] hold independent values");
    CHECK(a != b, "pointers are distinct");

    mem_free(a);
    mem_free(b);
    CHECK(mem_check(), "heap intact after write test");
}

void test_null_edge_cases(void) {
    separator("TEST 7: Edge Cases");

    void *p = mem_alloc(0);
    CHECK(p == NULL, "alloc(0) returns NULL");

    mem_free(NULL);   /* should not crash */
    CHECK(1, "free(NULL) does not crash");

    void *q = mem_realloc(NULL, 64);
    CHECK(q != NULL, "realloc(NULL, 64) acts like alloc");
    mem_free(q);

    void *r = mem_realloc(q, 0);  /* realloc with size 0 frees */
    CHECK(r == NULL, "realloc(ptr, 0) returns NULL");
}

void test_fragmentation(void) {
    separator("TEST 8: Fragmentation & Stats");

    /* Create a chequerboard of used/free blocks to show fragmentation */
    void *ptrs[10];
    for (int i = 0; i < 10; i++) ptrs[i] = mem_alloc(50);

    /* Free every other block — creates fragmentation */
    for (int i = 0; i < 10; i += 2) mem_free(ptrs[i]);

    printf(YLW "  [fragmented state — every other block freed]\n" RST);
    mem_stats();

    /* Free remaining — coalescing should clean up */
    for (int i = 1; i < 10; i += 2) mem_free(ptrs[i]);

    printf(YLW "  [after freeing all — coalescing should have merged]\n" RST);
    mem_stats();

    CHECK(mem_check(), "heap intact after fragmentation test");
}

/* ── Interactive menu ─────────────────────────────────────────────────────── */
static void *user_ptrs[16];
static int   user_count = 0;

void interactive_menu(void) {
    int choice;
    while (1) {
        printf(CYN "\n=== MEMORY ALLOCATOR MENU ===\n" RST);
        printf("  1. Allocate memory\n");
        printf("  2. Free last allocation\n");
        printf("  3. Reallocate last allocation\n");
        printf("  4. Calloc array\n");
        printf("  5. Dump heap\n");
        printf("  6. Show stats\n");
        printf("  7. Check integrity\n");
        printf("  8. Run all tests\n");
        printf("  0. Exit\n");
        printf("Choice: ");
        if (scanf("%d", &choice) != 1) break;

        size_t sz;
        switch (choice) {
            case 1:
                printf("Size in bytes: ");
                scanf("%zu", &sz);
                if (user_count < 16) {
                    user_ptrs[user_count] = mem_alloc(sz);
                    if (user_ptrs[user_count]) {
                        printf(GRN "Allocated %zu bytes at %p (slot %d)\n" RST,
                               sz, user_ptrs[user_count], user_count);
                        user_count++;
                    }
                } else printf(RED "Slot table full.\n" RST);
                break;

            case 2:
                if (user_count > 0) {
                    user_count--;
                    printf("Freeing slot %d (%p)\n", user_count, user_ptrs[user_count]);
                    mem_free(user_ptrs[user_count]);
                    user_ptrs[user_count] = NULL;
                } else printf("Nothing to free.\n");
                break;

            case 3:
                if (user_count > 0) {
                    printf("New size in bytes: ");
                    scanf("%zu", &sz);
                    int idx = user_count - 1;
                    user_ptrs[idx] = mem_realloc(user_ptrs[idx], sz);
                    printf(GRN "Reallocated to %zu bytes at %p\n" RST,
                           sz, user_ptrs[idx]);
                } else printf("Nothing to reallocate.\n");
                break;

            case 4: {
                size_t count;
                printf("Count: "); scanf("%zu", &count);
                printf("Element size: "); scanf("%zu", &sz);
                if (user_count < 16) {
                    user_ptrs[user_count] = mem_calloc(count, sz);
                    if (user_ptrs[user_count]) {
                        printf(GRN "Calloc'd %zu * %zu bytes at %p (slot %d)\n" RST,
                               count, sz, user_ptrs[user_count], user_count);
                        user_count++;
                    }
                } break;
            }

            case 5: mem_dump();   break;
            case 6: mem_stats();  break;
            case 7: mem_check();  break;

            case 8:
                /* Re-init heap and run full suite */
                mem_init();
                tests_run = tests_passed = 0;
                test_basic_alloc();
                test_alignment();
                test_coalescing();
                test_realloc();
                test_calloc();
                test_write_and_read();
                test_null_edge_cases();
                test_fragmentation();

                printf(CYN "\n=== RESULTS: %d / %d tests passed ===\n" RST,
                       tests_passed, tests_run);

                mem_init();   /* reset for interactive use */
                user_count = 0;
                break;

            case 0:
                printf("Goodbye!\n");
                return;

            default:
                printf("Invalid choice.\n");
        }
    }
}

/* ── Entry point ─────────────────────────────────────────────────────────── */
int main(void) {
    printf(CYN);
    printf(" ___  ___  ___ \n");
    printf("| . \\/ __>| . \\\n");
    printf("|  _/\\__ \\|  _/\n");
    printf("|_|  <___/|_|  \n");
    printf(RST);
    printf("  Custom Memory Allocator in C\n");
    printf("  Concepts: malloc/free, coalescing, splitting, alignment\n\n");

    mem_init();
    interactive_menu();
    return 0;
}
