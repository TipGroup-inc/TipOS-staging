#include "dyld.h"
#include "mach_o.h"
#include "memory.h"
#include <stdint.h>

extern int strcmp(const char *a, const char *b);
extern void vga_puts(const char *s);
extern void debug_puts(const char *s);
extern unsigned char libsystem_bin[];
extern unsigned int libsystem_bin_len;

static const char *dyld_strstr(const char *haystack, const char *needle) {
    if (!*needle) return haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return haystack;
    }
    return NULL;
}

#define DYLD_LOAD_BASE ((uint8_t *)0x2000000)
#define MAX_DYLIBS 16

static loaded_dylib_t dylibs[MAX_DYLIBS];
static int num_dylibs = 0;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;
} __attribute__((packed)) symtab_command_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t ilocalsym;
    uint32_t nlocalsym;
    uint32_t iextdefsym;
    uint32_t nextdefsym;
    uint32_t iundefsym;
    uint32_t nundefsym;
    uint32_t tocoff;
    uint32_t ntoc;
    uint32_t modtaboff;
    uint32_t nmodtab;
    uint32_t extrefsymoff;
    uint32_t nextrefsyms;
    uint32_t indirectsymoff;
    uint32_t nindirectsyms;
    uint32_t extreloff;
    uint32_t nextrel;
    uint32_t locreloff;
    uint32_t nlocrel;
} __attribute__((packed)) dysymtab_command_t;

static void *dyld_stub(void) {
    return 0;
}

static uint64_t dyld_compute_file_base(void *data, uint64_t size) {
    mach_header_64_t *header = (mach_header_64_t *)data;
    uint8_t *cmds = (uint8_t *)(header + 1);
    uint64_t file_base = 0;

    for (uint32_t i = 0; i < header->ncmds; i++) {
        load_command_t *cmd = (load_command_t *)cmds;
        if (cmd->cmd == LC_SEGMENT_64) {
            segment_command_64_t *seg = (segment_command_64_t *)cmds;
            if (seg->fileoff == 0) {
                file_base = seg->vmaddr;
                break;
            }
        }
        cmds += cmd->cmdsize;
    }
    return file_base;
}

static int dyld_parse_symtab_commands(void *data, uint64_t size,
                                      uint32_t *out_symoff,
                                      uint32_t *out_nsyms,
                                      uint32_t *out_stroff,
                                      uint32_t *out_strsize,
                                      uint32_t *out_indirectsymoff,
                                      uint32_t *out_nindirectsyms) {
    mach_header_64_t *header = (mach_header_64_t *)data;
    uint8_t *cmds = (uint8_t *)(header + 1);
    if (header->magic != MH_MAGIC_64 && header->magic != MH_CIGAM_64) return 0;

    *out_symoff = 0;
    *out_nsyms = 0;
    *out_stroff = 0;
    *out_strsize = 0;
    *out_indirectsymoff = 0;
    *out_nindirectsyms = 0;

    for (uint32_t i = 0; i < header->ncmds; i++) {
        load_command_t *cmd = (load_command_t *)cmds;
            if (cmd->cmd == LC_SYMTAB) {
            debug_puts("[dyld] Encontrado LC_SYMTAB\n");
        } else if (cmd->cmd == LC_DYSYMTAB) {
            debug_puts("[dyld] Encontrado LC_DYSYMTAB\n");
        }
        if (cmd->cmd == LC_SYMTAB) {
            symtab_command_t *sym = (symtab_command_t *)cmds;
            *out_symoff = sym->symoff;
            *out_nsyms = sym->nsyms;
            *out_stroff = sym->stroff;
            *out_strsize = sym->strsize;
        } else if (cmd->cmd == LC_DYSYMTAB) {
            dysymtab_command_t *dysym = (dysymtab_command_t *)cmds;
            *out_indirectsymoff = dysym->indirectsymoff;
            *out_nindirectsyms = dysym->nindirectsyms;
        }
        cmds += cmd->cmdsize;
    }

    if (!(*out_symoff && *out_nsyms && *out_stroff && *out_strsize)) {
        debug_puts("[dyld] FALHA: comandos de symtab incompletos\n");
    }

    return (*out_symoff && *out_nsyms && *out_stroff && *out_strsize);
}

static loaded_dylib_t *dyld_find_dylib(const char *name) {
    for (int i = 0; i < num_dylibs; i++) {
        if (strcmp(dylibs[i].name, name) == 0) return &dylibs[i];
    }
    return NULL;
}

static const char *dyld_extract_dylib_name(void *data) {
    mach_header_64_t *header = (mach_header_64_t *)data;
    uint8_t *cmds = (uint8_t *)(header + 1);
    for (uint32_t i = 0; i < header->ncmds; i++) {
        load_command_t *cmd = (load_command_t *)cmds;
        if (cmd->cmd == LC_ID_DYLIB || cmd->cmd == LC_LOAD_DYLIB) {
            uint32_t name_off = *(uint32_t *)(cmds + 8);
            return (const char *)cmds + name_off;
        }
        cmds += cmd->cmdsize;
    }
    return NULL;
}

void dyld_init(void) {
    num_dylibs = 0;
    dyld_load_dylib((void *)libsystem_bin, libsystem_bin_len);
}

loaded_dylib_t *dyld_load_dylib(void *data, uint64_t size) {
    if (!data || num_dylibs >= MAX_DYLIBS) return NULL;

    const char *name = dyld_extract_dylib_name(data);
    if (name) {
        loaded_dylib_t *existing = dyld_find_dylib(name);
        if (existing) return existing;
    }

    uint32_t symoff, nsyms, stroff, strsize, indirectsymoff, nindirectsyms;
    int has_symtab = dyld_parse_symtab_commands(data, size, &symoff, &nsyms, &stroff, &strsize, &indirectsymoff, &nindirectsyms);
    if (!has_symtab) {
        debug_puts("[dyld] Aviso: symtab ausente ou incompleto para dylib, carregando mesmo assim\n");
        symoff = 0; nsyms = 0; stroff = 0; strsize = 0; indirectsymoff = 0; nindirectsyms = 0;
    }

    void *entry = mach_o_load(data, (unsigned int)size);
    if (!entry) {
        mach_header_64_t *hdr = (mach_header_64_t *)data;
        /* MH_DYLIB == 6: dylibs may not have an entry point; mapping can still succeed */
#define MH_DYLIB 0x06
        if (hdr->filetype == MH_DYLIB) {
            debug_puts("[dyld] dylib mapeado sem entrypoint, continuando\n");
        } else {
            debug_puts("[dyld] mach_o_load falhou ao carregar dylib\n");
            return NULL;
        }
        (void)entry; /* keep variable for clarity */
        /* cleanup: undef local define */
#undef MH_DYLIB
    }

    loaded_dylib_t *lib = &dylibs[num_dylibs];
    lib->base = DYLD_LOAD_BASE;
    lib->vm_base = dyld_compute_file_base(data, size);
    lib->size = size;
    lib->symtab = symoff ? (nlist_64_t *)((uint8_t *)data + symoff) : NULL;
    lib->strtab = stroff ? (char *)data + stroff : NULL;
    lib->nsyms = nsyms;
    lib->indirectsymoff = indirectsymoff;
    lib->nindirectsyms = nindirectsyms;
    lib->name[0] = '\0';
    if (name) {
        for (int i = 0; i < 255 && name[i]; i++) lib->name[i] = name[i];
        lib->name[255] = '\0';
    }
    if (lib->name[0] == '\0') {
        for (int i = 0; i < 255 && name && name[i]; i++) lib->name[i] = name[i];
        lib->name[255] = '\0';
    }

    if (lib->name[0] == '\0') {
        const char *fallback = "libunknown";
        for (int i = 0; i < 255 && fallback[i]; i++) lib->name[i] = fallback[i];
        lib->name[255] = '\0';
    }

    num_dylibs++;
    return lib;
}

void *dyld_resolve_symbol(const char *name) {
    if (!name) return NULL;

    for (int i = 0; i < num_dylibs; i++) {
        loaded_dylib_t *lib = &dylibs[i];
        for (uint32_t j = 0; j < lib->nsyms; j++) {
            nlist_64_t *sym = &lib->symtab[j];
            if (sym->n_strx == 0) continue;
            const char *symname = lib->strtab + sym->n_strx;
            if (symname[0] == '\0') continue;
            if (strcmp(name, symname) == 0) {
                if ((sym->n_type & 0x0e) != 0x0) {
                    return (void *)((uint8_t *)lib->base + (sym->n_value - lib->vm_base));
                }
            }
            if (name[0] != '_' && symname[0] == '_' && strcmp(name, symname + 1) == 0) {
                return (void *)((uint8_t *)lib->base + (sym->n_value - lib->vm_base));
            }
        }
    }
    return NULL;
}

static int dyld_load_dylib_by_path(const char *path) {
    if (!path) {
        debug_puts("[dyld] dyld_load_dylib_by_path: path NULL\n");
        return 0;
    }
    debug_puts("[dyld] dyld_load_dylib_by_path: tentando ");
    debug_puts(path);
    debug_puts("\n");
    if (dyld_find_dylib(path)) {
        debug_puts("[dyld] já carregado: ");
        debug_puts(path);
        debug_puts("\n");
        return 1;
    }
    if (dyld_strstr(path, "libSystem.B.dylib") != NULL) {
        debug_puts("[dyld] mapeando libSystem embutida\n");
        int ok = dyld_load_dylib((void *)libsystem_bin, libsystem_bin_len) != NULL;
        if (!ok) debug_puts("[dyld] falha ao carregar libSystem_embutida\n");
        return ok;
    }
    /* Unknown dylib: for now, return success so deps can resolve to stubs */
    debug_puts("[dyld] dependencia desconhecida, usando stub: ");
    debug_puts(path);
    debug_puts("\n");
    return 1;
}

static int dyld_parse_load_dylibs(void *data, uint64_t size) {
    mach_header_64_t *header = (mach_header_64_t *)data;
    uint8_t *cmds = (uint8_t *)(header + 1);

    for (uint32_t i = 0; i < header->ncmds; i++) {
        load_command_t *cmd = (load_command_t *)cmds;
        if (cmd->cmd == LC_LOAD_DYLIB) {
            uint32_t name_off = *(uint32_t *)(cmds + 8);
            const char *path = (const char *)cmds + name_off;
            debug_puts("[dyld] Dependencia LC_LOAD_DYLIB: ");
            debug_puts(path);
            debug_puts("\n");
            if (!dyld_load_dylib_by_path(path)) return 0;
        }
        cmds += cmd->cmdsize;
    }
    return 1;
}

static uint32_t *dyld_read_indirect_symbols(void *data, uint64_t size, uint32_t indirectsymoff, uint32_t nindirectsyms) {
    return (uint32_t *)((uint8_t *)data + indirectsymoff);
}

static void dyld_write_pointer(uint64_t runtime_addr, void *value) {
    uint64_t *slot = (uint64_t *)(uintptr_t)runtime_addr;
    *slot = (uint64_t)value;
}

int dyld_bind_all(void *data, uint64_t size) {
    mach_header_64_t *header = (mach_header_64_t *)data;
    uint8_t *cmds = (uint8_t *)(header + 1);

    uint32_t symoff = 0, nsyms = 0, stroff = 0, strsize = 0;
    uint32_t indirectsymoff = 0, nindirectsyms = 0;
    if (!dyld_parse_symtab_commands(data, size, &symoff, &nsyms, &stroff, &strsize, &indirectsymoff, &nindirectsyms)) {
        return 0;
    }

    uint64_t file_base = dyld_compute_file_base(data, size);
    uint64_t slide = (uint64_t)DYLD_LOAD_BASE - file_base;
    nlist_64_t *symtab = (nlist_64_t *)((uint8_t *)data + symoff);
    char *strtab = (char *)data + stroff;
    uint32_t *indirect_sym = dyld_read_indirect_symbols(data, size, indirectsymoff, nindirectsyms);

    for (uint32_t i = 0; i < header->ncmds; i++) {
        load_command_t *cmd = (load_command_t *)cmds;
        if (cmd->cmd == LC_SEGMENT_64) {
            segment_command_64_t *seg = (segment_command_64_t *)cmds;
            section_64_t *sect = (section_64_t *)(cmds + sizeof(segment_command_64_t));
            for (uint32_t j = 0; j < seg->nsects; j++) {
                uint32_t type = sect->flags & SECTION_TYPE;
                if (type == S_NON_LAZY_SYMBOL_POINTERS || type == S_LAZY_SYMBOL_POINTERS) {
                    uint64_t section_runtime = slide + sect->addr;
                    uint32_t sym_index = sect->reserved1;
                    uint64_t entries = sect->size / 8;
                    for (uint64_t k = 0; k < entries; k++) {
                        uint32_t indirect_index = indirect_sym[sym_index + k];
                        if (indirect_index == INDIRECT_SYMBOL_ABS) continue;
                        if (indirect_index & INDIRECT_SYMBOL_LOCAL) continue;
                        if (indirect_index >= nsyms) continue;
                        nlist_64_t *sym = &symtab[indirect_index];
                        if (sym->n_strx == 0) continue;
                        const char *symname = strtab + sym->n_strx;
                        void *resolved = dyld_resolve_symbol(symname);
                        if (!resolved) resolved = (void *)dyld_stub;
                        dyld_write_pointer(section_runtime + k * 8, resolved);
                    }
                }
                sect++;
            }
        }
        cmds += cmd->cmdsize;
    }

    return 1;
}

void *dyld_load_executable(void *data, uint64_t size) {
    debug_puts("[dyld] Iniciando carregamento do executavel...\n");
    if (!data) {
        debug_puts("[dyld] FALHA: ponteiro de dados NULL\n");
        return NULL;
    }

    debug_puts("[dyld] Verificando dependencias...\n");
    if (!dyld_parse_load_dylibs(data, size)) {
        debug_puts("[dyld] FALHA: dyld_parse_load_dylibs retornou 0\n");
        return NULL;
    }

    debug_puts("[dyld] Carregando segmentos do executavel...\n");
    void *entry = mach_o_load(data, (unsigned int)size);
    if (!entry) {
        debug_puts("[dyld] FALHA: mach_o_load retornou NULL\n");
        return NULL;
    }
    debug_puts("[dyld] Segmentos carregados com sucesso\n");

    debug_puts("[dyld] Realizando bind de simbolos...\n");
    if (!dyld_bind_all(data, size)) {
        debug_puts("[dyld] FALHA: dyld_bind_all retornou 0\n");
        return NULL;
    }
    debug_puts("[dyld] Bind concluido\n");

    debug_puts("[dyld] Carregamento concluido!\n");
    return entry;
}
