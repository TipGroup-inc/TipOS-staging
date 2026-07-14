#ifndef PARSE_MB2_H
#define PARSE_MB2_H
#include <stdint.h>
#include "../lib/gui/vesa.h"

void parse_multiboot2(uint32_t magic, uint32_t addr, framebuffer_t *fb);
#endif
