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

// protótipos internos
static void *get_free_page(void);
static void free_page(void *addr);

void *kmalloc(size_t size) {
    // alinhamento 8
    size = (size + 7) & ~7UL;
    uint8_t *p = heap_ptr;
    heap_ptr += size;
    if ((size_t)(heap_ptr - HEAP_START) >= HEAP_SIZE) return NULL;
    return p;
}

void kfree(void *ptr) {
    // marcar página como livre se pertencer ao heap
    if (!ptr) return;
    free_page(ptr);
}

static void *get_free_page(void) {
    for (size_t i = 0; i < MAX_PAGES; i++) {
        if (page_is_free(i)) {
            set_page_used(i);
            return (void*)(HEAP_START + i * PAGE_SIZE);
        }
    }
    return NULL;
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
    size_t pages = (length + PAGE_SIZE -1)/PAGE_SIZE;
    uint8_t *base = NULL;
    for (size_t i=0;i<pages;i++) {
        void *p = get_free_page();
        if (!p) { // rollback
            for (size_t j=0;j<i;j++) free_page(base + j*PAGE_SIZE);
            return NULL;
        }
        if (i==0) base = p;
    }
    return base;
}

int munmap_user(void *addr, size_t length) {
    size_t pages = (length + PAGE_SIZE -1)/PAGE_SIZE;
    uint8_t *b = addr;
    for (size_t i=0;i<pages;i++) free_page(b + i*PAGE_SIZE);
    return 0;
}
