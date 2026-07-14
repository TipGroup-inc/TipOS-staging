#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

#define PROT_READ  1
#define PROT_WRITE 2
#define PROT_EXEC  4

#define MAP_PRIVATE 2
#define MAP_ANON    0x1000

void memory_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
void *kcalloc(size_t count, size_t size);
void *mmap_user(void *addr, size_t length, int prot, int flags);
int munmap_user(void *addr, size_t length);
void munmap_all_user(void);

uint64_t pml4_get_current(void);
uint64_t pml4_create(void);
void pml4_load(uint64_t pml4_pa);
void pml4_restore(uint64_t pml4_pa);
void pml4_destroy(uint64_t pml4_pa);
int pml4_map_phys(uint64_t pml4_pa, uint64_t virt_addr, uint64_t phys_addr,
                  size_t size, int writable);

#endif
