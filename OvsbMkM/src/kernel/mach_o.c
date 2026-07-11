#include "mach_o.h"
#include <stdint.h>

extern void vga_puts(const char *s);
extern void debug_puts(const char *s);

// Ponto de entrada do programa carregado
typedef void (*entry_point_t)(void);

void *mach_o_load(void *data, unsigned int len) {
    mach_header_64_t *header = (mach_header_64_t *)data;
    volatile unsigned short *vga = (unsigned short *)0xB8000;

    debug_puts("[MachO] Verificando magic...\n");
    if (header->magic == FAT_MAGIC || header->magic == FAT_CIGAM) {
        debug_puts("[MachO] FAT binary detectado\n");
        return 0;
    }

    if (header->magic != MH_MAGIC_64 && header->magic != MH_CIGAM_64) {
        debug_puts("[MachO] Magic inválido\n");
        return 0;
    }

    debug_puts("[MachO] Mach-O 64-bit válido\n");
    vga[160] = (0x0A << 8) | 'M';
    vga[161] = (0x0A << 8) | 'O';

    uint8_t *cmds = (uint8_t *)(header + 1);
    entry_point_t entry = 0;
    uint64_t file_base = 0;
    uint64_t slide = 0x2000000;

    for (uint32_t i = 0; i < header->ncmds; i++) {
        load_command_t *cmd = (load_command_t *)cmds;

        if (cmd->cmd == LC_SEGMENT_64) {
            segment_command_64_t *seg = (segment_command_64_t *)cmds;
            if (seg->filesize > 0) {
                if (file_base == 0 && seg->fileoff == 0) {
                    file_base = seg->vmaddr;
                    slide = 0x2000000;
                }
                uint8_t *src = (uint8_t *)data + seg->fileoff;
                uint8_t *dst = (uint8_t *)(seg->vmaddr - file_base + slide);
                for (uint64_t j = 0; j < seg->filesize; j++) {
                    dst[j] = src[j];
                }
                for (uint64_t j = seg->filesize; j < seg->vmsize; j++) {
                    dst[j] = 0;
                }
                if (seg->segname[0] == '_' && seg->segname[1] == '_' && seg->segname[2] == 'T' && seg->segname[3] == 'E') {
                    debug_puts("[MachO] Segmento __TEXT carregado\n");
                } else if (seg->segname[0] == '_' && seg->segname[1] == '_' && seg->segname[2] == 'D') {
                    debug_puts("[MachO] Segmento __DATA carregado\n");
                } else {
                    debug_puts("[MachO] Segmento carregado\n");
                }
            }
        } else if (cmd->cmd == LC_MAIN) {
            entry_point_command_t *main_cmd = (entry_point_command_t *)cmds;
            entry = (entry_point_t)(slide + main_cmd->entryoff);
                    debug_puts("[MachO] Entry point encontrado\n");
        } else if (cmd->cmd == LC_UNIXTHREAD) {
            uint32_t *state = (uint32_t *)(cmds + 16);
            uint64_t original_entry = ((uint64_t *)(state + 8))[0];
            if (file_base) {
                entry = (entry_point_t)(original_entry - file_base + slide);
            } else {
                entry = (entry_point_t)original_entry;
            }
            debug_puts("[MachO] Entry point encontrado\n");
        }

        cmds += cmd->cmdsize;
    }

    vga[162] = (0x0A << 8) | 'L';
    debug_puts("[MachO] Pronto para executar\n");
    return (void *)entry;
}
