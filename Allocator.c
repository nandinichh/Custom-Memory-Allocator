/*
 * allocator.c
 * Custom Memory Allocator — implements malloc / free / realloc / calloc
 * using a static heap, explicit free list, and first-fit placement.
 *
 * Key concepts demonstrated:
 *   - Block header metadata (size, free flag, magic number)
 *   - First-fit free-block search
 *   - Block splitting to reduce internal fragmentation
 *   - Coalescing adjacent free blocks to reduce external fragmentation
 *   - 8-byte alignment on every allocation
 *   - Heap integrity checking via magic numbers
 */

#include "allocator.h"
#include <string.h>   /* memset, memcpy */
#include <stdio.h>

/* ── Static heap ──────────────────────────────────────────────────────────── */
static unsigned char heap[HEAP_CAPACITY];   /* raw memory pool               */
static Block        *heap_start = NULL;     /* first block in the list        */
static int           initialized = 0;

/* ── Internal helpers ────────────────────────────────────────────────────── */

/* Return pointer to the payload (memory after the header) */
static void *payload(Block *b) {
    return (void *)((unsigned char *)b + BLOCK_SIZE);
}

/* Return pointer to the Block header given a payload pointer */
static Block *header(void *ptr) {
    return (Block *)((unsigned char *)ptr - BLOCK_SIZE);
}

/* Validate a block's magic number — catches use-after-free / corruption */
static int valid_block(Block *b) {
    return b != NULL && b->magic == MAGIC_NUMBER;
}

/* Split block b: keep 'size' bytes for the caller, carve remainder into
   a new free block (only if the leftover is large enough to be useful). */
static void split_block(Block *b, size_t size) {
    size_t remainder = b->size - size - BLOCK_SIZE;
    if (remainder < MIN_SPLIT_SIZE) return;   /* not worth splitting */

    /* Carve a new block out of the tail */
    Block *new_block = (Block *)((unsigned char *)payload(b) + size);
    new_block->size  = remainder;
    new_block->free  = 1;
    new_block->magic = MAGIC_NUMBER;
    new_block->next  = b->next;
    new_block->prev  = b;

    if (b->next) b->next->prev = new_block;
    b->next = new_block;
    b->size = size;
}

/* Coalesce block b with its successor if both are free.
   Returns the merged block (may be b itself). */
static Block *coalesce_next(Block *b) {
    if (!b->next || !valid_block(b->next) || !b->next->free) return b;

    b->size += BLOCK_SIZE + b->next->size;
    b->next  = b->next->next;
    if (b->next) b->next->prev = b;
    return b;
}

/* Coalesce block b with its predecessor if both are free.
   Returns the merged block. */
static Block *coalesce_prev(Block *b) {
    if (!b || !b->prev) return b;
    Block *p = b->prev;
    /* verify p is still a valid block inside our heap */
    if ((unsigned char *)p < heap || (unsigned char *)p >= heap + HEAP_CAPACITY) return b;
    if (!valid_block(p) || !p->free) return b;

    p->size += BLOCK_SIZE + b->size;
    p->next  = b->next;
    if (b->next) b->next->prev = p;
    return p;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

/*
 * mem_init — set up the heap with a single large free block.
 * Must be called before any allocation.
 */
void mem_init(void) {
    heap_start        = (Block *)heap;
    heap_start->size  = HEAP_CAPACITY - BLOCK_SIZE;
    heap_start->free  = 1;
    heap_start->next  = NULL;
    heap_start->prev  = NULL;
    heap_start->magic = MAGIC_NUMBER;
    initialized       = 1;
    printf("[mem_init] Heap ready: %d bytes at %p\n",
           HEAP_CAPACITY, (void *)heap_start);
}

/*
 * mem_alloc — first-fit allocation with splitting.
 * Aligns requested size to ALIGNMENT bytes.
 * Returns NULL on failure (out of memory).
 */
void *mem_alloc(size_t size) {
    if (!initialized) mem_init();
    if (size == 0) return NULL;

    size = ALIGN(size);   /* enforce 8-byte alignment */

    /* First-fit search */
    Block *cur = heap_start;
    while (cur) {
        if (!valid_block(cur)) {
            fprintf(stderr, "[mem_alloc] HEAP CORRUPTION detected at %p!\n",
                    (void *)cur);
            return NULL;
        }
        if (cur->free && cur->size >= size) {
            split_block(cur, size);
            cur->free = 0;
            return payload(cur);
        }
        cur = cur->next;
    }

    fprintf(stderr, "[mem_alloc] Out of memory! Requested %zu bytes.\n", size);
    return NULL;
}

/*
 * mem_free — mark block as free, then coalesce with neighbours
 * to reduce external fragmentation.
 */
void mem_free(void *ptr) {
    if (!ptr) return;

    Block *b = header(ptr);
    if (!valid_block(b)) {
        fprintf(stderr, "[mem_free] Invalid or already-freed pointer %p!\n", ptr);
        return;
    }
    if (b->free) {
        fprintf(stderr, "[mem_free] Double-free detected at %p!\n", ptr);
        return;
    }

    b->free = 1;

    /* Coalesce: try merging with next, then with prev */
    b = coalesce_next(b);
    b = coalesce_prev(b);
}

/*
 * mem_realloc — resize an existing allocation.
 * If ptr is NULL, behaves like mem_alloc.
 * If new_size is 0, behaves like mem_free.
 */
void *mem_realloc(void *ptr, size_t new_size) {
    if (!ptr)        return mem_alloc(new_size);
    if (!new_size)   { mem_free(ptr); return NULL; }

    Block *b = header(ptr);
    if (!valid_block(b)) {
        fprintf(stderr, "[mem_realloc] Invalid pointer %p\n", ptr);
        return NULL;
    }

    new_size = ALIGN(new_size);

    /* Already big enough — optionally split off the excess */
    if (b->size >= new_size) {
        split_block(b, new_size);
        return ptr;
    }

    /* Need a bigger block: allocate, copy, free old */
    void *new_ptr = mem_alloc(new_size);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, b->size);   /* copy only what was used */
    mem_free(ptr);
    return new_ptr;
}

/*
 * mem_calloc — allocate and zero-initialise (like calloc).
 */
void *mem_calloc(size_t count, size_t size) {
    size_t total = count * size;
    void  *ptr   = mem_alloc(total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

/* ── Diagnostics ─────────────────────────────────────────────────────────── */

/*
 * mem_dump — print every block: address, size, status.
 * Useful for step-by-step debugging (like GDB watch points).
 */
void mem_dump(void) {
    printf("\n=== HEAP DUMP ==========================================\n");
    printf("%-18s %-10s %-8s %-10s\n", "Address", "Size", "Status", "Magic");
    printf("--------------------------------------------------------\n");

    Block *cur = heap_start;
    int    idx = 0;
    while (cur) {
        printf("[%3d] %p  %-10zu %-8s 0x%08X%s\n",
               idx++,
               (void *)cur,
               cur->size,
               cur->free ? "FREE" : "USED",
               cur->magic,
               cur->magic != MAGIC_NUMBER ? " <-- CORRUPT!" : "");
        cur = cur->next;
    }
    printf("========================================================\n\n");
}

/*
 * mem_stats — summary statistics about the heap.
 */
void mem_stats(void) {
    size_t used_bytes  = 0, free_bytes  = 0;
    int    used_blocks = 0, free_blocks = 0;
    int    fragments   = 0;
    int    prev_free   = 0;

    Block *cur = heap_start;
    while (cur) {
        if (cur->free) {
            free_bytes += cur->size;
            free_blocks++;
            if (prev_free) fragments++;   /* two consecutive free = fragment */
            prev_free = 1;
        } else {
            used_bytes += cur->size;
            used_blocks++;
            prev_free = 0;
        }
        cur = cur->next;
    }

    printf("\n=== HEAP STATISTICS =====================================\n");
    printf("  Heap capacity   : %d bytes\n",   HEAP_CAPACITY);
    printf("  Used            : %zu bytes  (%d blocks)\n", used_bytes,  used_blocks);
    printf("  Free            : %zu bytes  (%d blocks)\n", free_bytes,  free_blocks);
    printf("  Overhead        : %zu bytes  (%d headers @ %zu bytes each)\n",
           (used_blocks + free_blocks) * BLOCK_SIZE,
           used_blocks + free_blocks, BLOCK_SIZE);
    printf("  Fragmentation   : %d adjacent free block pair(s)\n", fragments);
    printf("=========================================================\n\n");
}

/*
 * mem_check — walk the heap and verify every magic number.
 * Returns 1 if heap is intact, 0 if corruption is detected.
 */
int mem_check(void) {
    Block *cur = heap_start;
    int    ok  = 1;
    printf("[mem_check] Walking heap...\n");
    while (cur) {
        if (cur->magic != MAGIC_NUMBER) {
            fprintf(stderr, "  CORRUPT block at %p (magic=0x%08X)\n",
                    (void *)cur, cur->magic);
            ok = 0;
        }
        cur = cur->next;
    }
    printf("[mem_check] %s\n", ok ? "Heap OK." : "CORRUPTION FOUND!");
    return ok;
}
