#ifndef SMC_H
#define SMC_H

#include <stdint.h>

void smc_init(void);
const char *smc_get(const char *key);

#endif
