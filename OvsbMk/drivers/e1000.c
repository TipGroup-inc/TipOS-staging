#include "e1000.h"
#include "pci.h"
#include "../kernel/serial.h"
#include "../kernel/memory.h"

#define RX_DESC_COUNT 32
#define TX_DESC_COUNT 32
#define RX_BUF_SIZE   2048

typedef struct {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t  status;
    volatile uint8_t  errors;
    volatile uint16_t special;
} rx_desc_t;

typedef struct {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t  cso;
    volatile uint8_t  cmd;
    volatile uint8_t  status;
    volatile uint8_t  css;
    volatile uint16_t special;
} tx_desc_t;

static uint8_t *mmio_base = 0;
static int tx_cur = 0;

static rx_desc_t rx_descs[RX_DESC_COUNT] __attribute__((aligned(16)));
static tx_desc_t tx_descs[TX_DESC_COUNT] __attribute__((aligned(16)));
static uint8_t rx_buffers[RX_DESC_COUNT][RX_BUF_SIZE] __attribute__((aligned(16)));

static inline void mmio_write32(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(mmio_base + reg) = val;
}

static inline uint32_t mmio_read32(uint32_t reg) {
    return *(volatile uint32_t *)(mmio_base + reg);
}

int e1000_init(void) {
    uint8_t bus, dev, func;
    if (pci_find(E1000_VENDOR, E1000_DEVICE, &bus, &dev, &func) != 0) {
        serial_puts("[e1000] Placa nao encontrada\r\n");
        return -1;
    }
    serial_puts("[e1000] Placa encontrada!\r\n");

    // Pegar BAR0 (MMIO)
    uint32_t bar0 = pci_read32(bus, dev, func, PCI_BAR0);
    uint32_t mmio_phys = bar0 & 0xFFFFFFF0;
    serial_puts("[e1000] BAR0=");
    serial_puthex(mmio_phys);
    serial_puts("\r\n");

    // Mapear MMIO usando pml4_map_phys (2MB page)
    uint64_t mmio_aligned = mmio_phys & ~0x1FFFFFULL;
    pml4_map_phys(pml4_get_current(), mmio_aligned, mmio_aligned, 0x200000, 1);
    mmio_base = (uint8_t *)(uintptr_t)mmio_phys;

    // Reset
    mmio_write32(REG_CTRL, mmio_read32(REG_CTRL) | CTRL_RST);
    for (volatile int i = 0; i < 100000; i++);

    // Link up
    mmio_write32(REG_CTRL, mmio_read32(REG_CTRL) | CTRL_SLU);
    for (volatile int i = 0; i < 100000; i++);

    if (!(mmio_read32(REG_STATUS) & STATUS_LU)) {
        serial_puts("[e1000] Link nao esta up!\r\n");
        return -1;
    }
    serial_puts("[e1000] Link UP!\r\n");

    // Configurar RX
    for (int i = 0; i < RX_DESC_COUNT; i++) {
        rx_descs[i].addr = (uint64_t)(uintptr_t)rx_buffers[i];
        rx_descs[i].status = 0;
    }
    mmio_write32(REG_RDBAL, (uint32_t)(uintptr_t)rx_descs);
    mmio_write32(REG_RDBAH, 0);
    mmio_write32(REG_RDLEN, RX_DESC_COUNT * 16);
    mmio_write32(REG_RDH, 0);
    mmio_write32(REG_RDT, RX_DESC_COUNT - 1);
    mmio_write32(REG_RCTRL, RCTL_EN | RCTL_BAM);

    // Configurar TX
    for (int i = 0; i < TX_DESC_COUNT; i++) {
        tx_descs[i].addr = 0;
        tx_descs[i].status = 1; // Done
    }
    mmio_write32(REG_TDBAL, (uint32_t)(uintptr_t)tx_descs);
    mmio_write32(REG_TDBAH, 0);
    mmio_write32(REG_TDLEN, TX_DESC_COUNT * 16);
    mmio_write32(REG_TDH, 0);
    mmio_write32(REG_TDT, 0);
    mmio_write32(REG_TCTRL, TCTL_EN | TCTL_PSP);
    mmio_write32(REG_TIPG, 0x0060200A);

    serial_puts("[e1000] Driver inicializado!\r\n");
    return 0;
}

int e1000_send(const uint8_t *data, int len) {
    if (!mmio_base || len <= 0 || len > RX_BUF_SIZE) return -1;
    
    tx_descs[tx_cur].addr = (uint64_t)(uintptr_t)data;
    tx_descs[tx_cur].length = (uint16_t)len;
    tx_descs[tx_cur].cmd = 0x0B; // EOP | IFCS | RS
    tx_descs[tx_cur].status = 0;

    mmio_write32(REG_TDT, (tx_cur + 1) % TX_DESC_COUNT);
    tx_cur = (tx_cur + 1) % TX_DESC_COUNT;

    // Esperar transmissão
    for (volatile int i = 0; i < 1000000; i++) {
        if (tx_descs[tx_cur].status & 1) break;
    }

    serial_puts("[e1000] Pacote enviado!\r\n");
    return 0;
}

int e1000_receive(uint8_t *buf, int max_len) {
    (void)buf; (void)max_len;
    return -1; // TODO
}
