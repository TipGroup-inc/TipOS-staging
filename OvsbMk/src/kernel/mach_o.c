#include "mach_o.h"
#include "vga.h"
#include <stdint.h>

typedef void (*entry_point_t)(void);

void *mach_o_load(void *data, unsigned int len) {
    mach_header_64_t *header = (mach_header_64_t *)data;

    if (header->magic == FAT_MAGIC || header->magic == FAT_CIGAM)
        return 0;

    if (header->magic != MH_MAGIC_64 && header->magic != MH_CIGAM_64)
        return 0;

    uint8_t *cmds = (uint8_t *)(header + 1);
    entry_point_t entry = 0;
    uint64_t file_base = 0;
    uint64_t slide = 0x2000000;

    for (uint32_t i = 0; i < header->ncmds; i++) {
        load_command_t *cmd = (load_command_t *)cmds;

        if (cmd->cmd == LC_SEGMENT_64) {
            segment_command_64_t *seg = (segment_command_64_t *)cmds;
            if (seg->filesize > 0) {
                if (file_base == 0) {
                    file_base = seg->vmaddr;
                    slide = 0x2000000;
                }
                uint8_t *src = (uint8_t *)data + seg->fileoff;
                uint8_t *dst = (uint8_t *)(seg->vmaddr - file_base + slide);
                for (uint64_t j = 0; j < seg->filesize; j++)
                    dst[j] = src[j];
                for (uint64_t j = seg->filesize; j < seg->vmsize; j++)
                    dst[j] = 0;
            }
        } else if (cmd->cmd == LC_MAIN) {
            entry_point_command_t *main_cmd = (entry_point_command_t *)cmds;
            entry = (entry_point_t)(slide + main_cmd->entryoff);
        } else if (cmd->cmd == LC_UNIXTHREAD) {
            uint32_t *state = (uint32_t *)(cmds + 16);
            uint64_t original_entry = ((uint64_t *)(state + 8))[0];
            if (file_base)
                entry = (entry_point_t)(original_entry - file_base + slide);
            else
                entry = (entry_point_t)original_entry;
        }

        cmds += cmd->cmdsize;
    }

    return (void *)entry;
}
