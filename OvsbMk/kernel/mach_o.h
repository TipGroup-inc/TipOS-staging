/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: mach_o.h ~ funcoes anotadas: 0
 */
/* ♥ mach_o.h ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ MACH-O HEADER ~ "Loader de binarios da Apple~ mas roda no nosso kernel!"
 * Dica: Mach-O 64-bit tem magic 0xFEEDFACF~
 * Segmentos LC_SEGMENT_64 (0x19) e entry point LC_MAIN (0x80000028)~
 * Se o magic for FAT (0xCAFEBABE), volta 0~ nao suportado! >_< */

#ifndef MACH_O_H
#define MACH_O_H
#include <stdint.h>

#define MH_MAGIC_64 0xFEEDFACF
#define MH_CIGAM_64 0xCFFAEDFE
#define FAT_MAGIC   0xCAFEBABE
#define FAT_CIGAM   0xBEBAFECA

typedef struct {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
} __attribute__((packed)) mach_header_64_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
} __attribute__((packed)) load_command_t;

#define LC_SEGMENT_64 0x19
#define LC_MAIN       0x80000028
#define LC_UNIXTHREAD 0x05

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    char     segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
} __attribute__((packed)) segment_command_64_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint64_t entryoff;
    uint64_t stacksize;
} __attribute__((packed)) entry_point_command_t;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t flavor;
    uint32_t count;
} __attribute__((packed)) unixthread_command_t;

void *mach_o_load(void *data, unsigned int len);

/* Like mach_o_load, but allocates NEW physical pages and maps them
 * in the given pml4 at 0x10000000, so the current address space
 * is NOT overwritten. Returns entry point, or 0 on failure. */
void *mach_o_load_into_pml4(void *data, unsigned int len, uint64_t pml4);

/* Map a 2MB huge page at 'virt' to 'phys' in the given PML4.
 * Returns 0 on success, -1 if the required PDP/PD entries don't exist. */
int map_2mb_in_pml4(uint64_t pml4_pa, uint64_t virt, uint64_t phys);

/* Deep-clone the identity-mapped page tables (PML4→PDP→PD for slot 0).
 * Returns new PML4 physical address, or 0 on failure.
 * The caller can then modify PD entries without affecting the original. */
uint64_t clone_identity_tables(void);

#endif




/* ♥ mach_o.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
