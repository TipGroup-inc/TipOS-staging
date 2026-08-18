/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: vm_map.c ~ funcoes anotadas: 19
 */
#include "vm_map.h"
#include "memory.h"
#include "serial.h"
#include "process.h"
#include <stdint.h>
#include <stddef.h>

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static uint64_t vm_alloc_page(void) {
    void *p = mmap_user(NULL, 4096, PROT_READ | PROT_WRITE, MAP_ANON);
    return p ? (uint64_t)(uintptr_t)p : 0;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void vm_free_page(uint64_t pa) {
    munmap_user((void *)(uintptr_t)pa, 4096);
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static vm_map_entry_t *entry_alloc(void) {
    return (vm_map_entry_t *)kmalloc(sizeof(vm_map_entry_t));
}

/* ~ essa demorou pra debugar, respeita ~ */
static void entry_free(vm_map_entry_t *e) {
    kfree(e);
}

/* ~ essa demorou pra debugar, respeita ~ */
vm_object_t *vm_object_create(size_t size) {
    vm_object_t *obj = (vm_object_t *)kmalloc(sizeof(vm_object_t));
    if (!obj) return NULL;
    int npages = (size + 0xFFF) >> 12;
    if (npages == 0) npages = 1;
    obj->type = VM_OBJ_ANON;
    obj->refcnt = 1;
    obj->size = (size_t)npages << 12;
    obj->pages = (uint64_t *)kcalloc(npages, sizeof(uint64_t));
    obj->page_count = npages;
    if (!obj->pages) { kfree(obj); return NULL; }
    for (int i = 0; i < npages; i++)
        obj->pages[i] = 0;
    return obj;
}

/* ~ cuidado que essa aqui morde ~ */
vm_object_t *vm_object_share(vm_object_t *obj) {
    if (!obj) return NULL;
    obj->refcnt++;
    return obj;
}

/* ~ essa demorou pra debugar, respeita ~ */
void vm_object_destroy(vm_object_t *obj) {
    if (!obj) return;
    if (--obj->refcnt > 0) return;
    for (int i = 0; i < obj->page_count; i++)
        if (obj->pages[i])
            vm_free_page(obj->pages[i]);
    kfree(obj->pages);
    kfree(obj);
}

/* ~ cuidado que essa aqui morde ~ */
static uint64_t obj_get_page(vm_object_t *obj, size_t idx) {
    if (idx >= (size_t)obj->page_count) return 0;
    if (!obj->pages[idx]) {
        uint64_t pa = vm_alloc_page();
        if (!pa) return 0;
        obj->pages[idx] = pa;
    }
    return obj->pages[idx];
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void vm_map_init(vm_map_t *map, uint64_t min, uint64_t max) {
    map->head = NULL;
    map->nentries = 0;
    map->min_offset = min;
    map->max_offset = max;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static int entry_covers(vm_map_entry_t *e, uint64_t start, uint64_t end) {
    return e->start <= start && e->end >= end;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
vm_map_entry_t *vm_map_find_entry(vm_map_t *map, uint64_t addr) {
    vm_map_entry_t *e = map->head;
    while (e) {
        if (addr >= e->start && addr < e->end) return e;
        e = e->next;
    }
    return NULL;
}

/* ~ essa demorou pra debugar, respeita ~ */
static int overlap(vm_map_t *map, uint64_t start, uint64_t end) {
    vm_map_entry_t *e = map->head;
    while (e) {
        if (e->start < end && e->end > start) return 1;
        e = e->next;
    }
    return 0;
}

int vm_map_find(vm_map_t *map, vm_object_t *obj, uint64_t offset,
                uint64_t *addr, size_t size, int prot, int flags) {
    size = (size + 0xFFF) & ~0xFFFULL;
    if (size == 0) return -1;

    uint64_t hint = *addr;
    uint64_t candidate = hint ? hint : 0x70000000ULL;
    if (candidate < map->min_offset) candidate = map->min_offset;
    candidate = (candidate + 0xFFF) & ~0xFFFULL;

    int tries = 0;
    while (candidate + size <= map->max_offset && tries < 100) {
        if (!overlap(map, candidate, candidate + size))
            goto found;
        candidate += 0x100000;
        tries++;
    }
    return -1;

found:
    *addr = candidate;
    return vm_map_fixed(map, obj, offset, candidate, size, prot, flags);
}

int vm_map_fixed(vm_map_t *map, vm_object_t *obj, uint64_t offset,
                 uint64_t addr, size_t size, int prot, int flags) {
    size = (size + 0xFFF) & ~0xFFFULL;
    if (size == 0 || (addr & 0xFFF) != 0) return -1;
    if (overlap(map, addr, addr + size)) return -1;

    vm_map_entry_t *e = entry_alloc();
    if (!e) return -1;
    e->start = addr;
    e->end = addr + size;
    e->object = obj ? vm_object_share(obj) : NULL;
    e->offset = offset;
    e->prot = prot;
    e->flags = flags;

    vm_map_entry_t **pp = &map->head;
    while (*pp && (*pp)->start < addr)
        pp = &(*pp)->next;
    e->next = *pp;
    e->prev = NULL;
    if (*pp) {
        (*pp)->prev = e;
    }
    *pp = e;
    map->nentries++;
    return 0;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int vm_map_remove(vm_map_t *map, uint64_t start, uint64_t end) {
    (void)end;
    vm_map_entry_t *e = map->head;
    while (e) {
        vm_map_entry_t *next = e->next;
        if (e->start >= start && e->end <= end) {
            if (e->prev) e->prev->next = e->next;
            else map->head = e->next;
            if (e->next) e->next->prev = e->prev;
            if (e->object) vm_object_destroy(e->object);
            entry_free(e);
            map->nentries--;
        }
        e = next;
    }
    return 0;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
int vm_map_protect(vm_map_t *map, uint64_t start, uint64_t end, int prot) {
    vm_map_entry_t *e = map->head;
    while (e) {
        if (e->start < end && e->end > start) {
            e->prot = prot;
        }
        e = e->next;
    }
    return 0;
}

/* ~ essa demorou pra debugar, respeita ~ */
int vm_map_wire(vm_map_t *map, uint64_t start, size_t size, uint64_t pml4) {
    uint64_t end = start + size;
    vm_map_entry_t *e = map->head;
    while (e) {
        if (e->start < end && e->end > start) {
            uint64_t ws = e->start > start ? e->start : start;
            uint64_t we = e->end < end ? e->end : end;
            vm_object_t *obj = e->object;
            if (!obj && (e->flags & MAP_ANON)) {
                obj = vm_object_create(e->end - e->start);
                if (!obj) return -1;
                e->object = obj;
            }
            if (obj) {
                uint64_t a_start = ws & ~0x1FFFFFULL;
                uint64_t a_end   = (we + 0x1FFFFF) & ~0x1FFFFFULL;
                for (uint64_t chunk = a_start; chunk < a_end; chunk += 0x200000) {
                    int pml4_idx = (chunk >> 39) & 0x1FF;
                    int pdpt_idx = (chunk >> 30) & 0x1FF;
                    int pd_idx   = (chunk >> 21) & 0x1FF;
                    uint64_t *p4 = (uint64_t *)(uintptr_t)pml4;
                    if (!(p4[pml4_idx] & 1)) {
                        uint64_t *p = mmap_user(0,4096,3,0); if(!p) return -1;
                        for(int i=0;i<512;i++) p[i]=0;
                        p4[pml4_idx] = (uint64_t)(uintptr_t)p | 0x07;
                    }
                    uint64_t *pdpt = (uint64_t *)(uintptr_t)(p4[pml4_idx] & ~0xFFFULL);
                    if (!(pdpt[pdpt_idx] & 1)) {
                        uint64_t *p = mmap_user(0,4096,3,0); if(!p) return -1;
                        for(int i=0;i<512;i++) p[i]=0;
                        pdpt[pdpt_idx] = (uint64_t)(uintptr_t)p | 0x07;
                    }
                    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);
                    uint64_t old = pd[pd_idx];
                    uint64_t *pt;
                    if ((old & 1) && (old & 0x80)) {
                        pt = mmap_user(0,4096,3,0); if(!pt) return -1;
                        for (int i = 0; i < 512; i++) {
                            uint64_t va = chunk + (uint64_t)i * 0x1000;
                            uint64_t pa = 0;
                            if (va >= ws && va < we) {
                                size_t pg = (va - e->start + e->offset) >> 12;
                                pa = obj_get_page(obj, pg);
                                if (!pa) return -1;
                            }
                            pt[i] = pa ? (pa | 0x07) : 0;
                        }
                        pd[pd_idx] = (uint64_t)(uintptr_t)pt | 0x07;
                        __sync_synchronize();
                    } else {
                        if (!(old & 1)) {
                            pt = mmap_user(0,4096,3,0); if(!pt) return -1;
                            for (int i = 0; i < 512; i++) pt[i] = 0;
                            pd[pd_idx] = (uint64_t)(uintptr_t)pt | 0x07;
                        } else {
                            pt = (uint64_t *)(uintptr_t)(old & ~0xFFFULL);
                        }
                        for (int i = 0; i < 512; i++) {
                            uint64_t va = chunk + (uint64_t)i * 0x1000;
                            if (va >= ws && va < we && !(pt[i] & 1)) {
                                size_t pg = (va - e->start + e->offset) >> 12;
                                uint64_t pa = obj_get_page(obj, pg);
                                if (!pa) return -1;
                                pt[i] = pa | 0x07;
                            }
                        }
                    }
                }
            }
        }
        e = e->next;
    }
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4) : "memory");
    return 0;
}

/* ~ essa demorou pra debugar, respeita ~ */
vmspace_t *vmspace_create(void) {
    vmspace_t *vs = (vmspace_t *)kmalloc(sizeof(vmspace_t));
    if (!vs) return NULL;
    vm_map_init(&vs->vm_map, 0x10000, 0x7FFFF000ULL);
    vs->pml4 = pml4_create();
    vs->stack_top = 0;
    return vs;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void vmspace_destroy(vmspace_t *vs) {
    if (!vs) return;
    vm_map_remove(&vs->vm_map, vs->vm_map.min_offset, vs->vm_map.max_offset);
    pml4_destroy(vs->pml4);
    kfree(vs);
}

/* ~ cuidado que essa aqui morde ~ */
void vmspace_swap_pml4(vmspace_t *vs, uint64_t new_pml4) {
    vs->pml4 = new_pml4;
}

/* ~~ walk do pml4: aplica funcao nas PTEs de um range ~~
 * split_2mb_pde quebra huge pages, depois cada PTE 4KB é ajustada.
 * Retorna 0 se todas as PTEs do range foram tocadas. */
static int pml4_walk_ptes(uint64_t pml4, uint64_t start, uint64_t end,
                          void (*fn)(uint64_t *pte, uint64_t va, void *ctx),
                          void *ctx) {
    uint64_t *p4 = (uint64_t *)(uintptr_t)pml4;
    for (uint64_t chunk = start & ~0x1FFFFFULL; chunk < end; chunk += 0x200000) {
        int pml4_idx = (chunk >> 39) & 0x1FF;
        int pdpt_idx = (chunk >> 30) & 0x1FF;
        int pd_idx   = (chunk >> 21) & 0x1FF;
        if (!(p4[pml4_idx] & 1)) continue;
        uint64_t *pdpt = (uint64_t *)(uintptr_t)(p4[pml4_idx] & ~0xFFFULL);
        if (!(pdpt[pdpt_idx] & 1)) continue;
        uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);
        uint64_t pde = pd[pd_idx];
        if (!(pde & 1)) continue;
        if (pde & 0x80) {
            if (split_2mb_pde(pml4, chunk) < 0) return -1;
            pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);
        }
        uint64_t *pt = (uint64_t *)(uintptr_t)(pd[pd_idx] & ~0xFFFULL);
        for (int i = 0; i < 512; i++) {
            uint64_t va = chunk + (uint64_t)i * 0x1000;
            if (va >= start && va < end && (pt[i] & 1))
                fn(&pt[i], va, ctx);
        }
    }
    return 0;
}

/* ~~ callback: aplica R/W + U/S conforme prot ~~ */
static void pte_set_prot(uint64_t *pte, uint64_t va, void *ctx) {
    (void)va;
    int prot = *(int *)ctx;
    uint64_t f = *pte;
    if (prot & 2) f |= 0x02; else f &= ~0x02ULL;
    f |= 0x04;  /* U/S sempre ligado — userland */
    if (!(prot & 1)) f &= ~0x02ULL;  /* sem READ = sem WRITE (Linux: R implica W) */
    if (!(prot & 4)) f |= 0x8000000000000000ULL;  /* NX se sem EXEC */
    else f &= ~0x8000000000000000ULL;
    *pte = f;
}

/* ~~ callback: zera a PTE (unmap) ~~ */
static void pte_clear(uint64_t *pte, uint64_t va, void *ctx) {
    (void)va; (void)ctx;
    *pte = 0;
}

/* ~~ vm_munmap: desmapeia PTEs e remove entries do range ~~ */
int vm_munmap(uint64_t addr, size_t size) {
    pcb_t *cur = process_current();
    if (!cur) return -1;
    vm_map_t *map = (vm_map_t *)cur->vm_map;
    if (!map) return -1;
    size = (size + 0xFFF) & ~0xFFFULL;
    if (size == 0) return 0;
    uint64_t end = addr + size;
    pml4_walk_ptes(cur->pml4, addr, end, pte_clear, NULL);
    __asm__ volatile("invlpg %0" : : "m"(*(uint8_t *)addr) : "memory");
    vm_map_remove(map, addr, end);
    return 0;
}

/* ~~ vm_mprotect: muda permissoes das PTEs do range ~~ */
int vm_mprotect(uint64_t addr, size_t size, int prot) {
    pcb_t *cur = process_current();
    if (!cur) return -1;
    vm_map_t *map = (vm_map_t *)cur->vm_map;
    if (!map) return -1;
    size = (size + 0xFFF) & ~0xFFFULL;
    if (size == 0) return 0;
    uint64_t end = addr + size;
    if (vm_map_protect(map, addr, end, prot) < 0) return -1;
    pml4_walk_ptes(cur->pml4, addr, end, pte_set_prot, &prot);
    return 0;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int vm_mmap(uint64_t *addr, size_t size, int prot, int flags) {
    pcb_t *cur = process_current();
    if (!cur) return -1;
    vm_map_t *map = (vm_map_t *)cur->vm_map;
    if (!map) return -1;

    size = (size + 0xFFF) & ~0xFFFULL;
    if (size == 0) return -1;

    vm_object_t *obj = NULL;
    if (flags & MAP_ANON) {
        obj = vm_object_create(size);
        if (!obj) return -1;
    }

    uint64_t map_addr = *addr;
    int r;
    if (flags & MAP_FIXED) {
        r = vm_map_fixed(map, obj, 0, map_addr, size, prot, flags);
    } else {
        r = vm_map_find(map, obj, 0, &map_addr, size, prot, flags);
    }
    if (r < 0) {
        if (obj) vm_object_destroy(obj);
        return -1;
    }

    if (vm_map_wire(map, map_addr, size, cur->pml4) < 0) {
        vm_map_remove(map, map_addr, map_addr + size);
        return -1;
    }

    *addr = map_addr;
    return 0;
}

/* ♥ vm_map.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
