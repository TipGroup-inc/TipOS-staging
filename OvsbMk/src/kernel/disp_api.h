#ifndef DISP_API_H
#define DISP_API_H

#include <stdint.h>

#define SYS_disp_get_fb  200
#define SYS_disp_flush   201

uint64_t sys_disp_get_fb(uint64_t *addr, uint32_t *width,
                          uint32_t *height, uint32_t *pitch);
void sys_disp_flush(void *backbuffer);

#endif
