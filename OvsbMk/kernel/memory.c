/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: memory.c ~ funcoes anotadas: 31
 */
/* ♥ memory.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ SPINLOCK ~ Lock atomico! "__sync_lock_test_and_set protege tudo~"
 * mem_lock/mem_unlock com __sync_lock_test_and_set + pause
 * Protege todas as operacoes do buddy allocator e SLUB!
 * Single-core safe ~ se travar, culpe o scheduler! moe~ */

/* ♥ Per-CPU pools ~ Fast paths sem lock pra alloc comum!
 * pcpu_pages: pool de paginas soltas (order 0) pra alloc rapida
 * pcpu_objects[cache]: pool de objetos pre-free pra cada kmem_cache
 * Quando o pool enche, drena pro slab. Quando esvazia, recarrega do slab.
 * Tudo single-core por enquanto, mas a estrutura é SMP-ready! >_< */

#include "memory.h"
#include "serial.h"
#include <stdint.h>

#define HEAP_PHYS    0xA00000u
#define HEAP_SIZE    (64u * 1024u * 1024u)
#define FRAME_SIZE   4096u
#define FRAME_SHIFT  12u
#define FRAME_COUNT  (HEAP_SIZE / FRAME_SIZE)
#define MAX_ORDER    14u

#define FRAME_ALLOC  0x80u
#define FRAME_USER   0x40u
#define FRAME_ORDER  0x3Fu

#define SLAB_MAGIC   0x534C4142u
#define CACHE_COUNT  8u

/* Per-CPU pool constants */
#define PCPU_PAGE_POOL   32
#define PCPU_OBJ_CACHE   16
#define PCPU_OBJ_REFILL  8

static uint8_t frames[FRAME_COUNT];

typedef struct free_block {
    struct free_block *next;
    struct free_block *prev;
} free_block_t;

static free_block_t *free_area[MAX_ORDER + 1];

typedef struct kmem_cache kmem_cache_t;

typedef struct slab {
    uint32_t      magic;
    uint32_t      in_use;
    uint16_t      total;
    void         *freelist;
    struct slab  *next;
    struct slab  *prev;
    kmem_cache_t *cache;
} slab_t;

struct kmem_cache {
    size_t  obj_size;
    slab_t *slabs;
    int     obj_per_slab;
    int     id;
};

static kmem_cache_t kmalloc_caches[CACHE_COUNT];

/* Per-CPU page pool */
static struct {
    void *pages[PCPU_PAGE_POOL];
    int count;
} pcpu_pages;

/* Per-CPU object freelists for each cache */
static struct {
    void *objs[PCPU_OBJ_CACHE];
    int count;
} pcpu_objects[CACHE_COUNT];

static volatile int memlock = 0;

/* ~ essa demorou pra debugar, respeita ~ */
static inline void mem_lock(void) {
    while (__sync_lock_test_and_set(&memlock, 1))
        __asm__ volatile("pause");
    __asm__ volatile("" ::: "memory");
}

/* ~ essa demorou pra debugar, respeita ~ */
static inline void mem_unlock(void) {
    __asm__ volatile("" ::: "memory");
    __sync_lock_release(&memlock);
}

/* ~ essa demorou pra debugar, respeita ~ */
static inline size_t addr_to_frame(uint64_t addr) {
    return (addr - HEAP_PHYS) >> FRAME_SHIFT;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static inline uint64_t frame_to_addr(size_t idx) {
    return HEAP_PHYS + (idx << FRAME_SHIFT);
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void list_add(free_block_t **head, free_block_t *blk) {
    blk->next = *head;
    blk->prev = NULL;
    if (*head) (*head)->prev = blk;
    *head = blk;
}

/* ~ cuidado que essa aqui morde ~ */
static void list_remove(free_block_t **head, free_block_t *blk) {
    if (blk->prev)
        blk->prev->next = blk->next;
    else
        *head = blk->next;
    if (blk->next)
        blk->next->prev = blk->prev;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void *buddy_alloc(int order, int user) {
    if (order < 0 || order > MAX_ORDER) return NULL;
    serial_puts("ba: lock...\r\n");
    mem_lock();
    serial_puts("ba: locked\r\n");
    int found_order = -1;
    for (int o = order; o <= MAX_ORDER; o++) {
        if (free_area[o]) { found_order = o; break; }
    }
    if (found_order < 0) { serial_puts("ba: no free\r\n"); mem_unlock(); return NULL; }
    serial_puts("ba: found o="); serial_puthex((uint32_t)found_order); serial_puts("\r\n");

    free_block_t *block = free_area[found_order];
    list_remove(&free_area[found_order], block);
    size_t block_idx = addr_to_frame((uint64_t)block);

    for (int o = found_order; o > order; o--) {
        size_t buddy_idx = block_idx ^ (1UL << (o - 1));
        free_block_t *buddy = (free_block_t *)frame_to_addr(buddy_idx);
        frames[buddy_idx] = (uint8_t)(o - 1);
        list_add(&free_area[o - 1], buddy);
    }

    frames[block_idx] = (uint8_t)(order | FRAME_ALLOC);
    if (user) frames[block_idx] |= FRAME_USER;
    mem_unlock();
    return (void *)frame_to_addr(block_idx);
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void buddy_free(void *addr, int order) {
    if (!addr || order < 0 || order > MAX_ORDER) return;
    uint64_t phys = (uint64_t)addr;
    if (phys < HEAP_PHYS || phys >= HEAP_PHYS + HEAP_SIZE) return;
    if (phys & (FRAME_SIZE - 1)) return;
    mem_lock();
    size_t idx = addr_to_frame(phys);
    frames[idx] = (uint8_t)order;
    while (order < MAX_ORDER) {
        size_t buddy_idx = idx ^ (1UL << order);
        if ((frames[buddy_idx] & (FRAME_ALLOC | FRAME_ORDER)) == (uint8_t)order) {
            free_block_t *buddy = (free_block_t *)frame_to_addr(buddy_idx);
            list_remove(&free_area[order], buddy);
            idx = (idx < buddy_idx) ? idx : buddy_idx;
            order++;
            frames[idx] = (uint8_t)order;
        } else {
            break;
        }
    }
    free_block_t *block = (free_block_t *)frame_to_addr(idx);
    list_add(&free_area[order], block);
    mem_unlock();
}

/* ~ cuidado que essa aqui morde ~ */
static void *slab_alloc(kmem_cache_t *cache) {
    if (!cache) return NULL;
    slab_t *s = cache->slabs;
    while (s) {
        if (s->freelist) {
            void *obj = s->freelist;
            s->freelist = *(void **)obj;
            s->in_use++;
            return obj;
        }
        s = s->next;
    }
    void *page = buddy_alloc(0, 0);
    if (!page) return NULL;
    s = (slab_t *)page;
    s->magic   = SLAB_MAGIC;
    s->in_use  = 0;
    s->total   = cache->obj_per_slab;
    s->cache   = cache;
    s->next    = cache->slabs;
    s->prev    = NULL;
    if (cache->slabs) cache->slabs->prev = s;
    cache->slabs = s;
    char *data = (char *)s + sizeof(slab_t);
    char *end  = (char *)s + FRAME_SIZE;
    void *head = NULL;
    int count = 0;
    while (data + cache->obj_size <= end) {
        *(void **)data = head;
        head = data;
        data += cache->obj_size;
        count++;
    }
    s->freelist = head;
    s->total = count;
    void *obj = s->freelist;
    s->freelist = *(void **)obj;
    s->in_use = 1;
    return obj;
}

/* ~ cuidado que essa aqui morde ~ */
static void slab_free(void *ptr) {
    if (!ptr) return;
    slab_t *s = (slab_t *)((uint64_t)ptr & ~(FRAME_SIZE - 1));
    if (s->magic != SLAB_MAGIC) return;
    *(void **)ptr = s->freelist;
    s->freelist = ptr;
    s->in_use--;
    if (s->in_use == 0) {
        kmem_cache_t *cache = s->cache;
        if (s->next) s->next->prev = s->prev;
        if (s->prev) s->prev->next = s->next;
        else if (cache) cache->slabs = s->next;
        buddy_free(s, 0);
    }
}

/* ~~ Per-CPU page pool: fast path para alloc/free de pages ~~ */

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void *pcpu_page_alloc(void) {
    if (pcpu_pages.count > 0)
        return pcpu_pages.pages[--pcpu_pages.count];
    return buddy_alloc(0, 0);
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void pcpu_page_free(void *p) {
    if (pcpu_pages.count < PCPU_PAGE_POOL)
        pcpu_pages.pages[pcpu_pages.count++] = p;
    else
        buddy_free(p, 0);
}

/* ~~ Per-CPU slab cache: refill e drain ~~ */

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void *pcpu_slab_alloc(kmem_cache_t *cache) {
    int id = cache->id;
    if (pcpu_objects[id].count > 0)
        return pcpu_objects[id].objs[--pcpu_objects[id].count];
    void *obj = slab_alloc(cache);
    if (!obj) return NULL;
    /* refill: grab extra objects from same slab */
    slab_t *s = (slab_t *)((uint64_t)obj & ~(FRAME_SIZE - 1));
    int want = PCPU_OBJ_REFILL;
    while (want > 0 && s->freelist) {
        void *extra = s->freelist;
        s->freelist = *(void **)extra;
        s->in_use++;
        if (pcpu_objects[id].count < PCPU_OBJ_CACHE)
            pcpu_objects[id].objs[pcpu_objects[id].count++] = extra;
        else
            break;
        want--;
    }
    return obj;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void pcpu_slab_free(void *ptr, slab_t *s) {
    kmem_cache_t *cache = s->cache;
    int id = cache->id;
    if (pcpu_objects[id].count < PCPU_OBJ_CACHE) {
        pcpu_objects[id].objs[pcpu_objects[id].count++] = ptr;
        return;
    }
    /* drain all per-CPU objects back to their respective slabs */
    while (pcpu_objects[id].count > 0) {
        void *obj = pcpu_objects[id].objs[--pcpu_objects[id].count];
        slab_t *os = (slab_t *)((uint64_t)obj & ~(FRAME_SIZE - 1));
        *(void **)obj = os->freelist;
        os->freelist = obj;
        os->in_use--;
        if (os->in_use == 0) {
            kmem_cache_t *oc = os->cache;
            if (os->next) os->next->prev = os->prev;
            if (os->prev) os->prev->next = os->next;
            else if (oc) oc->slabs = os->next;
            buddy_free(os, 0);
        }
    }
    /* free current object directly to its slab */
    *(void **)ptr = s->freelist;
    s->freelist = ptr;
    s->in_use--;
    if (s->in_use == 0) {
        if (s->next) s->next->prev = s->prev;
        if (s->prev) s->prev->next = s->next;
        else if (cache) cache->slabs = s->next;
        buddy_free(s, 0);
    }
}

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ cuidado que essa aqui morde ~ */
void memory_init(void) {
    mem_lock();
    for (int i = 0; i <= MAX_ORDER; i++)
        free_area[i] = NULL;
    for (size_t i = 0; i < FRAME_COUNT; i++)
        frames[i] = 0;
    free_block_t *block = (free_block_t *)(uint64_t)HEAP_PHYS;
    block->next = NULL;
    block->prev = NULL;
    free_area[MAX_ORDER] = block;
    frames[0] = MAX_ORDER;
    static const size_t sizes[CACHE_COUNT] = {
        16, 32, 64, 128, 256, 512, 1024, 2048
    };
    for (int i = 0; i < CACHE_COUNT; i++) {
        kmalloc_caches[i].obj_size    = sizes[i];
        kmalloc_caches[i].slabs       = NULL;
        kmalloc_caches[i].obj_per_slab =
            (int)((FRAME_SIZE - sizeof(slab_t)) / sizes[i]);
        kmalloc_caches[i].id          = i;
    }
    pcpu_pages.count = 0;
    for (int i = 0; i < CACHE_COUNT; i++)
        pcpu_objects[i].count = 0;
    mem_unlock();
}

/* ~ essa demorou pra debugar, respeita ~ */
void *kmalloc(size_t size) {
    if (size == 0) return NULL;
    if (size < 16) size = 16;
    if (size >= 2048) {
        size_t pages = (size + FRAME_SIZE - 1) / FRAME_SIZE;
        int order = 0;
        while ((1UL << order) < pages) order++;
        if (order == 0) return pcpu_page_alloc();
        return buddy_alloc(order, 0);
    }
    for (int i = 0; i < CACHE_COUNT; i++) {
        if (kmalloc_caches[i].obj_size >= size)
            return pcpu_slab_alloc(&kmalloc_caches[i]);
    }
    return NULL;
}

/* ~~ Liberando memória~~ não esquece de pagar o aluguel! */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void kfree(void *ptr) {
    if (!ptr) return;
    uint64_t addr = (uint64_t)ptr;
    if (addr < HEAP_PHYS || addr >= HEAP_PHYS + HEAP_SIZE) return;
    slab_t *s = (slab_t *)(addr & ~(FRAME_SIZE - 1));
    if (s->magic == SLAB_MAGIC) {
        if (s->cache) {
            pcpu_slab_free(ptr, s);
            return;
        }
        slab_free(ptr);
        return;
    }
    if (addr & (FRAME_SIZE - 1)) return;
    size_t idx = addr_to_frame(addr);
    if (idx >= FRAME_COUNT) return;
    if (!(frames[idx] & FRAME_ALLOC)) return;
    int order = frames[idx] & FRAME_ORDER;
    if (order == 0) { pcpu_page_free(ptr); return; }
    buddy_free(ptr, order);
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void *kcalloc(size_t count, size_t size) {
    size_t total = count * size;
    void *p = kmalloc(total);
    if (p) {
        size_t alloc_size = total;
        if (total < 2048) {
            for (int i = 0; i < CACHE_COUNT; i++) {
                if (kmalloc_caches[i].obj_size >= total) {
                    alloc_size = kmalloc_caches[i].obj_size;
                    break;
                }
            }
        } else {
            size_t pages = (total + FRAME_SIZE - 1) / FRAME_SIZE;
            int order = 0;
            while ((1UL << order) < pages) order++;
            alloc_size = (1UL << order) * FRAME_SIZE;
        }
        for (size_t i = 0; i < alloc_size; i++)
            ((uint8_t *)p)[i] = 0;
    }
    return p;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void *mmap_user(void *addr, size_t length, int prot, int flags) {
    (void)addr; (void)prot; (void)flags;
    size_t pages = (length + FRAME_SIZE - 1) / FRAME_SIZE;
    if (pages == 0) pages = 1;
    int order = 0;
    while ((1UL << order) < pages) order++;
    return buddy_alloc(order, 1);
}

/* ~~ munmap_user ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
int munmap_user(void *addr, size_t length) {
    (void)length;
    if (!addr) return 0;
    uint64_t a = (uint64_t)addr;
    if (a & (FRAME_SIZE - 1)) return -1;
    size_t idx = addr_to_frame(a);
    if (idx >= FRAME_COUNT) return -1;
    if (!(frames[idx] & FRAME_ALLOC)) return 0;
    int order = frames[idx] & FRAME_ORDER;
    buddy_free(addr, order);
    return 0;
}

/* ~~ munmap_all_user ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
void munmap_all_user(void) {
    mem_lock();
    for (size_t i = 0; i < FRAME_COUNT; ) {
        uint8_t f = frames[i];
        if ((f & (FRAME_ALLOC | FRAME_USER)) == (FRAME_ALLOC | FRAME_USER)) {
            int order = f & FRAME_ORDER;
            frames[i] = (uint8_t)order;
            size_t idx = i;
            int ord = order;
            while (ord < MAX_ORDER) {
                size_t buddy_idx = idx ^ (1UL << ord);
                if ((frames[buddy_idx] & (FRAME_ALLOC | FRAME_ORDER)) == (uint8_t)ord) {
                    free_block_t *buddy = (free_block_t *)frame_to_addr(buddy_idx);
                    list_remove(&free_area[ord], buddy);
                    idx = (idx < buddy_idx) ? idx : buddy_idx;
                    ord++;
                    frames[idx] = (uint8_t)ord;
                } else {
                    break;
                }
            }
            free_block_t *blk = (free_block_t *)frame_to_addr(idx);
            list_add(&free_area[ord], blk);
            i += (1UL << order);
        } else {
            i++;
        }
    }
    mem_unlock();
}

static void *page_alloc(void) { return pcpu_page_alloc(); }
static void page_free(void *p) { pcpu_page_free(p); }

/* ~~ Pegando o valor~ só confia que ta certo~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
uint64_t pml4_get_current(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

/* ~~ Criando coisa nova~~ que emocionante! */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
uint64_t pml4_create(void) {
    uint64_t *current = (uint64_t *)pml4_get_current();
    uint64_t *new_pml4 = page_alloc();
    if (!new_pml4) return 0;
    for (int i = 0; i < 512; i++)
        new_pml4[i] = current[i];
    return (uint64_t)(uintptr_t)new_pml4;
}

/* ~~ pml4_load ~~ */
/* ~ cuidado que essa aqui morde ~ */
void pml4_load(uint64_t pml4_pa) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_pa) : "memory");
}

/* ~~ pml4_restore ~~ */
void pml4_restore(uint64_t pml4_pa) { pml4_load(pml4_pa); }

/* ~~ pml4_add_user ~ Add U/S bit to a PD entry for user access ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
int pml4_add_user(uint64_t pml4_pa, uint64_t vaddr) {
    uint64_t *pml4 = (uint64_t *)(uintptr_t)pml4_pa;
    int pml4_idx = (vaddr >> 39) & 0x1FF;
    int pdpt_idx = (vaddr >> 30) & 0x1FF;
    int pd_idx   = (vaddr >> 21) & 0x1FF;
    if (!(pml4[pml4_idx] & 1)) return -1;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & 1)) return -1;
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & 1)) {
        uint64_t phys = vaddr & ~0x1FFFFFULL;
        pd[pd_idx] = phys | 0x87;
    } else {
        pd[pd_idx] |= 0x04;
    }
    return 0;
}

/* Split a 2MB PD entry (huge page) into 512 × 4KB PT entries.
 * Returns 0 on success, -1 on failure.
 * After split, each 4KB entry preserves the same physical addr + flags
 * as the original 2MB entry, but as 4KB pages. */
/* ~ essa demorou pra debugar, respeita ~ */
int split_2mb_pde(uint64_t pml4_pa, uint64_t vaddr) {
    int pml4_idx = (vaddr >> 39) & 0x1FF;
    int pdpt_idx = (vaddr >> 30) & 0x1FF;
    int pd_idx   = (vaddr >> 21) & 0x1FF;

    uint64_t *pml4 = (uint64_t *)(uintptr_t)pml4_pa;
    if (!(pml4[pml4_idx] & 1)) return -1;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & 1)) return -1;
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);

    uint64_t pde = pd[pd_idx];
    if (!(pde & 1)) return -1;
    if (!(pde & 0x80)) return 0;  // Already 4KB pages, nothing to do

    /* Allocate a 4KB page table */
    uint64_t *pt = mmap_user(0, 4096, 3, 0);
    if (!pt) return -1;

    uint64_t phys_base = pde & ~0x1FFFFFULL;
    uint64_t flags = pde & 0x1FF;
    flags &= ~0x80;  // Clear huge-page bit
    for (int i = 0; i < 512; i++)
        pt[i] = (phys_base + (uint64_t)i * 0x1000) | flags;

    pd[pd_idx] = (uint64_t)(uintptr_t)pt | 0x07;
    __sync_synchronize();
    {
        uint64_t _chk;
        __asm__ volatile("movq %1, %0" : "=r"(_chk) : "m"(pd[pd_idx]));
        (void)_chk;
    }
    return 0;
}

    /* Fine-grained: set U/S on a single 4KB page within a PD entry.
     * Splits the 2MB huge page first if necessary. */
/* ~ essa demorou pra debugar, respeita ~ */
int pml4_add_user_4kb(uint64_t pml4_pa, uint64_t vaddr) {
    if (split_2mb_pde(pml4_pa, vaddr & ~0x1FFFFFULL) < 0) return -1;
    int pml4_idx = (vaddr >> 39) & 0x1FF;
    int pdpt_idx = (vaddr >> 30) & 0x1FF;
    int pd_idx   = (vaddr >> 21) & 0x1FF;
    int pt_idx   = (vaddr >> 12) & 0x1FF;

    uint64_t *pml4 = (uint64_t *)(uintptr_t)pml4_pa;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & ~0xFFFULL);
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);
    uint64_t *pt = (uint64_t *)(uintptr_t)(pd[pd_idx] & ~0xFFFULL);
    pt[pt_idx] |= 0x04;
    __sync_synchronize();
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    return 0;
}

/* ~~ pml4_destroy ~~ */
/* ~ cuidado que essa aqui morde ~ */
void pml4_destroy(uint64_t pml4_pa) {
    page_free((void *)(uintptr_t)pml4_pa);
}

/* ~~ pml4_map_phys ~~ */
int pml4_map_phys(uint64_t pml4_pa, uint64_t virt_addr, uint64_t phys_addr,
                  size_t size, int writable) {
    uint64_t entry_flags = 0x87;  /* 0x83 | 0x04 (User) */
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
            pml4[pml4_idx] = (uint64_t)(uintptr_t)pdpt | 0x07;
        }
        uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & ~0xFFF);
        if (!(pdpt[pdpt_idx] & 1)) {
            uint64_t *pd = mmap_user(0, 4096, 3, 0);
            if (!pd) return -1;
            for (int i = 0; i < 512; i++) pd[i] = 0;
            pdpt[pdpt_idx] = (uint64_t)(uintptr_t)pd | 0x07;
        }
        uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFF);
        pd[pd_idx] = pa | entry_flags;
    }
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_pa) : "memory");
    return 0;
}

/* ~~ User high-address allocator ~~
 * Allocates physical pages and maps them at a high virtual address
 * (starting at 0x70000000) with full user-accessible flags.
 * This avoids QEMU TCG issues with identity-map 2MB→4KB splits. */
#define USER_VA_BASE    0x70000000ULL
#define USER_VA_END     0x7F000000ULL
static uint64_t user_va_next = USER_VA_BASE;

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static int ensure_pml4_entry(uint64_t *pml4, int idx) {
    if (pml4[idx] & 1) return 0;
    uint64_t *page = mmap_user(0, 4096, 3, 0);
    if (!page) return -1;
    for (int i = 0; i < 512; i++) page[i] = 0;
    pml4[idx] = (uint64_t)(uintptr_t)page | 0x07;
    return 0;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static int map_one_page(uint64_t va, uint64_t pa, uint64_t cr3) {
    uint64_t *pml4 = (uint64_t *)(uintptr_t)cr3;
    int pml4_idx = (va >> 39) & 0x1FF;
    int pdpt_idx = (va >> 30) & 0x1FF;
    int pd_idx   = (va >> 21) & 0x1FF;
    int pt_idx   = (va >> 12) & 0x1FF;
    if (ensure_pml4_entry(pml4, pml4_idx) < 0) return -1;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & 1)) {
        uint64_t *pd = mmap_user(0, 4096, 3, 0);
        if (!pd) return -1;
        for (int i = 0; i < 512; i++) pd[i] = 0;
        pdpt[pdpt_idx] = (uint64_t)(uintptr_t)pd | 0x07;
    }
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & 1)) {
        uint64_t *pt = mmap_user(0, 4096, 3, 0);
        if (!pt) return -1;
        for (int i = 0; i < 512; i++) pt[i] = 0;
        pd[pd_idx] = (uint64_t)(uintptr_t)pt | 0x07;
    } else if (pd[pd_idx] & 0x80) {
        uint64_t pde = pd[pd_idx];
        uint64_t *pt = mmap_user(0, 4096, 3, 0);
        if (!pt) return -1;
        uint64_t phys_base = pde & ~0x1FFFFFULL;
        uint64_t flags = pde & 0x1FF;
        flags &= ~0x80;
        for (int i = 0; i < 512; i++)
            pt[i] = (phys_base + (uint64_t)i * 0x1000) | flags;
        pd[pd_idx] = (uint64_t)(uintptr_t)pt | 0x07;
    }
    uint64_t *pt = (uint64_t *)(uintptr_t)(pd[pd_idx] & ~0xFFFULL);
    pt[pt_idx] = pa | 0x67;
    __sync_synchronize();
    __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory");
    return 0;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void *mmap_user_high(size_t length) {
    serial_puts("mh: len="); serial_puthex((uint32_t)length); serial_puts("\r\n");
    size_t pages = (length + FRAME_SIZE - 1) / FRAME_SIZE;
    if (pages < 64) pages = 64;
    serial_puts("mh: pages="); serial_puthex((uint32_t)pages); serial_puts("\r\n");
    int order = 0;
    while ((1UL << order) < pages) order++;
    serial_puts("mh: order="); serial_puthex((uint32_t)order); serial_puts("\r\n");
    uint64_t n_total = (1UL << order);
    serial_puts("mh: n_total="); serial_puthex((uint32_t)n_total); serial_puts("\r\n");
    serial_puts("mh: alloc...\r\n");
    void *phys = buddy_alloc(order, 1);
    serial_puts("mh: alloc done\r\n");
    if (!phys) { serial_puts("mh: alloc FAIL\r\n"); return NULL; }
    uint64_t pa = (uint64_t)(uintptr_t)phys;
#define N_GUARD 4
    uint64_t *guard_page = buddy_alloc(0, 1);
    if (!guard_page) { buddy_free(phys, order); serial_puts("mh: guard FAIL\r\n"); return NULL; }
    uint64_t guard_pa = (uint64_t)(uintptr_t)guard_page;
    void *guard2 = buddy_alloc(0, 1);
    if (!guard2) { buddy_free(phys, order); buddy_free(guard_page, 0); serial_puts("mh: guard2 FAIL\r\n"); return NULL; }
    uint64_t guard2_pa = (uint64_t)(uintptr_t)guard2;
    void *guard3 = buddy_alloc(0, 1);
    if (!guard3) { buddy_free(phys, order); buddy_free(guard_page, 0); buddy_free(guard2, 0); serial_puts("mh: guard3 FAIL\r\n"); return NULL; }
    uint64_t guard3_pa = (uint64_t)(uintptr_t)guard3;
    void *guard4 = buddy_alloc(0, 1);
    if (!guard4) { buddy_free(phys, order); buddy_free(guard_page, 0); buddy_free(guard2, 0); buddy_free(guard3, 0); serial_puts("mh: guard4 FAIL\r\n"); return NULL; }
    uint64_t guard4_pa = (uint64_t)(uintptr_t)guard4;
    uint64_t va = user_va_next;
    uint64_t total_size = (n_total + N_GUARD) * FRAME_SIZE;
    if (va + total_size > USER_VA_END) {
        buddy_free(phys, order);
        buddy_free(guard_page, 0);
        buddy_free(guard2, 0);
        buddy_free(guard3, 0);
        buddy_free(guard4, 0);
        return NULL;
    }
    uint64_t cr3 = pml4_get_current();
    for (uint64_t i = 0; i < n_total; i++) {
        if (map_one_page(va + i * FRAME_SIZE, pa + i * FRAME_SIZE, cr3) < 0) {
            buddy_free(phys, order);
            buddy_free(guard_page, 0);
            buddy_free(guard2, 0);
            buddy_free(guard3, 0);
            buddy_free(guard4, 0);
            return NULL;
        }
    }
    if (map_one_page(va + n_total * FRAME_SIZE, guard_pa, cr3) < 0) goto guard_fail;
    if (map_one_page(va + (n_total + 1) * FRAME_SIZE, guard2_pa, cr3) < 0) goto guard_fail;
    if (map_one_page(va + (n_total + 2) * FRAME_SIZE, guard3_pa, cr3) < 0) goto guard_fail;
    if (map_one_page(va + (n_total + 3) * FRAME_SIZE, guard4_pa, cr3) < 0) goto guard_fail;
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
    serial_puts("mh: done guard va="); serial_puthex((uint32_t)(va + n_total * FRAME_SIZE)); serial_puts("\r\n");
    user_va_next = va + (n_total + N_GUARD) * FRAME_SIZE;
    return (void *)va;
guard_fail:
    buddy_free(phys, order);
    buddy_free(guard_page, 0);
    buddy_free(guard2, 0);
    buddy_free(guard3, 0);
    buddy_free(guard4, 0);
    return NULL;
}

/* ♥ memory.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
