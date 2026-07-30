/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: vm_map.h ~ funcoes anotadas: 0
 */
#ifndef VM_MAP_H
#define VM_MAP_H

#include <stdint.h>
#include <stddef.h>

#define VM_PROT_NONE   0
#define VM_PROT_READ   1
#define VM_PROT_WRITE  2
#define VM_PROT_EXEC   4
#define VM_PROT_ALL    7

#define MAP_SHARED     1
#define MAP_PRIVATE    2
#define MAP_ANON       0x1000
#define MAP_FIXED      0x0010

#define VM_OBJ_ANON    0

typedef struct vm_object vm_object_t;
typedef struct vm_map_entry vm_map_entry_t;
typedef struct vm_map vm_map_t;
typedef struct vmspace vmspace_t;

struct vm_object {
    int type;
    int refcnt;
    size_t size;
    uint64_t *pages;
    int page_count;
};

struct vm_map_entry {
    vm_map_entry_t *prev, *next;
    uint64_t start, end;
    vm_object_t *object;
    uint64_t offset;
    int prot;
    int flags;
};

struct vm_map {
    vm_map_entry_t *head;
    int nentries;
    uint64_t min_offset, max_offset;
};

struct vmspace {
    vm_map_t vm_map;
    uint64_t pml4;
    uint64_t stack_top;
};

vm_object_t *vm_object_create(size_t size);
vm_object_t *vm_object_share(vm_object_t *obj);
void vm_object_destroy(vm_object_t *obj);

void vm_map_init(vm_map_t *map, uint64_t min, uint64_t max);
int vm_map_find(vm_map_t *map, vm_object_t *obj, uint64_t offset,
                uint64_t *addr, size_t size, int prot, int flags);
int vm_map_fixed(vm_map_t *map, vm_object_t *obj, uint64_t offset,
                 uint64_t addr, size_t size, int prot, int flags);
int vm_map_remove(vm_map_t *map, uint64_t start, uint64_t end);
int vm_map_protect(vm_map_t *map, uint64_t start, uint64_t end, int prot);

vm_map_entry_t *vm_map_find_entry(vm_map_t *map, uint64_t addr);
int vm_map_wire(vm_map_t *map, uint64_t start, size_t size, uint64_t pml4);

vmspace_t *vmspace_create(void);
void vmspace_destroy(vmspace_t *vs);
void vmspace_swap_pml4(vmspace_t *vs, uint64_t new_pml4);

int vm_mmap(uint64_t *addr, size_t size, int prot, int flags);

#endif

/* ♥ vm_map.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
