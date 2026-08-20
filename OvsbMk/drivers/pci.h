/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: pci.h ~ funcoes anotadas: 8
 */
#ifndef PCI_H
#define PCI_H
#include <stdint.h>

#define PCI_VENDOR       0x00
#define PCI_DEVICE       0x02
#define PCI_COMMAND      0x04
#define PCI_BAR0      0x10
#define PCI_BAR1      0x14
#define PCI_COMMAND      0x04
#define PCI_CLASS_REV    0x08
#define PCI_HDR_TYPE     0x0E
#define PCI_SUBSYS_VEND  0x2C
#define PCI_SUBSYS_ID    0x2E

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static inline void pci_outl(uint32_t val, uint16_t port) {
    __asm__ volatile ("outl %0, %w1" : : "a"(val), "d"(port));
}
/* ~ cuidado que essa aqui morde ~ */
static inline uint32_t pci_inl(uint16_t port) {
    uint32_t r;
    __asm__ volatile ("inl %w1, %0" : "=a"(r) : "d"(port));
    return r;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static inline uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
    uint32_t addr = 0x80000000U | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)func << 8) | (reg & 0xFC);
    pci_outl(addr, 0xCF8);
    return pci_inl(0xCFC);
}

/* ~ cuidado que essa aqui morde ~ */
static inline void pci_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t val) {
    uint32_t addr = 0x80000000U | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)func << 8) | (reg & 0xFC);
    pci_outl(addr, 0xCF8);
    pci_outl(val, 0xCFC);
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static inline uint8_t pci_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
    uint32_t v = pci_read32(bus, dev, func, reg & 0xFC);
    return (uint8_t)(v >> ((reg & 3) * 8));
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static inline uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) {
    uint32_t v = pci_read32(bus, dev, func, reg & 0xFC);
    return (uint16_t)(v >> ((reg & 2) * 8));
}

/* ~ essa demorou pra debugar, respeita ~ */
static inline uint16_t pci_bar0_io(uint8_t bus, uint8_t dev, uint8_t func) {
    return (uint16_t)(pci_read32(bus, dev, func, PCI_BAR0) & 0xFFFC);
}

/* ~ cuidado que essa aqui morde ~ */
static inline uint8_t pci_find_cap(uint8_t bus, uint8_t dev, uint8_t func, uint8_t cap_id) {
    uint8_t cap_ptr = (uint8_t)(pci_read16(bus, dev, func, 0x34) & 0xFF);
    while (cap_ptr) {
        uint8_t id = pci_read8(bus, dev, func, cap_ptr);
        if (id == cap_id) return cap_ptr;
        cap_ptr = pci_read8(bus, dev, func, (uint8_t)(cap_ptr + 1));
    }
    return 0;
}

int pci_find(uint16_t vendor, uint16_t device, uint8_t *bus, uint8_t *dev, uint8_t *func);

#endif

/* ♥ pci.h ~ arquivo fofinho do OvsbMk! kyun~ <3 */
