#ifndef DYLD_H
#define DYLD_H

#include <stdint.h>
#include "mach_o.h"

#define LC_SYMTAB       0x02
#define LC_DYSYMTAB     0x0B
#define LC_LOAD_DYLIB   0x0C
#define LC_ID_DYLIB     0x0D
#define LC_DYLD_INFO    0x22
#define LC_DYLD_ONLY    0x80000022

#define S_NON_LAZY_SYMBOL_POINTERS 0x6
#define S_LAZY_SYMBOL_POINTERS     0x7
#define SECTION_TYPE               0x000000ff

#define INDIRECT_SYMBOL_LOCAL 0x80000000
#define INDIRECT_SYMBOL_ABS   0xFFFFFFFF

typedef struct {
    uint32_t n_strx;
    uint8_t  n_type;
    uint8_t  n_sect;
    uint16_t n_desc;
    uint64_t n_value;
} nlist_64_t;

typedef struct {
    void *base;
    uint64_t vm_base;
    uint64_t size;
    char name[256];
    nlist_64_t *symtab;
    char *strtab;
    uint32_t nsyms;
    uint32_t indirectsymoff;
    uint32_t nindirectsyms;
} loaded_dylib_t;

void dyld_init(void);
loaded_dylib_t *dyld_load_dylib(void *data, uint64_t size);
void *dyld_resolve_symbol(const char *name);
void *dyld_load_executable(void *data, uint64_t size);
int dyld_bind_all(void *data, uint64_t size);

#endif
