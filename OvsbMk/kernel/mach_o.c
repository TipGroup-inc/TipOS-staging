/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: mach_o.c ~ funcoes anotadas: 4
 */
/* ♥ mach_o.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ MACH-O ~ loader de binarios 64-bit! feedface magic~ */
/* ♥ MACH-O LOADER ~ "Carrega binario Mach-O 64-bit na memoria!"
 * Dica: Le o header, itera pelos load commands, copia segmentos~
 * Aplica slide em 0x2000000 pra nao conflitar com o kernel~
 * Suporta LC_MAIN e LC_UNIXTHREAD como entry point~
 * Se o magic for invalido, retorna NULL~ baka! */

#include "mach_o.h"
#include <stdint.h>
#include "console.h"
#include "memory.h"
#include "serial.h"

typedef void (*entry_point_t)(void);

/* ~ essa demorou pra debugar, respeita ~ */
void *mach_o_load(void *data, unsigned int len) {
    mach_header_64_t *header = (mach_header_64_t *)data;

    if (header->magic == FAT_MAGIC || header->magic == FAT_CIGAM) {
        console_write("mach_o: FAT binary nao suportado\n");
        return 0;
    }

    if (header->magic != MH_MAGIC_64 && header->magic != MH_CIGAM_64) {
        console_write("mach_o: magic invalido\n");
        return 0;
    }

    uint8_t *cmds = (uint8_t *)(header + 1);
    entry_point_t entry = 0;
    uint64_t file_base = 0;
    uint64_t slide = 0;
    uint64_t max_va = 0;

    for (uint32_t i = 0; i < header->ncmds; i++) {
        load_command_t *cmd = (load_command_t *)cmds;

        if (cmd->cmd == LC_SEGMENT_64) {
            segment_command_64_t *seg = (segment_command_64_t *)cmds;
            if (seg->filesize > 0) {
                if (file_base == 0) {
                    file_base = seg->vmaddr;
                    slide = seg->vmaddr;
                }
                uint64_t dst_va = seg->vmaddr - file_base + slide;
                uint8_t *dst = (uint8_t *)dst_va;
                uint8_t *src = (uint8_t *)data + seg->fileoff;
                for (uint64_t j = 0; j < seg->filesize; j++)
                    dst[j] = src[j];
                for (uint64_t j = seg->filesize; j < seg->vmsize; j++)
                    dst[j] = 0;
                uint64_t seg_end = dst_va + seg->vmsize;
                if (seg_end > max_va)
                    max_va = seg_end;
                /* Zero extra memory for data references beyond vmsize */
                uint64_t extra_end = (seg_end + 0x1BFFF) & ~0xFFFULL;
                for (uint64_t addr = seg_end; addr < extra_end; addr++)
                    *(volatile uint8_t *)addr = 0;
                if (extra_end > max_va)
                    max_va = extra_end;
                console_printf("mach_o: loaded %s at %x, %d bytes\n",
                    seg->segname, (unsigned)dst_va,
                    (unsigned)seg->filesize);

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

    /* Load at original vmaddr (no slide). The identity map covers
       all physical RAM, so the binary's absolute addresses work
       without relocation. Entry code zeros its own BSS range. */
    console_printf("mach_o: entry at %x\n", (unsigned)(uintptr_t)entry);
    return (void *)entry;
}

/* Helper: map a 2MB virtual page in pml4 to a freshly allocated physical page.
 * Modifies pml4 page tables directly without switching CR3. */
/* Deep-clone the identity-mapped page tables (PML4→PDP→PD for slot 0).
 * Returns new PML4 physical address, or 0 on failure.
 * The caller can then modify PD entries without affecting the original. */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
uint64_t clone_identity_tables(void) {
    uint64_t *old_pml4 = (uint64_t *)(uintptr_t)pml4_get_current();

    /* Allocate new PD table */
    uint64_t *new_pd = mmap_user(0, 4096, 3, 0);
    if (!new_pd) return 0;

    /* Allocate new PDP table */
    uint64_t *new_pdp = mmap_user(0, 4096, 3, 0);
    if (!new_pdp) return 0;

    /* Allocate new PML4 */
    uint64_t *new_pml4 = mmap_user(0, 4096, 3, 0);
    if (!new_pml4) return 0;

    /* Copy identity PD entries from old PD table (PDP[0] → PD).
     * Strip U/S bit (0x04) from ALL entries so kernel memory is
     * NOT accessible from user mode. The spawn handler will add U/S
     * back for the specific pages the process needs (code, stack,
     * framebuffer, etc.).
     *
     * IMPORTANT: only copy 2MB huge pages (bit 0x80 = PS).
     * PDEs that were split into 4KB page tables by vm_map_wire
     * (e.g. for a backbuffer mmap) are NOT copied — the child
     * must not share page tables with the parent. */
    uint64_t *old_pdp = (uint64_t *)(uintptr_t)(old_pml4[0] & ~0xFFFULL);
    uint64_t *old_pd = (uint64_t *)(uintptr_t)(old_pdp[0] & ~0xFFFULL);
    for (int i = 0; i < 512; i++) {
        if (old_pd[i] & 0x80)
            new_pd[i] = old_pd[i] & ~0x04ULL;
        else
            new_pd[i] = 0;
    }

    /* Set up new PDP: only entry 0 (identity) points to the new PD.
     * 0x07 = Present + R/W + User (U/S bit essential for ring 3!) */
    new_pdp[0] = (uint64_t)(uintptr_t)new_pd | 0x07;

    /* Copy remaining old PDP entries (all zero, which is fine) */
    for (int i = 1; i < 512; i++)
        new_pdp[i] = old_pdp[i];

    /* Set up new PML4: entry 0 points to new PDP.
     * 0x07 = Present + R/W + User (U/S bit essential for ring 3!) */
    new_pml4[0] = (uint64_t)(uintptr_t)new_pdp | 0x07;
    for (int i = 1; i < 512; i++)
        new_pml4[i] = old_pml4[i];

    return (uint64_t)(uintptr_t)new_pml4;
}

/* Map a 2MB virtual page in the given pml4 by modifying the PD entry.
 * The pml4 must already have its own private PD table (use clone_identity_tables). */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int map_2mb_in_pml4(uint64_t pml4_pa, uint64_t virt, uint64_t phys) {
    int pml4_idx = (virt >> 39) & 0x1FF;
    int pdpt_idx = (virt >> 30) & 0x1FF;
    int pd_idx   = (virt >> 21) & 0x1FF;

    uint64_t *pml4 = (uint64_t *)(uintptr_t)pml4_pa;
    if (!(pml4[pml4_idx] & 1)) return -1;
    uint64_t *pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & 1)) return -1;
    uint64_t *pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_idx] & ~0xFFFULL);
    pd[pd_idx] = (phys & ~0x1FFFFFULL) | 0x87;
    return 0;
}

/* Load a Mach-O binary into a specific pml4's address space.
 * Allocates new physical pages for the code, so the caller's
 * address space is NOT overwritten (even if both map 0x10000000).
 * Returns the entry point. */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void *mach_o_load_into_pml4(void *data, unsigned int len, uint64_t pml4) {
    mach_header_64_t *header = (mach_header_64_t *)data;
    if (header->magic != MH_MAGIC_64 && header->magic != MH_CIGAM_64) {
        serial_puts("[mach_o] invalid magic in load_into_pml4\n");
        return 0;
    }

    uint8_t *cmds = (uint8_t *)(header + 1);
    entry_point_t entry = 0;
    uint64_t file_base = 0;
    uint64_t slide = 0;

    if (!pml4) {
        serial_puts("[mach_o] no pml4\n");
        return 0;
    }

    for (uint32_t i = 0; i < header->ncmds; i++) {
        load_command_t *cmd = (load_command_t *)cmds;
        if (cmd->cmd == LC_SEGMENT_64) {
            segment_command_64_t *seg = (segment_command_64_t *)cmds;
            if (seg->filesize > 0) {
                if (file_base == 0) {
                    file_base = seg->vmaddr;
                    slide = seg->vmaddr;
                }
                uint64_t dst_va = seg->vmaddr - file_base + slide;
                uint64_t seg_end = dst_va + seg->vmsize;
                uint64_t aligned_start = dst_va & ~(0x1FFFFFULL);
                uint64_t aligned_end = (seg_end + 0x1FFFFF) & ~(0x1FFFFFULL);

                for (uint64_t va = aligned_start; va < aligned_end; va += 0x200000) {
                    void *phys = mmap_user(0, 0x200000, 3, 0);
                    if (!phys) {
                        serial_puts("[mach_o] alloc failed\n");
                        return 0;
                    }
                    if (map_2mb_in_pml4(pml4, va, (uint64_t)phys) < 0) {
                        serial_puts("[mach_o] map failed\n");
                        return 0;
                    }
                    /* Zero out and copy segment data into the NEW page */
                    for (uint64_t off = 0; off < 0x200000; off++)
                        ((uint8_t *)phys)[off] = 0;
                    if (va == (dst_va & ~(0x1FFFFFULL))) {
                        uint8_t *src = (uint8_t *)data + seg->fileoff;
                        uint8_t *base = (uint8_t *)phys;
                        for (uint64_t j = 0; j < seg->filesize; j++)
                            base[(dst_va & 0x1FFFFF) + j] = src[j];
                    }
                }

                serial_puts("[mach_o] loaded into pml4 at ");
                serial_puthex((uint32_t)dst_va);
                serial_puts(" size ");
                serial_puthex((uint32_t)seg->filesize);
                serial_puts("\n");
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




/* ♥ mach_o.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
