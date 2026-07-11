#ifndef NVRAM_H
#define NVRAM_H

#include <stdint.h>

void nvram_init(void);
const char *nvram_get(const char *key);
int nvram_set(const char *key, const char *value);

#endif
