/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: virtio_gpu.h ~ funcoes anotadas: 10
 */
#ifndef VIRTIO_GPU_H
#define VIRTIO_GPU_H
#include <stdint.h>
#include "../lib/gui/vesa.h"

/* VirtIO PCI legacy registers (I/O ports, relative to BAR0) */
#define VIRTIO_PCI_HOST_FEATURES  0
#define VIRTIO_PCI_GUEST_FEATURES 4
#define VIRTIO_PCI_QUEUE_PFN      8
#define VIRTIO_PCI_QUEUE_NUM      12
#define VIRTIO_PCI_QUEUE_SEL      14
#define VIRTIO_PCI_QUEUE_NOTIFY   16
#define VIRTIO_PCI_STATUS         18
#define VIRTIO_PCI_ISR            19

/* Device status bits */
#define VIRTIO_STATUS_ACK         1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_FAILED      128

/* VirtIO GPU device ID */
#define VIRTIO_GPU_PCI_VENDOR  0x1AF4
#define VIRTIO_GPU_PCI_DEVICE  0x1050

/* VirtIO GPU queue indices */
#define VIRTIO_GPU_Q_CONTROL   0
#define VIRTIO_GPU_Q_CURSOR    1

/* Queue size */
#define VIRTIO_GPU_Q_SIZE 256

/* VirtIO descriptor flags */
#define VIRTQ_DESC_F_NEXT  1
#define VIRTQ_DESC_F_WRITE 2

/* --- VirtIO GPU command types --- */
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO      0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D     0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF         0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT            0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH         0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D    0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106

#define VIRTIO_GPU_RESP_OK_NODATA             0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO       0x1101
#define VIRTIO_GPU_RESP_ERR_UNSPEC            0x1200

/* GPU formats */
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM 256

/* --- VirtIO GPU structures --- */
/* ~ cuidado que essa aqui morde ~ */
typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t context_id;
    uint32_t padding;
} virtio_gpu_ctrl_hdr_t;

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
typedef struct __attribute__((packed)) {
    uint32_t x, y, w, h;
} virtio_gpu_rect_t;

/* ~ essa demorou pra debugar, respeita ~ */
typedef struct __attribute__((packed)) {
    virtio_gpu_rect_t r;
    uint32_t enabled;
    uint32_t flags;
} virtio_gpu_display_one_t;

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_display_one_t pmodes[16];
} virtio_gpu_resp_display_info_t;

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
} virtio_gpu_resource_create_2d_t;

/* ~ essa demorou pra debugar, respeita ~ */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint32_t scanout_id;
    uint32_t resource_id;
} virtio_gpu_set_scanout_t;

/* ~ cuidado que essa aqui morde ~ */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} virtio_gpu_mem_entry_t;

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} virtio_gpu_attach_backing_t;

/* ~ essa demorou pra debugar, respeita ~ */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_transfer_to_host_2d_t;

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_resource_flush_t;

/* --- Backing pages --- */
#define VIRTIO_GPU_BACKING_SIZE 0x400000  /* 4 MB backing */

extern int g_virtio_active;
int virtio_gpu_init(framebuffer_t *fb, uint64_t *fb_va_out);
int virtio_gpu_flush(void);

#endif

/* ♥ virtio_gpu.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
