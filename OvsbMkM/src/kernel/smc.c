#include "smc.h"
#include "kernel.h"
#include "memory.h"
#include <string.h>

void smc_init(void) {
    // Inicialização mínima do SMC virtual
}

const char *smc_get(const char *key) {
    // Respostas fixas para chaves conhecidas
    if (!key) return "";
    if (strcmp(key, "SSN") == 0) return "SN123456789";
    if (strcmp(key, "BDID") == 0) return "BOARD-XYZ";
    if (strcmp(key, "MLB") == 0) return "MLB0001";
    if (strcmp(key, "UUID") == 0) return "00000000-0000-0000-0000-000000000000";
    return "";
}
