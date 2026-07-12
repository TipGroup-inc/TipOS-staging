#ifndef RING3_H
#define RING3_H

#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} tss_t;

void tss_init(void);
void enter_ring3(void *entry, void *user_rsp);

#endif
