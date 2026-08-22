#ifndef E1000_H
#define E1000_H
#include <stdint.h>

#define E1000_VENDOR  0x8086
#define E1000_DEVICE  0x100E

// Registradores MMIO
#define REG_CTRL      0x0000
#define REG_STATUS    0x0008
#define REG_EEPROM    0x0014
#define REG_ICR       0x00C0
#define REG_IMS       0x00D0
#define REG_RCTRL     0x0100
#define REG_TCTRL     0x0400
#define REG_TIPG      0x0410
#define REG_RDBAL     0x2800
#define REG_RDBAH     0x2804
#define REG_RDLEN     0x2808
#define REG_RDH       0x2810
#define REG_RDT       0x2818
#define REG_TDBAL     0x3800
#define REG_TDBAH     0x3804
#define REG_TDLEN     0x3808
#define REG_TDH       0x3810
#define REG_TDT       0x3818
#define REG_MTA       0x5200

// Flags
#define CTRL_RST      0x04000000
#define CTRL_SLU      0x00000040
#define STATUS_LU     0x00000002
#define RCTL_EN       0x00000002
#define RCTL_BAM      0x00008000
#define TCTL_EN       0x00000002
#define TCTL_PSP      0x00000008

int e1000_init(void);
int e1000_send(const uint8_t *data, int len);
int e1000_receive(uint8_t *buf, int max_len);

#endif
