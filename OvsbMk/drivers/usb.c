/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: usb.c ~ funcoes anotadas: 21
 */
/* ♥ USB UHCI Driver ~ pro QEMU usb-tablet funcionar com T410! ♥
 * Só implementa o mínimo necessário: UHCI, control transfer,
 * interrupt transfer pro HID do tablet.
 *
 * Isso NÃO é um stack USB genérico. É uma gambiarra estrutural.
 * Mas funciona ~~ testado no QEMU (e provavelmente só lá). <3
 *
 * Referência: Intel UHCI Spec + USB 1.1 Spec + QEMU source.
 * Porque ler documentação é pra quem tem tempo. >_> */

#include <stdint.h>
#include <stddef.h>
#include "mouse.h"
#include "../kernel/memory.h"

/* ─── I/O port helpers (precisa vir ANTES de pci_read) ─────── */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static inline uint8_t inb_(uint16_t p) {
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(p));
    return v;
}
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static inline uint16_t inw_(uint16_t p) {
    uint16_t v;
    __asm__ volatile ("inw %w1, %0" : "=a"(v) : "d"(p));
    return v;
}
/* ~ cuidado que essa aqui morde ~ */
static inline uint32_t inl_(uint16_t p) {
    uint32_t v;
    __asm__ volatile ("inl %1, %0" : "=a"(v) : "Nd"(p));
    return v;
}
/* ~ cuidado que essa aqui morde ~ */
static inline void outb_(uint16_t p, uint8_t v) {
    __asm__ volatile ("outb %0, %1" :: "a"(v), "Nd"(p));
}
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static inline void outw_(uint16_t p, uint16_t v) {
    __asm__ volatile ("outw %0, %w1" :: "a"(v), "d"(p));
}
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static inline void outl_(uint16_t p, uint32_t v) {
    __asm__ volatile ("outl %0, %1" :: "a"(v), "Nd"(p));
}

/* ─── PCI config access ───────────────────────────────────── */
#define PCI_ADDR  0xCF8
#define PCI_DATA  0xCFC

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static inline uint32_t pci_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    uint32_t addr = 0x80000000 | (bus << 16) | ((slot & 31) << 11) | ((func & 7) << 8) | (off & 0xFC);
    outl_(PCI_ADDR, addr);
    return inl_(PCI_DATA);
}

/* ~ cuidado que essa aqui morde ~ */
static inline void pci_write_cmd(uint8_t bus, uint8_t slot, uint32_t cmdval) {
    uint32_t addr = 0x80000000 | (bus << 16) | ((slot & 31) << 11) | (0 << 8) | 0x04;
    outl_(PCI_ADDR, addr);
    outl_(PCI_DATA, cmdval);
}

/* ─── UHCI registers (I/O offset from base) ───────────────── */
#define UHCI_USBCMD      0
#define UHCI_USBSTS      2
#define UHCI_USBINTR     4
#define UHCI_FRNUM       6
#define UHCI_FLBASEADD   8
#define UHCI_SOF         12
#define UHCI_PORTSC1     16
#define UHCI_PORTSC2     18

/* UHCI command bits */
#define USBCMD_RS        (1 << 0)
#define USBCMD_HCRESET   (1 << 1)
#define USBCMD_CF        (1 << 6)

/* Status bits */
#define USBSTS_HCHALTED  (1 << 5)

/* Port status bits */
#define PORTSC_CCS       (1 << 0)
#define PORTSC_PE        (1 << 2)
#define PORTSC_RESET     (1 << 9)

/* ─── UHCI transfer descriptors and queue heads ───────────── */
typedef struct {
    volatile uint32_t link;
    volatile uint32_t status;
    volatile uint32_t token;
    volatile uint32_t buffer;
} __attribute__((packed)) uhci_td_t;

typedef struct {
    volatile uint32_t link;
    volatile uint32_t element;
} __attribute__((packed)) uhci_qh_t;

#define TD_LINK_TERM     (1 << 0)
#define TD_LINK_QH       (1 << 1)
#define TD_ACTIVE        (1u << 23)
#define TD_STALLED       (1u << 22)
#define TD_BABBLE        (1u << 20)
#define TD_TIMEOUT       (1u << 18)
#define TD_IOC           (1u << 24)

#define TD_PID_SHIFT     0
#define TD_DEVADDR_SHIFT 8
#define TD_ENDP_SHIFT    15
#define TD_TOGGLE_SHIFT  19
#define TD_MAXLEN_SHIFT  21

#define PID_SETUP  0x2D
#define PID_IN     0x69
#define PID_OUT    0xE1

/* ─── USB standard requests ───────────────────────────────── */
#define USB_DIR_IN      (1 << 7)
#define USB_REQ_GET_DESCRIPTOR   6
#define USB_REQ_SET_ADDRESS      5
#define USB_REQ_SET_CONFIG       9

#define USB_DT_DEVICE     1
#define USB_DT_CONFIG     2

/* ─── Forward declarations ────────────────────────────────── */
static void *uhci_alloc(size_t size);
static uint32_t virt_to_phys(void *v);

/* ─── Global state ────────────────────────────────────────── */
static uint16_t uhci_base = 0;
static int usb_ok = 0;

static uint32_t *frame_list = NULL;
static uhci_qh_t *periodic_qh = NULL;   /* pra schedule periodico (SOF) */
static uhci_qh_t *intr_qh = NULL;       /* QH do endpoint interrupt do HID */
static uhci_qh_t *ctrl_qh = NULL;       /* QH pra control transfers */
static uhci_td_t *intr_td[2] = {NULL, NULL};
static int intr_toggle = 0;
static uint8_t report_buf[8];

static uint8_t tablet_addr = 0;
static uint8_t tablet_endp = 0;
static int tablet_maxp = 0;

/* ─── Allocate zeroed memory for UHCI DMA ─────────────────── */
/* ~ cuidado que essa aqui morde ~ */
static void *uhci_alloc(size_t size) {
    void *p = kmalloc(size);
    if (!p) return NULL;
    for (size_t i = 0; i < size; i++) ((uint8_t *)p)[i] = 0;
    return p;
}

/* ─── Virtual to physical (heap is identity-mapped) ───────── */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static inline uint32_t virt_to_phys(void *v) {
    return (uint32_t)(uintptr_t)v;
}

/* ─── Helper: write 16-bit LE value into a uint8_t array ──── */
/* ~ essa demorou pra debugar, respeita ~ */
static inline void w16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/* ─── Helper: make a setup packet ─────────────────────────── */
static void mk_setup(uint8_t *buf, uint8_t bmReqType, uint8_t bRequest,
                      uint16_t wValue, uint16_t wIndex, uint16_t wLength) {
    buf[0] = bmReqType;
    buf[1] = bRequest;
    w16(buf + 2, wValue);
    w16(buf + 4, wIndex);
    w16(buf + 6, wLength);
}

/* ─── Prepare a TD ────────────────────────────────────────── */
static void td_setup(uhci_td_t *td, uint32_t link, uint32_t token,
                     uint32_t buf_phys, uint32_t status) {
    td->link   = link;
    td->status = status;
    td->token  = token;
    td->buffer = buf_phys;
}

/* ─── Wait for TD to complete (poll) ──────────────────────── */
/* ~ essa demorou pra debugar, respeita ~ */
static int td_wait(uhci_td_t *td) {
    for (int i = 0; i < 50000; i++) {
        if (!(td->status & TD_ACTIVE)) {
            if (td->status & (TD_STALLED | TD_BABBLE | TD_TIMEOUT))
                return -1;
            return 0;
        }
        for (volatile int w = 0; w < 10; w++);
    }
    return -2;
}

/* ─── PCI scan for UHCI (Intel PIIX4: 8086:7020) ─────────── */
/* ~ cuidado que essa aqui morde ~ */
static int uhci_find(void) {
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            uint32_t id = pci_read(bus, slot, 0, 0);
            if ((id & 0xFFFF) == 0x8086 && ((id >> 16) & 0xFFFF) == 0x7020) {
                uint32_t bar4 = pci_read(bus, slot, 0, 0x20);
                uint16_t iobase = bar4 & 0xFFFC;
                if (iobase == 0) continue;
                uint32_t cmd = pci_read(bus, slot, 0, 0x04);
                pci_write_cmd(bus, slot, cmd | 0x05);
                return iobase;
            }
        }
    }
    return 0;
}

/* ─── Initialize UHCI controller ──────────────────────────── */
/* ~ essa demorou pra debugar, respeita ~ */
static int uhci_init(uint16_t base) {
    outw_(base + UHCI_USBCMD, USBCMD_HCRESET);
    for (volatile int w = 0; w < 2000; w++);

    uint16_t port = inw_(base + UHCI_PORTSC1);
    if (!(port & PORTSC_CCS))
        port = inw_(base + UHCI_PORTSC2);
    if (!(port & PORTSC_CCS)) return -1;

    outw_(base + UHCI_PORTSC1, PORTSC_RESET);
    for (volatile int w = 0; w < 2000; w++);
    outw_(base + UHCI_PORTSC1, 0);
    for (volatile int w = 0; w < 1000; w++);

    port = inw_(base + UHCI_PORTSC1);
    if (!(port & PORTSC_CCS)) {
        outw_(base + UHCI_PORTSC2, PORTSC_RESET);
        for (volatile int w = 0; w < 2000; w++);
        outw_(base + UHCI_PORTSC2, 0);
        for (volatile int w = 0; w < 1000; w++);
    }
    return 0;
}

/* ─── Start UHCI (set up frame list, run) ─────────────────── */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int uhci_start(uint16_t base, uint32_t fl_phys) {
    outw_(base + UHCI_USBCMD, 0);
    for (volatile int w = 0; w < 500; w++);

    outl_(base + UHCI_FLBASEADD, fl_phys);
    outb_(base + UHCI_SOF, 64);
    outw_(base + UHCI_USBSTS, 0x3F);
    outw_(base + UHCI_USBCMD, USBCMD_RS | USBCMD_CF);

    for (volatile int w = 0; w < 500; w++);
    if (inw_(base + UHCI_USBSTS) & USBSTS_HCHALTED) return -1;
    return 0;
}

/* ─── USB control transfer ────────────────────────────────── */
static int usb_control_transfer(uint16_t base, uint8_t dev_addr,
                                 const uint8_t *setup, int setup_len,
                                 uint8_t *data, int data_len, int data_in) {
    static uhci_td_t td[3];
    uint32_t td_phys = virt_to_phys(td);

    uint32_t td2_phys = td_phys + sizeof(uhci_td_t);
    uint32_t td3_phys = td_phys + 2 * sizeof(uhci_td_t);

    /* Setup TD → links to data TD (or status TD if no data) */
    uint32_t next = (data_len > 0 && data) ? td2_phys : td3_phys;
    uint32_t tok = (PID_SETUP << TD_PID_SHIFT) |
                   (dev_addr << TD_DEVADDR_SHIFT) |
                   (0 << TD_TOGGLE_SHIFT) |
                   ((setup_len & 0x7FF) << TD_MAXLEN_SHIFT);
    td_setup(&td[0], TD_LINK_TERM | next,
             tok, virt_to_phys((void *)setup), TD_ACTIVE | TD_IOC);

    /* Data TD (if any) → links to status TD */
    if (data_len > 0 && data) {
        uint32_t pid = data_in ? PID_IN : PID_OUT;
        uint32_t dtok = (pid << TD_PID_SHIFT) |
                        (dev_addr << TD_DEVADDR_SHIFT) |
                        (1 << TD_TOGGLE_SHIFT) |
                        ((data_len & 0x7FF) << TD_MAXLEN_SHIFT);
        td_setup(&td[1], TD_LINK_TERM | td3_phys,
                 dtok, virt_to_phys(data), TD_ACTIVE | TD_IOC);
    }

    /* Status TD (opposite direction) — end of chain */
    uint32_t spid = data_in ? PID_OUT : PID_IN;
    uint32_t stok = (spid << TD_PID_SHIFT) |
                    (dev_addr << TD_DEVADDR_SHIFT) |
                    (1 << TD_TOGGLE_SHIFT) |
                    (0x7FF << TD_MAXLEN_SHIFT);
    td_setup(&td[2], TD_LINK_TERM, stok, 0, TD_ACTIVE | TD_IOC);

    /* Queue via ctrl_qh element */
    ctrl_qh->element = td_phys;

    int r = td_wait(&td[2]);

    /* Restore */
    ctrl_qh->element = TD_LINK_TERM;
    return r;
}

/* ─── Enumerate USB device ────────────────────────────────── */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static int usb_enumerate(uint16_t base) {
    uint8_t buf[64];
    uint8_t setup[8];
    int r;

    /* GET_DESCRIPTOR (Device, 8 bytes) — address 0 */
    mk_setup(setup, USB_DIR_IN | 0x00 | 0x00, USB_REQ_GET_DESCRIPTOR,
             USB_DT_DEVICE << 8, 0, 8);
    r = usb_control_transfer(base, 0, setup, 8, buf, 8, 1);
    if (r < 0) return -1;

    /* SET_ADDRESS (address 1) */
    tablet_addr = 1;
    mk_setup(setup, 0x00 | 0x00 | 0x00, USB_REQ_SET_ADDRESS,
             tablet_addr, 0, 0);
    r = usb_control_transfer(base, 0, setup, 8, NULL, 0, 0);
    if (r < 0) return -1;
    for (volatile int w = 0; w < 500; w++);

    /* GET_DESCRIPTOR (Device, full) */
    mk_setup(setup, USB_DIR_IN | 0x00 | 0x00, USB_REQ_GET_DESCRIPTOR,
             USB_DT_DEVICE << 8, 0, sizeof(buf));
    r = usb_control_transfer(base, tablet_addr, setup, 8, buf, sizeof(buf), 1);
    if (r < 0) return -1;

    /* GET_DESCRIPTOR (Configuration) */
    mk_setup(setup, USB_DIR_IN | 0x00 | 0x00, USB_REQ_GET_DESCRIPTOR,
             USB_DT_CONFIG << 8, 0, sizeof(buf));
    r = usb_control_transfer(base, tablet_addr, setup, 8, buf, sizeof(buf), 1);
    if (r < 0) return -1;

    /* Parse config descriptor looking for endpoint */
    int pos = buf[0] < sizeof(buf) ? buf[0] : sizeof(buf);
    int i = buf[4];
    while (i < pos) {
        uint8_t dlen = buf[i];
        uint8_t dtype = buf[i+1];
        if (dlen < 2) break;
        if (dtype == 4) {
            tablet_endp = buf[i+2] & 0x0F;
            tablet_maxp = buf[i+4] | (buf[i+5] << 8);
            break;
        }
        i += dlen;
    }
    if (tablet_maxp == 0) return -1;

    /* SET_CONFIGURATION (value 1) */
    mk_setup(setup, 0x00 | 0x00 | 0x00, USB_REQ_SET_CONFIG, 1, 0, 0);
    r = usb_control_transfer(base, tablet_addr, setup, 8, NULL, 0, 0);
    if (r < 0) return -1;

    return 0;
}

/* ─── Set up interrupt endpoint ───────────────────────────── */
/* ~ cuidado que essa aqui morde ~ */
static int usb_setup_intr(void) {
    for (int i = 0; i < 2; i++) {
        intr_td[i] = (uhci_td_t *)uhci_alloc(sizeof(uhci_td_t));
        if (!intr_td[i]) return -1;
    }
    return 0;
}

/* ─── Queue an interrupt IN transfer ──────────────────────── */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void usb_queue_intr(void) {
    if (!usb_ok) return;

    uhci_td_t *td = intr_td[intr_toggle];
    int toggle = intr_toggle;
    intr_toggle = !intr_toggle;

    uint32_t tok = (PID_IN << TD_PID_SHIFT) |
                   (tablet_addr << TD_DEVADDR_SHIFT) |
                   (tablet_endp << TD_ENDP_SHIFT) |
                   (toggle << TD_TOGGLE_SHIFT) |
                   ((tablet_maxp & 0x7FF) << TD_MAXLEN_SHIFT);

    td_setup(td, TD_LINK_TERM, tok, virt_to_phys(report_buf), TD_ACTIVE | TD_IOC);
    intr_qh->element = virt_to_phys(td);
}

/* ─── Process interrupt data from tablet ──────────────────── */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void usb_process_intr(void) {
    if (!usb_ok) return;

    uhci_td_t *td = intr_td[!intr_toggle];
    if (td->status & TD_ACTIVE) return;

    if (!(td->status & (TD_STALLED | TD_BABBLE | TD_TIMEOUT))) {
        static int prev_x = -1, prev_y = -1;

        int btn = report_buf[0] & 0x07;
        int x = report_buf[1] | (report_buf[2] << 8);
        int y = report_buf[3] | (report_buf[4] << 8);

        /* Push deltas into PS/2 mouse state */
        if (prev_x >= 0 && prev_y >= 0) {
            mouse_x += x - prev_x;
            mouse_y += y - prev_y;
            mouse_buttons = btn;
        }
        prev_x = x;
        prev_y = y;
    }

    usb_queue_intr();
}

/* ─── Public API ──────────────────────────────────────────── */

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
int usb_init(void) {
    uhci_base = uhci_find();
    if (!uhci_base) return -1;

    if (uhci_init(uhci_base) < 0) return -1;

    frame_list = (uint32_t *)uhci_alloc(4096);
    if (!frame_list) return -1;
    uint32_t fl_phys = virt_to_phys(frame_list);

    ctrl_qh = (uhci_qh_t *)uhci_alloc(sizeof(uhci_qh_t));
    if (!ctrl_qh) return -1;
    ctrl_qh->link = TD_LINK_TERM;
    ctrl_qh->element = TD_LINK_TERM;

    intr_qh = (uhci_qh_t *)uhci_alloc(sizeof(uhci_qh_t));
    if (!intr_qh) return -1;
    intr_qh->link = virt_to_phys(ctrl_qh) | TD_LINK_QH;
    intr_qh->element = TD_LINK_TERM;

    periodic_qh = (uhci_qh_t *)uhci_alloc(sizeof(uhci_qh_t));
    if (!periodic_qh) return -1;
    periodic_qh->link = virt_to_phys(intr_qh) | TD_LINK_QH;
    periodic_qh->element = TD_LINK_TERM;

    uint32_t qh_link = virt_to_phys(periodic_qh) | TD_LINK_QH;
    for (int i = 0; i < 1024; i++)
        frame_list[i] = qh_link;

    if (uhci_start(uhci_base, fl_phys) < 0) return -1;

    if (usb_enumerate(uhci_base) < 0) return -1;
    if (usb_setup_intr() < 0) return -1;

    usb_queue_intr();
    usb_ok = 1;
    return 0;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void usb_poll(void) {
    if (!usb_ok) return;
    usb_process_intr();
}

/* ♥ usb.c ~ arquivo fofinho do OvsbMk! kyun~ <3 */
