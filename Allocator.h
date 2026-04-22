#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>  /* size_t */
#include <stdio.h>

/* ── Block header stored just before every allocation ── */
typedef struct Block {
    size_t        size;     /* usable payload size in bytes        */
    int           free;     /* 1 = free, 0 = in use                */
    struct Block *next;     /* next block in the free/used list     */
    struct Block *prev;     /* previous block (for coalescing)      */
    unsigned int  magic;    /* 0xDEADBEEF sanity check             */
} Block;

#define BLOCK_SIZE      sizeof(Block)
#define MAGIC_NUMBER    0xDEADBEEF
#define HEAP_CAPACITY   (1024 * 1024)   /* 1 MB static heap         */
#define MIN_SPLIT_SIZE  32              /* don't split tiny blocks   */
#define ALIGNMENT       8              /* 8-byte alignment          */
#define ALIGN(size)     (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

/* ── Public API  (drop-in replacements for malloc / free / realloc) ── */
void  mem_init   (void);
void *mem_alloc  (size_t size);
void  mem_free   (void *ptr);
void *mem_realloc(void *ptr, size_t new_size);
void *mem_calloc (size_t count, size_t size);

/* ── Diagnostics ── */
void  mem_dump   (void);          /* print every block              */
void  mem_stats  (void);          /* summary: used / free / frags   */
int   mem_check  (void);          /* heap integrity check (returns 1=ok) */

#endif /* ALLOCATOR_H */
