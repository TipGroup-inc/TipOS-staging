#include "memory.h"
#include "kernel.h"
#include <stdint.h>

// Heap simples bump allocator + page bitmap para mmap
// Região de heap: 0x900000 - 0x940000 (4MB)
#define HEAP_START ((uint8_t*)0x900000)
#define HEAP_SIZE  (4 * 1024 * 1024)

#define PAGE_SIZE 4096
#define MAX_PAGES (HEAP_SIZE / PAGE_SIZE)

static uint8_t *heap_ptr = HEAP_START;
static uint8_t page_bitmap[MAX_PAGES/8];

void memory_init(void) {
    // zerar bitmap
    for (size_t i=0;i<sizeof(page_bitmap);i++) page_bitmap[i]=0;
    heap_ptr = HEAP_START;
}

static void set_page_used(size_t idx) { page_bitmap[idx/8] |= (1 << (idx&7)); }
static void set_page_free(size_t idx) { page_bitmap[idx/8] &= ~(1 << (idx&7)); }
static int page_is_free(size_t idx) { return !(page_bitmap[idx/8] & (1 << (idx&7))); }

static void free_page(void *addr);

void *kmalloc(size_t size) {
    size = (size + 7) & ~7UL;
    uint8_t *p = heap_ptr;
    heap_ptr += size;
    if ((size_t)(heap_ptr - HEAP_START) >= HEAP_SIZE) return NULL;
    return p;
}

void kfree(void *ptr) {
    if (!ptr) return;
    free_page(ptr);
}

static void free_page(void *addr) {
    if (!addr) return;
    uintptr_t off = (uint8_t*)addr - HEAP_START;
    if ((uintptr_t)off >= HEAP_SIZE) return;
    size_t idx = off / PAGE_SIZE;
    set_page_free(idx);
}

void *mmap_user(void *addr, size_t length, int prot, int flags) {
    (void)addr; (void)prot; (void)flags;
    size_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    /* Find a contiguous run of free pages */
    size_t run_start = 0;
    size_t run_len = 0;
    for (size_t i = 0; i < MAX_PAGES; i++) {
        if (page_is_free(i)) {
            if (run_len == 0) run_start = i;
            run_len++;
            if (run_len >= pages) {
                uint8_t *base = HEAP_START + run_start * PAGE_SIZE;
                for (size_t j = 0; j < pages; j++)
                    set_page_used(run_start + j);
                return base;
            }
        } else {
            run_len = 0;
        }
    }
    return NULL;
}

int munmap_user(void *addr, size_t length) {
    size_t pages = (length + PAGE_SIZE - 1) / PAGE_SIZE;
    uint8_t *b = addr;
    size_t start_idx = ((uintptr_t)b - (uintptr_t)HEAP_START) / PAGE_SIZE;
    for (size_t i = 0; i < pages; i++)
        set_page_free(start_idx + i);
    return 0;
}
