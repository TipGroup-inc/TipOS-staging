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

#endif
