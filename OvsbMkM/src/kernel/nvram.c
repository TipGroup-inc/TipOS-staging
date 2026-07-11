#include "nvram.h"
#include "kernel.h"
#include <string.h>

static char storage[16][64];

void nvram_init(void) {
    // clear
    for (int i=0;i<16;i++) storage[i][0]=0;
}

const char *nvram_get(const char *key) {
    if (!key) return "";
    // busca simples: key is index-like
    if (strcmp(key, "boot-args") == 0) return "";
    return "";
}

int nvram_set(const char *key, const char *value) {
    (void)key; (void)value; return 0;
}
