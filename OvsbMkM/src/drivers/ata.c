#include "ata.h"
#define ATA_DATA       0x1F0
#define ATA_SECTORS    0x1F2
#define ATA_LBA_LOW    0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HIGH   0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_COMMAND    0x1F7
#define ATA_STATUS     0x1F7

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

void ata_init(void) {
    for (volatile int i = 0; i < 100000; i++);
}

int ata_read_sector(uint32_t lba, uint8_t *buffer) {
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTORS, 1);
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, 0x20);
    while (1) {
        uint8_t status = inb(ATA_STATUS);
        if (status & 0x08) break;
        if (status & 0x01) return -1;
    }
    for (int i = 0; i < 256; i++) {
        uint16_t data;
        __asm__ volatile ("inw %1, %0" : "=a"(data) : "Nd"(ATA_DATA));
        buffer[i * 2] = data & 0xFF;
        buffer[i * 2 + 1] = (data >> 8) & 0xFF;
    }
    return 0;
}

int ata_write_sector(uint32_t lba, const uint8_t *buffer) {
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTORS, 1);
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, 0x30);
    while (1) {
        uint8_t status = inb(ATA_STATUS);
        if (status & 0x08) break;
        if (status & 0x01) return -1;
    }
    for (int i = 0; i < 256; i++) {
        uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
        __asm__ volatile ("outw %0, %1" :: "a"(data), "Nd"(ATA_DATA));
    }
    for (volatile int i = 0; i < 100000; i++);
    return 0;
}
