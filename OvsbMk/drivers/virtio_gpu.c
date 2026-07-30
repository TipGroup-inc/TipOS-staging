/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: virtio_gpu.c ~ funcoes anotadas: 12
 */
#include <stdint.h>
#include <stddef.h>
#include "pci.h"
#include "virtio_gpu.h"
#include "../kernel/serial.h"
#include "../kernel/memory.h"
#include "../lib/gui/vesa.h"

int g_virtio_active = 0;

/* Mapped MMIO regions */
static volatile uint8_t *g_common = NULL;
static volatile uint8_t *g_notify = NULL;
static uint32_t g_notify_mult = 0;

/* Virtqueue structures */
#define VQ_SIZE 256

/* ~ essa demorou pra debugar, respeita ~ */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vq_desc_t;

/* ~ cuidado que essa aqui morde ~ */
typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VQ_SIZE];
} vq_avail_t;

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
typedef struct __attribute__((packed)) {
    uint32_t id;
    uint32_t len;
} vq_used_elem_t;

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    vq_used_elem_t ring[VQ_SIZE];
} vq_used_t;

typedef struct {
    vq_desc_t  *desc;
    vq_avail_t *avail;
    vq_used_t  *used;
    uint16_t    size;
    uint16_t    next_desc;
    uint16_t    last_used;
    uint16_t    notify_off;
    uint64_t    desc_paddr;
} virtqueue_t;

static virtqueue_t g_ctrlq;
static uint32_t g_rid = 1;
static uint32_t g_fb_w = 0, g_fb_h = 0;

/* Pre-allocated buffers for virtio_gpu_flush hot path */
static virtio_gpu_transfer_to_host_2d_t g_flush_xfer;
static virtio_gpu_resource_flush_t      g_flush_res;
static virtio_gpu_ctrl_hdr_t           g_flush_resp;

/* Common cfg helpers */
static inline uint8_t  vc_r8(uint32_t o)  { return g_common[o]; }
static inline void     vc_w8(uint32_t o, uint8_t v)  { g_common[o] = v; }
static inline uint16_t vc_r16(uint32_t o) { return *(volatile uint16_t*)(g_common + o); }
static inline void     vc_w16(uint32_t o, uint16_t v){ *(volatile uint16_t*)(g_common + o) = v; }
static inline uint32_t vc_r32(uint32_t o) { return *(volatile uint32_t*)(g_common + o); }
static inline void     vc_w32(uint32_t o, uint32_t v){ *(volatile uint32_t*)(g_common + o) = v; }
static inline void     vc_w64(uint32_t o, uint64_t v){ *(volatile uint64_t*)(g_common + o) = v; }

/* ~ essa demorou pra debugar, respeita ~ */
static int map_bar(uint64_t paddr, uint32_t common_off, uint32_t notify_off) {
    uint64_t base2m = paddr & ~(uint64_t)0x1FFFFF;
    uint64_t va_base = 0xFFFFFF0000000000ULL;
    uint64_t pml4 = pml4_get_current();
    if (pml4_map_phys(pml4, va_base, base2m, 0x200000, 1) != 0)
        return -1;

    serial_puts("[VG] map done va=");
    serial_puthex(va_base);
    serial_puts(" pa=");
    serial_puthex(base2m);
    serial_puts("\r\n");

    uint64_t base_off = paddr & 0x1FFFFF;
    g_common = (volatile uint8_t*)(va_base + base_off + common_off);
    g_notify = (volatile uint8_t*)(va_base + base_off + notify_off);

    serial_puts("[VG] common va=");
    serial_puthex((uint32_t)(uintptr_t)g_common);
    serial_puts(" notify va=");
    serial_puthex((uint32_t)(uintptr_t)g_notify);
    serial_puts("\r\n");

    return 0;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int vq_init(virtqueue_t *vq, uint32_t sel, uint16_t qsize) {
    size_t desc_sz = qsize * sizeof(vq_desc_t);
    size_t avail_sz = 2 + 2 + qsize * 2;
    size_t used_sz  = 2 + 2 + qsize * 8;
    size_t total = desc_sz + avail_sz + used_sz + 64;
    total = (total + 0xFFF) & ~0xFFF;

    void *mem = kmalloc(total);
    if (!mem) return -1;
    for (size_t i = 0; i < total; i++) ((uint8_t*)mem)[i] = 0;

    vq->desc  = (vq_desc_t*)mem;
    vq->avail = (vq_avail_t*)((uint8_t*)mem + desc_sz);
    uintptr_t used_p = ((uintptr_t)mem + desc_sz + avail_sz + 3) & ~3;
    vq->used   = (vq_used_t*)used_p;
    vq->size   = qsize;
    vq->next_desc = 0;
    vq->last_used = 0;
    vq->desc_paddr = (uint64_t)(uintptr_t)mem;
    vq->notify_off = 0;

    vc_w16(0x16, (uint16_t)sel);
    vc_w16(0x18, qsize);
    vc_w64(0x20, vq->desc_paddr);
    vc_w64(0x28, vq->desc_paddr + ((uint8_t*)vq->avail - (uint8_t*)mem));
    vc_w64(0x30, vq->desc_paddr + ((uint8_t*)vq->used  - (uint8_t*)mem));
    vc_w16(0x1C, 1);

    vq->notify_off = vc_r16(0x1E);

    serial_puts("[VG] vq ");
    serial_puthex(sel);
    serial_puts(" sz=");
    serial_puthex(qsize);
    serial_puts(" phys=");
    serial_puthex((uint32_t)vq->desc_paddr);
    serial_puts(" nto=");
    serial_puthex(vq->notify_off);
    serial_puts("\r\n");
    return 0;
}

/* ~ essa demorou pra debugar, respeita ~ */
static int vq_alloc_desc(virtqueue_t *vq) {
    if (vq->next_desc >= vq->size) return -1;
    return (int)vq->next_desc++;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void vq_post(virtqueue_t *vq, int head) {
    uint16_t i = vq->avail->idx;
    vq->avail->ring[i] = (uint16_t)head;
    __asm__ volatile("" : : : "memory");
    vq->avail->idx = i + 1;
    __asm__ volatile("" : : : "memory");
    uint32_t doorbell = g_notify_mult * vq->notify_off;
    *(volatile uint16_t*)(g_notify + doorbell) = 0;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int vq_wait(virtqueue_t *vq) {
    for (int t = 0; t < 50000000; t++) {
        __asm__ volatile("pause");
        if (vq->last_used != vq->used->idx) {
            uint16_t i = vq->last_used & (vq->size - 1);
            vq->last_used++;
            return (int)vq->used->ring[i].id;
        }
    }
    serial_puts("[VG] vq_wait TIMEOUT\r\n");
    return -1;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int vg_cmd(void *cmd, uint32_t clen, void *resp, uint32_t rlen) {
    int d = vq_alloc_desc(&g_ctrlq);
    if (d < 0) return -1;
    g_ctrlq.desc[d].addr = (uint64_t)(uintptr_t)cmd;
    g_ctrlq.desc[d].len  = clen;
    g_ctrlq.desc[d].flags = 0;
    g_ctrlq.desc[d].next = 0;

    int rd = -1;
    if (resp) {
        rd = vq_alloc_desc(&g_ctrlq);
        if (rd < 0) return -1;
        g_ctrlq.desc[rd].addr = (uint64_t)(uintptr_t)resp;
        g_ctrlq.desc[rd].len  = rlen;
        g_ctrlq.desc[rd].flags = 0x02;
        g_ctrlq.desc[rd].next = 0;
        g_ctrlq.desc[d].flags = 0x01;
        g_ctrlq.desc[d].next  = (uint16_t)rd;
    }

    vq_post(&g_ctrlq, d);
    return (vq_wait(&g_ctrlq) >= 0) ? 0 : -1;
}

/* --- Public API --- */

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int virtio_gpu_init(framebuffer_t *fb, uint64_t *fb_va_out) {
    uint8_t bus, dev, func;
    serial_puts("[VG] modern init...\r\n");

    if (pci_find(0x1AF4, 0x1050, &bus, &dev, &func) < 0) {
        serial_puts("[VG] no device\r\n");
        return -1;
    }

    uint16_t cmd = pci_read16(bus, dev, func, 4);
    cmd |= 7;
    pci_write32(bus, dev, func, 4, cmd);

    /* Parse PCI capabilities */
    uint8_t cap = pci_read8(bus, dev, func, 0x34);
    uint64_t bar_paddr = 0;
    uint32_t common_off = 0, notify_off = 0;
    int found_common = 0, found_notify = 0;

    while (cap) {
        uint8_t id = pci_read8(bus, dev, func, cap);
        if (id == 0x09) {
            uint8_t  cfg_type = pci_read8(bus, dev, func, cap + 3);
            uint8_t  bar_idx  = pci_read8(bus, dev, func, cap + 4);
            uint32_t reg_off  = pci_read32(bus, dev, func, cap + 8);
            pci_read32(bus, dev, func, cap + 12);

            serial_puts("[VG] cap t=");
            serial_puthex(cfg_type);
            serial_puts(" bar=");
            serial_puthex(bar_idx);
            serial_puts(" po=");
            serial_puthex(reg_off);
            serial_puts("\r\n");

            if (cfg_type == 1 || cfg_type == 2) {
                uint32_t bv = pci_read32(bus, dev, func, 0x10 + bar_idx * 4);
                uint64_t ba = (uint64_t)(bv & 0xFFFFFFF0);
                if (bv & 4) {
                    uint32_t hi = pci_read32(bus, dev, func, 0x10 + (bar_idx + 1) * 4);
                    ba |= (uint64_t)hi << 32;
                }
                if (!bar_paddr) bar_paddr = ba;
            }

            if (cfg_type == 1) {
                common_off = reg_off;
                found_common = 1;
            } else if (cfg_type == 2) {
                notify_off = reg_off;
                found_notify = 1;
                g_notify_mult = pci_read32(bus, dev, func, cap + 16);
                serial_puts("[VG] nmult=");
                serial_puthex(g_notify_mult);
                serial_puts("\r\n");
            }
        }
        cap = pci_read8(bus, dev, func, cap + 1);
    }

    if (!found_common || !found_notify || !bar_paddr) {
        serial_puts("[VG] missing caps\r\n");
        return -1;
    }

    serial_puts("[VG] bar_paddr=");
    serial_puthex((uint32_t)bar_paddr);
    serial_puts("\r\n");

    if (map_bar(bar_paddr, common_off, notify_off) < 0) {
        serial_puts("[VG] map FAIL\r\n");
        return -1;
    }
    serial_puts("[VG] MMIO mapped\r\n");

    /* Reset */
    vc_w8(0x14, 0);
    for (volatile int d = 0; d < 100; d++);

    /* Init sequence */
    vc_w8(0x14, 1);
    for (volatile int d = 0; d < 100; d++);
    vc_w8(0x14, 3);
    for (volatile int d = 0; d < 100; d++);

    /* Features */
    vc_w32(0, 0);
    uint32_t f0 = vc_r32(4);
    serial_puts("[VG] f0=");
    serial_puthex(f0);
    serial_puts("\r\n");

    vc_w32(8, f0);
    vc_w32(0xC, 0);

    vc_w8(0x14, 0x0B);
    for (volatile int d = 0; d < 100; d++);
    if (!(vc_r8(0x14) & 8)) {
        serial_puts("[VG] FEATURES_OK fail\r\n");
        return -1;
    }
    serial_puts("[VG] features ok\r\n");

    uint16_t num_q = vc_r16(0x12);
    serial_puts("[VG] queues=");
    serial_puthex(num_q);
    serial_puts("\r\n");
    if (num_q < 1) { serial_puts("[VG] no queues\r\n"); return -1; }

    if (vq_init(&g_ctrlq, 0, 256) < 0) {
        serial_puts("[VG] vq FAIL\r\n");
        return -1;
    }

    vc_w8(0x14, 0x0F);
    serial_puts("[VG] DRIVER_OK\r\n");

    /* GET_DISPLAY_INFO */
    virtio_gpu_ctrl_hdr_t *req  = kmalloc(sizeof(*req));
    virtio_gpu_resp_display_info_t *resp = kmalloc(sizeof(*resp));
    req->type     = 0x0100;
    req->flags    = 0;
    req->fence_id = 0;
    req->context_id = 0;
    req->padding  = 0;

    if (vg_cmd(req, sizeof(*req), resp, sizeof(*resp)) < 0) {
        serial_puts("[VG] get_disp FAIL\r\n");
        kfree(req); kfree(resp);
        return -1;
    }

    uint32_t w = resp->pmodes[0].r.w;
    uint32_t h = resp->pmodes[0].r.h;
    serial_puts("[VG] display ");
    serial_puthex(w);
    serial_puts("x");
    serial_puthex(h);
    serial_puts("\r\n");
    kfree(req); kfree(resp);

    if (w == 0 || h == 0) {
        serial_puts("[VG] no display\r\n");
        return -1;
    }

    /* Allocate backing (identity-mapped heap) */
    size_t fb_sz = (size_t)w * h * 4;
    uint32_t pages = (uint32_t)((fb_sz + 0xFFF) / 0x1000);
    void *backing = kmalloc(pages * 4096);
    if (!backing) { serial_puts("[VG] backing FAIL\r\n"); return -1; }

    g_fb_w = w;
    g_fb_h = h;

    /* RESOURCE_CREATE_2D */
    {
        virtio_gpu_resource_create_2d_t *c = kmalloc(sizeof(*c));
        virtio_gpu_ctrl_hdr_t *r = kmalloc(sizeof(*r));
        c->hdr.type = 0x0101;
        c->hdr.flags = 0;
        c->resource_id = g_rid;
        c->format = 256;
        c->width  = w;
        c->height = h;
        if (vg_cmd(c, sizeof(*c), r, sizeof(*r)) < 0) {
            serial_puts("[VG] create2d FAIL\r\n");
            kfree(c); kfree(r); return -1;
        }
        kfree(c); kfree(r);
    }

    /* ATTACH_BACKING */
    {
        size_t asz = sizeof(virtio_gpu_attach_backing_t) + sizeof(virtio_gpu_mem_entry_t);
        virtio_gpu_attach_backing_t *a = kmalloc(asz);
        virtio_gpu_mem_entry_t *e = (virtio_gpu_mem_entry_t*)((uint8_t*)a + sizeof(*a));
        a->hdr.type = 0x0106;
        a->hdr.flags = 0;
        a->resource_id = g_rid;
        a->nr_entries = 1;
        e->addr = (uint64_t)(uintptr_t)backing;
        e->length = pages * 4096;
        e->padding = 0;

        virtio_gpu_ctrl_hdr_t *r = kmalloc(sizeof(*r));
        if (vg_cmd(a, asz, r, sizeof(*r)) < 0) {
            serial_puts("[VG] attach FAIL\r\n");
            kfree(a); kfree(r); return -1;
        }
        kfree(a); kfree(r);
    }

    /* SET_SCANOUT */
    {
        virtio_gpu_set_scanout_t *s = kmalloc(sizeof(*s));
        virtio_gpu_ctrl_hdr_t *r = kmalloc(sizeof(*r));
        s->hdr.type = 0x0103;
        s->hdr.flags = 0;
        s->r.x = 0; s->r.y = 0; s->r.w = w; s->r.h = h;
        s->scanout_id = 0;
        s->resource_id = g_rid;
        if (vg_cmd(s, sizeof(*s), r, sizeof(*r)) < 0) {
            serial_puts("[VG] scanout FAIL\r\n");
            kfree(s); kfree(r); return -1;
        }
        kfree(s); kfree(r);
    }

    fb->addr  = (uint64_t)(uintptr_t)backing;
    fb->pitch = w * 4;
    fb->width = w;
    fb->height = h;
    fb->bpp   = 32;
    *fb_va_out = (uint64_t)(uintptr_t)backing;
    g_virtio_active = 1;

    serial_puts("[VG] init OK!\r\n");
    return 0;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
int virtio_gpu_flush(void) {
    if (!g_virtio_active) return -1;

    g_flush_xfer.hdr.type = 0x0105;
    g_flush_xfer.hdr.flags = 0;
    g_flush_xfer.r.x = 0; g_flush_xfer.r.y = 0;
    g_flush_xfer.r.w = g_fb_w; g_flush_xfer.r.h = g_fb_h;
    g_flush_xfer.offset = 0;
    g_flush_xfer.resource_id = g_rid;
    g_flush_xfer.padding = 0;
    if (vg_cmd(&g_flush_xfer, sizeof(g_flush_xfer),
               &g_flush_resp, sizeof(g_flush_resp)) < 0) {
        return -1;
    }

    g_flush_res.hdr.type = 0x0104;
    g_flush_res.hdr.flags = 0;
    g_flush_res.r.x = 0; g_flush_res.r.y = 0;
    g_flush_res.r.w = g_fb_w; g_flush_res.r.h = g_fb_h;
    g_flush_res.resource_id = g_rid;
    g_flush_res.padding = 0;
    if (vg_cmd(&g_flush_res, sizeof(g_flush_res),
               &g_flush_resp, sizeof(g_flush_resp)) < 0) {
        return -1;
    }

    return 0;
}

/* ♥ virtio_gpu.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
