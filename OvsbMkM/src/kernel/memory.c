#include "memory.h"
#include "kernel.h"
#include <stdint.h>

// Heap simples bump allocator + page bitmap para mmap
// Região de heap: 0x900000 - 0x4900000 (64MB)
#define HEAP_START ((uint8_t*)0x900000)
#define HEAP_SIZE  (64 * 1024 * 1024)

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
    (void)ptr;
    // Bump allocator: no free. kfree is intentionally a no-op.
    // Kernel objects are permanent (no leak in practice since
    // kmalloc is unused and the 4MB heap is enough for boot).
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

void munmap_all_user(void) {
    for (size_t i = 0; i < MAX_PAGES; i++)
        set_page_free(i);
}

uint64_t pml4_get_current(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static void *page_alloc(void) {
    return mmap_user(0, 4096, 3, 0);
}

static void page_free(void *p) {
    munmap_user(p, 4096);
}

uint64_t pml4_create(void) {
    uint64_t *current = (uint64_t *)pml4_get_current();
    uint64_t *new_pml4 = page_alloc();
    if (!new_pml4) return 0;
    // Copy all entries — shares kernel page tables.
    // Future: copy only high-half kernel mappings and create
    // private user page tables for full isolation.
    for (int i = 0; i < 512; i++)
        new_pml4[i] = current[i];
    return (uint64_t)(uintptr_t)new_pml4;
}

void pml4_load(uint64_t pml4_pa) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_pa) : "memory");
}

void pml4_restore(uint64_t pml4_pa) {
    pml4_load(pml4_pa);
}

void pml4_destroy(uint64_t pml4_pa) {
    page_free((void *)pml4_pa);
}

int pml4_map_phys(uint64_t pml4_pa, uint64_t virt_addr, uint64_t phys_addr,
                  size_t size, int writable) {
    uint64_t entry_flags = 0x83;
    if (writable) entry_flags |= 0x04;

    uint64_t *pml4 = (uint64_t *)(uintptr_t)pml4_pa;

    for (uint64_t offset = 0; offset < size; offset += 0x200000) {
        uint64_t va = virt_addr + offset;
        uint64_t pa = phys_addr + offset;

        int pml4_idx = (va >> 39) & 0x1FF;
        int pdpt_idx = (va >> 30) & 0x1FF;
        int pd_idx   = (va >> 21) & 0x1FF;

        if (!(pml4[pml4_idx] & 1)) {
            uint64_t *pdpt = mmap_user(0, 4096, 3, 0);
            if (!pdpt) return -1;
            for (int i = 0; i < 512; i++) pdpt[i] = 0;
            pml4[pml4_idx] = (uint64_t)(uintptr_t)pdpt | 0x03;
        }

        uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & ~0xFFF);

        if (!(pdpt[pdpt_idx] & 1)) {
            uint64_t *pd = mmap_user(0, 4096, 3, 0);
            if (!pd) return -1;
            for (int i = 0; i < 512; i++) pd[i] = 0;
            pdpt[pdpt_idx] = (uint64_t)(uintptr_t)pd | 0x03;
        }

        uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFF);
        pd[pd_idx] = pa | entry_flags;
    }

    __asm__ volatile ("mov %0, %%cr3" : : "r"(pml4_pa) : "memory");
    return 0;
}
