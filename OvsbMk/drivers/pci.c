/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: pci.c ~ funcoes anotadas: 1
 */
#include "pci.h"

/* ~ essa demorou pra debugar, respeita ~ */
int pci_find(uint16_t vendor, uint16_t device, uint8_t *bus_out, uint8_t *dev_out, uint8_t *func_out) {
    for (int bus = 0; bus < 256; bus++) {
        for (int dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read32(bus, dev, 0, 0);
            uint16_t v = id & 0xFFFF;
            uint16_t d = (id >> 16) & 0xFFFF;
            if (v == 0xFFFF) continue;
            if (v == vendor && d == device) {
                *bus_out = bus; *dev_out = dev; *func_out = 0;
                return 0;
            }
        }
    }
    return -1;
}

/* ♥ pci.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
