/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ filesystem ~ onde os byte moram organizadinho (ou n) ~
 * arquivo: fat32.c ~ funcoes anotadas: 26
 */
/* ♥ fat32.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ FAT32 ~ sistema de arquivos gordinho mas funcional! */
/* ♥ FAT32 - Driver de sistema de arquivos ~ "Gordão mas eficiente!"
 * Dica: FAT32 = File Allocation Table 32 bits~
 * A Tabela FAT guarda a cadeia de clusters de cada arquivo~
 * Se perder a FAT, perdeu tudo~ entao nao mexe nela! >_<
 * Cada entrada de diretorio tem 32 bytes com nome 8.3~
 * Nomes Longos (LFN) sao ignorados aqui~ preguiça~ kyun!
 *
 * Bugs corrigidos (da versao TipOS):
 * 1. name_to_83() nao respeitava terminador nulo~
 *    Nomes curtos tipo "X" corrompiam o buffer com lixo da pilha~
 * 2. read_chain/write_chain: divisao to_read/512 truncava <512 pra 0~
 *    Arquivo HELLO com 180 bytes era lido como 0 bytes~ hihi burro~
 * 3. create_entry_at() retornava -1 generico; agora FAT_ERR_IO/NOESPACE
 * 4. fat32_mkdir() tinha bytes nulos no lugar de espacos em '.' e '..'~ */

#include "fat32.h"
#include <stdint.h>
#include "../kernel/utils.h"
#include "../kernel/console.h"
#include "../drivers/ata.h"
#include "../kernel/memory.h"

/* Cores pro list_dir */
#define C_DIR  0xFF60FF60
#define C_FILE 0xFFC0C0C0
#define C_OUT 0xFFC0C0C0

static fat32_boot_t boot;
static uint32_t first_data_sector;
static uint32_t sectors_per_cluster;
static uint32_t current_dir_cluster;
static uint32_t fat_start_sector;

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static uint8_t name_to_83(const char *name, uint8_t out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';
    if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
        int i = 0;
        while (name[i] && i < 11) { out[i] = name[i]; i++; }
        return 0;
    }
    const char *dot = 0;
    const char *p = name;
    while (*p) {
        if (*p == '.') { dot = p; break; }
        p++;
    }
    int pos = 0;
    while (name != dot && *name && pos < 8) {
        char c = *name++;
        if (c >= 'a' && c <= 'z') c -= 0x20;
        out[pos++] = c;
    }
    if (dot) {
        dot++;
        pos = 8;
        while (*dot && pos < 11) {
            char c = *dot++;
            if (c >= 'a' && c <= 'z') c -= 0x20;
            out[pos++] = c;
        }
    }
    return 0;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static uint32_t cluster_to_sector(uint32_t cluster) {
    return first_data_sector + (cluster - 2) * sectors_per_cluster;
}

/* ~ cuidado que essa aqui morde ~ */
static uint32_t fat_read_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    uint8_t sector[512];
    if (ata_read_sector(fat_sector, sector) != 0) return 0;
    return *(uint32_t *)(sector + ent_offset) & 0x0FFFFFFF;
}

/* ~ cuidado que essa aqui morde ~ */
static int fat_write_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    uint8_t sector[512];
    if (ata_read_sector(fat_sector, sector) != 0) return -1;
    *(uint32_t *)(sector + ent_offset) =
        (*(uint32_t *)(sector + ent_offset) & 0xF0000000) | (value & 0x0FFFFFFF);
    if (ata_write_sector(fat_sector, sector) != 0) return -1;
    return 0;
}

/* ~ cuidado que essa aqui morde ~ */
static uint32_t fat_find_free(void) {
    uint32_t total_clusters =
        (boot.total_sectors_32 - first_data_sector) / sectors_per_cluster;
    for (uint32_t c = 2; c < total_clusters + 2; c++) {
        if (fat_read_entry(c) == 0) return c;
    }
    return 0;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static uint32_t fat_chain_last(uint32_t start) {
    uint32_t c = start;
    while (1) {
        uint32_t next = fat_read_entry(c);
        if (next >= 0x0FFFFFF8) return c;
        if (next == 0) return c;
        c = next;
    }
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int read_chain(uint32_t start_cluster, uint8_t *buffer, uint32_t size) {
    uint32_t cluster = start_cluster;
    uint32_t offset = 0;
    uint32_t cluster_bytes = sectors_per_cluster * 512;

    while (cluster >= 2 && cluster < 0x0FFFFFF8 && offset < size) {
        uint32_t sector = cluster_to_sector(cluster);
        uint32_t to_read = size - offset;
        if (to_read > cluster_bytes) to_read = cluster_bytes;

        uint32_t num_sectors = to_read / 512;
        for (uint32_t s = 0; s < num_sectors; s++) {
            if (ata_read_sector(sector + s, buffer + offset + s * 512) != 0)
                return -1;
        }
        uint32_t remainder = to_read % 512;
        if (remainder > 0) {
            uint8_t tmp[512];
            if (ata_read_sector(sector + num_sectors, tmp) != 0) return -1;
            for (uint32_t b = 0; b < remainder; b++)
                buffer[offset + num_sectors * 512 + b] = tmp[b];
        }
        offset += to_read;
        cluster = fat_read_entry(cluster);
    }
    return offset;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int write_chain(uint32_t start_cluster, uint8_t *buffer, uint32_t size) {
    uint32_t cluster = start_cluster;
    uint32_t offset = 0;
    uint32_t cluster_bytes = sectors_per_cluster * 512;

    while (size > 0) {
        uint32_t sector = cluster_to_sector(cluster);
        uint32_t to_write = size < cluster_bytes ? size : cluster_bytes;
        uint8_t sector_buf[512];

        uint32_t num_sectors = to_write / 512;
        for (uint32_t s = 0; s < num_sectors; s++) {
            for (uint32_t b = 0; b < 512; b++)
                sector_buf[b] = buffer[offset + s * 512 + b];
            if (ata_write_sector(sector + s, sector_buf) != 0) return -1;
        }
        uint32_t remainder = to_write % 512;
        if (remainder > 0) {
            uint8_t tmp[512];
            if (ata_read_sector(sector + num_sectors, tmp) != 0) return -1;
            for (uint32_t b = 0; b < remainder; b++)
                tmp[b] = buffer[offset + num_sectors * 512 + b];
            if (ata_write_sector(sector + num_sectors, tmp) != 0) return -1;
        }
        offset += to_write;
        size -= to_write;

        if (size > 0) {
            uint32_t next = fat_read_entry(cluster);
            if (next == 0 || next >= 0x0FFFFFF8) {
                uint32_t free = fat_find_free();
                if (free == 0) return -1;
                if (fat_write_entry(cluster, free) != 0) return -1;
                if (fat_write_entry(free, 0x0FFFFFFF) != 0) return -1;
                cluster = free;
            } else {
                cluster = next;
            }
        }
    }
    return offset;
}

/* ~ cuidado que essa aqui morde ~ */
static int alloc_clusters(uint32_t start, uint32_t count) {
    uint32_t prev = start;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t free = fat_find_free();
        if (free == 0) return -1;
        if (fat_write_entry(prev, free) != 0) return -1;
        if (fat_write_entry(free, 0x0FFFFFFF) != 0) return -1;
        prev = free;
    }
    return 0;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static uint32_t first_cluster_of(fat32_dir_entry_t *e) {
    return ((uint32_t)e->first_cluster_high << 16) | e->first_cluster_low;
}

/* ~~ LFN: compara nome longo montado com o pedido (case-insensitive) ~~ */
static int lfn_match(const char *lfn, int len, const char *name) {
    int i;
    for (i = 0; i < len; i++) {
        char lc = lfn[i], nc = name[i];
        if (lc >= 'a' && lc <= 'z') lc -= 0x20;
        if (nc >= 'a' && nc <= 'z') nc -= 0x20;
        if (nc == '\0' || lc != nc) return 0;
    }
    return name[len] == '\0';
}

static int find_entry(uint32_t dir_cluster, const char *name,
                      fat32_dir_entry_t *out) {
    uint8_t name83[11];
    name_to_83(name, name83);

    /* ~~ Long File Names: entradas attr 0x0F guardam o nome em UTF-16~~
     * 13 chars por entrada, em ordem DECRESCENTE antes da entrada curta~
     * Sem isso o kernel não enxerga fonts.dir/6x13.pcf.gz/etc~ */
    char lfn[264];
    int lfn_len = 0;
    int expect_seq = 0;
    static const int lofs[13] = {1,3,5,7,9, 14,16,18,20,22,24, 28,30};

    uint32_t cluster = dir_cluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t sector = cluster_to_sector(cluster);
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            uint8_t buf[512];
            if (ata_read_sector(sector + s, buf) != 0) return -1;
            fat32_dir_entry_t *entries = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0x00) return -1;
                if (entries[i].name[0] == 0xE5) { lfn_len = 0; continue; }
                if (entries[i].attr == 0x0F) {
                    int seq = entries[i].name[0];
                    if (seq & 0x40) { lfn_len = 0; expect_seq = seq & 0x3F; }
                    if (expect_seq >= 1 && (seq & 0x3F) == expect_seq &&
                        (expect_seq - 1) * 13 + 13 < (int)sizeof(lfn)) {
                        int pos = (expect_seq - 1) * 13;
                        const uint8_t *c = entries[i].name;
                        for (int k = 0; k < 13; k++) {
                            uint16_t u = c[lofs[k]] | (uint16_t)(c[lofs[k]+1] << 8);
                            if (u == 0x0000 || u == 0xFFFF) break;
                            lfn[pos + k] = (char)(u & 0xFF);
                            if (pos + k + 1 > lfn_len) lfn_len = pos + k + 1;
                        }
                        expect_seq--;
                    } else {
                        lfn_len = 0; expect_seq = 0;
                    }
                    continue;
                }
                /* entrada normal: tenta LFN montado, depois 8.3~ */
                if (lfn_len > 0 && lfn_match(lfn, lfn_len, name)) {
                    if (out) *out = entries[i];
                    return cluster_to_sector(cluster) + s;
                }
                lfn_len = 0;
                int match = 1;
                for (int j = 0; j < 11; j++) {
                    if (entries[i].name[j] != name83[j]) { match = 0; break; }
                }
                if (match) {
                    if (out) *out = entries[i];
                    return cluster_to_sector(cluster) + s;
                }
            }
        }
        cluster = fat_read_entry(cluster);
    }
    return -1;
}

/* ~ cuidado que essa aqui morde ~ */
static int list_dir_at(uint32_t dir_cluster) {
    uint32_t cluster = dir_cluster;
    int count = 0;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t sector = cluster_to_sector(cluster);
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            uint8_t buf[512];
            if (ata_read_sector(sector + s, buf) != 0) return -1;
            fat32_dir_entry_t *entries = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0x00) goto done;
                if (entries[i].name[0] == 0xE5) continue;
                if (entries[i].attr == 0x0F) continue;
                if (entries[i].attr & 0x10) console_set_fg(C_DIR);
                else console_set_fg(C_FILE);
                for (int j = 0; j < 11; j++) {
                    char c = entries[i].name[j];
                    if (c != ' ') console_putchar(c);
                }
                if (entries[i].attr & 0x10) console_putchar('/');
                console_set_fg(C_OUT);
                uint16_t mt = entries[i].modification_time;
                uint16_t md = entries[i].modification_date;
                if (md) {
                    int hh = (mt >> 11) & 0x1F;
                    int mm = (mt >> 5) & 0x3F;
                    int mo = (md >> 5) & 0x0F;
                    int dd = md & 0x1F;
                    console_putchar(' ');
                    console_putchar('0' + mo/10); console_putchar('0' + mo%10);
                    console_putchar('-');
                    console_putchar('0' + dd/10); console_putchar('0' + dd%10);
                    console_putchar(' ');
                    console_putchar('0' + hh/10); console_putchar('0' + hh%10);
                    console_putchar(':');
                    console_putchar('0' + mm/10); console_putchar('0' + mm%10);
                }
                console_putchar('\n');
                count++;
            }
        }
        cluster = fat_read_entry(cluster);
    }
done:
    return count;
}

/* ~~ fat32_match_prefix ~~ */
int fat32_match_prefix(const char *prefix, uint32_t dir_cluster,
                       fat32_dirent_t *entries, int max_entries) {
    int count = 0, plen = 0;
    while (prefix[plen]) plen++;
    uint32_t cluster = dir_cluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8 && count < max_entries) {
        uint32_t sector = cluster_to_sector(cluster);
        for (uint32_t s = 0; s < sectors_per_cluster && count < max_entries; s++) {
            uint8_t buf[512];
            if (ata_read_sector(sector + s, buf) != 0) return count;
            fat32_dir_entry_t *e = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16 && count < max_entries; i++) {
                if (e[i].name[0] == 0x00) return count;
                if (e[i].name[0] == 0xE5) continue;
                if (e[i].attr == 0x0F) continue;
                char name[64]; int len = 0;
                for (int j = 0; j < 8 && e[i].name[j] != ' '; j++) name[len++] = e[i].name[j];
                int has_ext = 0;
                for (int j = 8; j < 11; j++) if (e[i].name[j] != ' ') has_ext = 1;
                if (has_ext && len > 0) {
                    name[len++] = '.';
                    for (int j = 8; j < 11 && e[i].name[j] != ' '; j++) name[len++] = e[i].name[j];
                }
                name[len] = '\0';
                if (plen == 0 || strncmp(name, prefix, plen) == 0) {
                    int k; for (k = 0; k < len && k < 63; k++) entries[count].name[k] = name[k];
                    entries[count].name[k] = '\0';
                    entries[count].attr = e[i].attr;
                    count++;
                }
            }
        }
        cluster = fat_read_entry(cluster);
    }
    return count;
}

static int create_entry_at(uint32_t dir_cluster, const char *name,
                           uint8_t attr) {
    uint8_t name83[11];
    name_to_83(name, name83);

    uint32_t cluster = dir_cluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t sector_num = cluster_to_sector(cluster);
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            uint8_t buf[512];
            if (ata_read_sector(sector_num + s, buf) != 0) return FAT_ERR_IO;
            fat32_dir_entry_t *entries = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0x00 ||
                    entries[i].name[0] == 0xE5) {
                    for (int j = 0; j < 11; j++)
                        entries[i].name[j] = name83[j];
                    entries[i].attr = attr;
                    entries[i].size = 0;
                    entries[i].first_cluster_low = 0;
                    entries[i].first_cluster_high = 0;
                    if (ata_write_sector(sector_num + s, buf) != 0)
                        return FAT_ERR_IO;
                    return 0;
                }
            }
        }
        cluster = fat_read_entry(cluster);
    }
    return FAT_ERR_NOSPACE;
}

/* ~ essa demorou pra debugar, respeita ~ */
static int delete_entry_at(uint32_t dir_cluster, const char *name) {
    uint32_t cluster = dir_cluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t sector_num = cluster_to_sector(cluster);
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            uint8_t buf[512];
            if (ata_read_sector(sector_num + s, buf) != 0) return -1;
            fat32_dir_entry_t *entries = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0x00) return -1;
                if (entries[i].name[0] == 0xE5) continue;
                if (entries[i].attr == 0x0F) continue;
                uint8_t name83[11];
                name_to_83(name, name83);
                int match = 1;
                for (int j = 0; j < 11; j++) {
                    if (entries[i].name[j] != name83[j]) { match = 0; break; }
                }
                if (match) {
                    entries[i].name[0] = 0xE5;
                    if (ata_write_sector(sector_num + s, buf) != 0) return -1;
                    return 0;
                }
            }
        }
        cluster = fat_read_entry(cluster);
    }
    return -1;
}

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ essa demorou pra debugar, respeita ~ */
int fat32_init(void) {
    uint8_t sector[512];
    if (ata_read_sector(0, sector) != 0) return -1;
    for (int i = 0; i < sizeof(fat32_boot_t); i++)
        ((uint8_t *)&boot)[i] = sector[i];
    if (boot.boot_sector_signature != 0xAA55) return -1;

    sectors_per_cluster = boot.sectors_per_cluster;
    first_data_sector =
        boot.reserved_sectors + boot.num_fats * boot.sectors_per_fat_32;
    fat_start_sector = boot.reserved_sectors;

    current_dir_cluster = boot.root_cluster;
    return 0;
}

/* ~~ Criando coisa nova~~ que emocionante! */
/* ~ essa demorou pra debugar, respeita ~ */
int fat32_create_file(const char *name) {
    return create_entry_at(current_dir_cluster, name, 0x20);
}

/* ~~ Deletando~~ vou sentir saudades~ brinks! */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
int fat32_delete_file(const char *name) {
    return delete_entry_at(current_dir_cluster, name);
}

/* ~~ Lendo coisinhas~ toma cuidado pra não ler lixo! */
/* ~ essa demorou pra debugar, respeita ~ */
/* ~~ fat32_walk ~ resolve caminho COM diretórios (absoluto ou relativo) ~~
 * Deixa o cwd no dir final e devolve o nome-base em out.
 * O caller DEVE restaurar current_dir_cluster depois (salve antes!)~
 * Sem isso "/share/X11/xkb/rules/evdev" era procurado DENTRO de /BIN! */
static int fat32_walk(const char *path, char *out, int outlen) {
    if (!path || !path[0]) return -1;
    char tmp[512];
    int ti = 0;
    while (path[ti] && ti < 511) { tmp[ti] = path[ti]; ti++; }
    tmp[ti] = '\0';
    char *cur = tmp;
    if (*cur == '/') { current_dir_cluster = boot.root_cluster; cur++; }
    char *last = 0, *s = cur;
    while (*s) { if (*s == '/') last = s; s++; }
    char *file = cur;
    if (last) {
        *last = '\0';
        file = last + 1;
        /* ~~ caminha componente a componente (cada um é uma entrada!) ~~ */
        char *w = cur;
        while (w && *w) {
            char *n = w;
            while (*n && *n != '/') n++;
            int end = (*n == '\0');
            if (*n) *n = '\0';
            if (w[0] && !(w[0] == '.' && w[1] == '\0')) {
                if (fat32_change_dir(w) < 0) return -1;
            }
            if (end) break;
            w = n + 1;
        }
    }
    if (!file[0]) return -1;
    int j = 0;
    while (file[j] && j < outlen - 1) { out[j] = file[j]; j++; }
    out[j] = '\0';
    return 0;
}

int fat32_read_file(const char *name, uint8_t *buffer, uint32_t size) {
    fat32_dir_entry_t entry;
    uint32_t saved = current_dir_cluster;
    char base[256];
    if (fat32_walk(name, base, sizeof(base)) < 0) {
        current_dir_cluster = saved; return -1;
    }
    int r = find_entry(current_dir_cluster, base, &entry);
    current_dir_cluster = saved;
    if (r < 0) return -1;
    uint32_t cluster = first_cluster_of(&entry);
    if (cluster == 0) return 0;
    uint32_t to_read = size < entry.size ? size : entry.size;
    return read_chain(cluster, buffer, to_read);
}

/* ~~ Escrevendo~~ não vai corromper nada, vai? >_< */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int fat32_write_file(const char *name, const uint8_t *buffer, uint32_t size) {
    fat32_dir_entry_t entry;
    if (find_entry(current_dir_cluster, name, &entry) < 0) return -1;
    uint32_t cluster = first_cluster_of(&entry);

    if (cluster == 0) {
        cluster = fat_find_free();
        if (cluster == 0) return -1;
        if (fat_write_entry(cluster, 0x0FFFFFFF) != 0) return -1;
        entry.first_cluster_low = cluster & 0xFFFF;
        entry.first_cluster_high = (cluster >> 16) & 0xFFFF;
    }

    int result = write_chain(cluster, (uint8_t *)buffer, size);
    if (result < 0) return result;

    uint8_t name83[11];
    name_to_83(name, name83);
    uint32_t c = current_dir_cluster;
    while (c >= 2 && c < 0x0FFFFFF8) {
        uint32_t s = cluster_to_sector(c);
        for (uint32_t si = 0; si < sectors_per_cluster; si++) {
            uint8_t buf[512];
            if (ata_read_sector(s + si, buf) != 0) return result;
            fat32_dir_entry_t *entries = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0xE5) continue;
                if (entries[i].attr == 0x0F) continue;
                int match = 1;
                for (int j = 0; j < 11; j++)
                    if (entries[i].name[j] != name83[j]) { match = 0; break; }
                if (match) {
                    entries[i].size = result;
                    entries[i].first_cluster_low = cluster & 0xFFFF;
                    entries[i].first_cluster_high = (cluster >> 16) & 0xFFFF;
                    if (ata_write_sector(s + si, buf) != 0) return -1;
                    return result;
                }
            }
        }
        c = fat_read_entry(c);
    }
    return result;
}

/* ~~ fat32_list_dir ~~ */
/* ~ cuidado que essa aqui morde ~ */
int fat32_list_dir(void) {
    return list_dir_at(current_dir_cluster);
}

/* ~~ fat32_change_dir ~~ */
/* ~ cuidado que essa aqui morde ~ */
int fat32_change_dir(const char *name) {
    if (name[0] == '\0' || (name[0] == '/' && name[1] == '\0')) {
        current_dir_cluster = boot.root_cluster;
        return 0;
    }
    if (name[0] == '/') {
        current_dir_cluster = boot.root_cluster;
        name++;
        if (name[0] == '\0') return 0;
    }

    fat32_dir_entry_t entry;
    if (find_entry(current_dir_cluster, name, &entry) < 0) {
        return FAT_ERR_NOTFOUND;
    }
    if (!(entry.attr & 0x10)) {
        return FAT_ERR_NOTDIR;
    }
    uint32_t new_cluster = first_cluster_of(&entry);
    if (new_cluster == 0) new_cluster = boot.root_cluster;
    current_dir_cluster = new_cluster;
    return 0;
}

/* ~~ Pegando o valor~ só confia que ta certo~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void fat32_get_cwd_name(char *out, int maxlen) {
    if (current_dir_cluster == boot.root_cluster) {
        out[0] = '/';
        out[1] = '\0';
        return;
    }

    char stack[16][11];
    int depth = 0;
    uint32_t c = current_dir_cluster;

    while (c != boot.root_cluster) {
        uint32_t parent_cluster = boot.root_cluster;
        uint32_t sector_num = cluster_to_sector(c);
        uint8_t buf[512];
        if (ata_read_sector(sector_num, buf) == 0) {
            fat32_dir_entry_t *entries = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0xE5) continue;
                if (entries[i].attr != 0x0F &&
                    entries[i].name[0] == '.' &&
                    entries[i].name[1] == '.' &&
                    entries[i].name[2] == ' ') {
                    parent_cluster = first_cluster_of(&entries[i]);
                    if (parent_cluster == 0) parent_cluster = boot.root_cluster;
                    break;
                }
            }
        }

        uint8_t found = 0;
        uint32_t pc = parent_cluster;
        while (pc >= 2 && pc < 0x0FFFFFF8 && !found) {
            uint32_t ps = cluster_to_sector(pc);
            for (uint32_t si = 0; si < sectors_per_cluster && !found; si++) {
                uint8_t pb[512];
                if (ata_read_sector(ps + si, pb) != 0) break;
                fat32_dir_entry_t *pe = (fat32_dir_entry_t *)pb;
                for (int i = 0; i < 16 && !found; i++) {
                    if (pe[i].name[0] == 0x00 ||
                        pe[i].name[0] == 0xE5) continue;
                    if (pe[i].attr == 0x0F) continue;
                    uint32_t ec = first_cluster_of(&pe[i]);
                    if (ec == c) {
                        for (int j = 0; j < 11; j++)
                            stack[depth][j] = pe[i].name[j];
                        depth++;
                        found = 1;
                    }
                }
            }
            pc = fat_read_entry(pc);
        }
        c = parent_cluster;
    }

    int pos = 0;
    out[pos++] = '/';
    for (int i = depth - 1; i >= 0; i--) {
        for (int j = 0; j < 11 && stack[i][j] != ' '; j++) {
            if (pos < maxlen - 1) out[pos++] = stack[i][j];
        }
        if (i > 0 && pos < maxlen - 1) out[pos++] = '/';
    }
    out[pos] = '\0';
}

/* ~~ Pegando o valor~ só confia que ta certo~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
uint32_t fat32_get_cwd(void) {
    return current_dir_cluster;
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void fat32_set_cwd(uint32_t cluster) {
    current_dir_cluster = cluster;
}

/* ~~ fat32_mkdir ~~ */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int fat32_mkdir(const char *name) {
    uint32_t new_cluster = fat_find_free();
    if (new_cluster == 0) return FAT_ERR_NOSPACE;

    fat32_dir_entry_t entry;
    if (find_entry(current_dir_cluster, name, &entry) >= 0)
        return FAT_ERR_EXISTS;

    int r = create_entry_at(current_dir_cluster, name, 0x10);
    if (r != 0) return r;

    uint8_t name83[11];
    name_to_83(name, name83);
    uint32_t c = current_dir_cluster;
    while (c >= 2 && c < 0x0FFFFFF8) {
        uint32_t s = cluster_to_sector(c);
        for (uint32_t si = 0; si < sectors_per_cluster; si++) {
            uint8_t buf[512];
            if (ata_read_sector(s + si, buf) != 0) return FAT_ERR_IO;
            fat32_dir_entry_t *entries = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16; i++) {
                if (entries[i].name[0] == 0xE5) continue;
                if (entries[i].attr == 0x0F) continue;
                int match = 1;
                for (int j = 0; j < 11; j++)
                    if (entries[i].name[j] != name83[j]) { match = 0; break; }
                if (match && (entries[i].attr & 0x10)) {
                    entries[i].first_cluster_low = new_cluster & 0xFFFF;
                    entries[i].first_cluster_high =
                        (new_cluster >> 16) & 0xFFFF;
                    if (ata_write_sector(s + si, buf) != 0) return FAT_ERR_IO;

                    uint8_t dirbuf[512] = {0};
                    fat32_dir_entry_t *de = (fat32_dir_entry_t *)dirbuf;
                    de[0].name[0] = '.';
                    de[0].attr = 0x10;
                    for (int j = 1; j < 11; j++) de[0].name[j] = ' ';
                    de[0].first_cluster_low = new_cluster & 0xFFFF;
                    de[0].first_cluster_high =
                        (new_cluster >> 16) & 0xFFFF;
                    de[1].name[0] = '.';
                    de[1].name[1] = '.';
                    de[1].attr = 0x10;
                    for (int j = 2; j < 11; j++) de[1].name[j] = ' ';
                    de[1].first_cluster_low = current_dir_cluster & 0xFFFF;
                    de[1].first_cluster_high =
                        (current_dir_cluster >> 16) & 0xFFFF;
                    uint32_t ds = cluster_to_sector(new_cluster);
                    if (fat_write_entry(new_cluster, 0x0FFFFFFF) != 0) return FAT_ERR_IO;
                    if (ata_write_sector(ds, dirbuf) != 0) return FAT_ERR_IO;
                    return 0;
                }
            }
        }
        c = fat_read_entry(c);
    }
    return -1;
}

/* ~~ fat32_rename ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
int fat32_rename(const char *oldname, const char *newname) {
    uint8_t old83[11], new83[11];
    name_to_83(oldname, old83);
    name_to_83(newname, new83);
    uint32_t c = current_dir_cluster;
    while (c >= 2 && c < 0x0FFFFFF8) {
        uint32_t s = cluster_to_sector(c);
        for (uint32_t si = 0; si < sectors_per_cluster; si++) {
            uint8_t buf[512];
            if (ata_read_sector(s + si, buf) != 0) return FAT_ERR_IO;
            fat32_dir_entry_t *e = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16; i++) {
                if (e[i].name[0] == 0x00 || e[i].name[0] == 0xE5) continue;
                if (e[i].attr == 0x0F) continue;
                int match = 1;
                for (int j = 0; j < 11; j++)
                    if (e[i].name[j] != old83[j]) { match = 0; break; }
                if (!match) continue;
                for (int j = 0; j < 11; j++) e[i].name[j] = new83[j];
                if (ata_write_sector(s + si, buf) != 0) return FAT_ERR_IO;
                return 0;
            }
        }
        c = fat_read_entry(c);
    }
    return FAT_ERR_NOTFOUND;
}

/* ~~ fat32_rmdir ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
int fat32_rmdir(const char *name) {
    uint8_t name83[11];
    name_to_83(name, name83);
    uint32_t c = current_dir_cluster;
    while (c >= 2 && c < 0x0FFFFFF8) {
        uint32_t s = cluster_to_sector(c);
        for (uint32_t si = 0; si < sectors_per_cluster; si++) {
            uint8_t buf[512];
            if (ata_read_sector(s + si, buf) != 0) return FAT_ERR_IO;
            fat32_dir_entry_t *e = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16; i++) {
                if (e[i].name[0] == 0x00 || e[i].name[0] == 0xE5) continue;
                if (e[i].attr == 0x0F) continue;
                int match = 1;
                for (int j = 0; j < 11; j++)
                    if (e[i].name[j] != name83[j]) { match = 0; break; }
                if (!match) continue;
                if (!(e[i].attr & 0x10)) return FAT_ERR_NOTDIR;
                uint32_t cl = ((uint32_t)e[i].first_cluster_high << 16) | e[i].first_cluster_low;
                if (cl != 0) {
                    uint8_t db[512];
                    if (ata_read_sector(cluster_to_sector(cl), db) == 0) {
                        fat32_dir_entry_t *de = (fat32_dir_entry_t *)db;
                        int empty = 1;
                        for (int ei = 2; ei < 16; ei++) {
                            if (de[ei].name[0] != 0x00 && de[ei].name[0] != 0xE5)
                                { empty = 0; break; }
                        }
                        if (!empty) return FAT_ERR_NOTEMPTY;
                    }
                    uint32_t next;
                    do {
                        next = fat_read_entry(cl);
                        fat_write_entry(cl, 0);
                        cl = next;
                    } while (cl >= 2 && cl < 0x0FFFFFF8);
                }
                e[i].name[0] = 0xE5;
                if (ata_write_sector(s + si, buf) != 0) return FAT_ERR_IO;
                return 0;
            }
        }
        c = fat_read_entry(c);
    }
    return FAT_ERR_NOTFOUND;
}

/* ~~ fat32_stat ~~ */
int fat32_stat(const char *name, uint32_t *size, uint8_t *attr,
               uint16_t *mtime, uint16_t *mdate) {
    fat32_dir_entry_t e;
    uint32_t saved = current_dir_cluster;
    char base[256];
    if (fat32_walk(name, base, sizeof(base)) < 0) {
        current_dir_cluster = saved; return FAT_ERR_NOTFOUND;
    }
    int r = find_entry(current_dir_cluster, base, &e);
    current_dir_cluster = saved;
    if (r < 0) return FAT_ERR_NOTFOUND;
    if (size) *size = e.size;
    if (attr) *attr = e.attr;
    if (mtime) *mtime = e.modification_time;
    if (mdate) *mdate = e.modification_date;
    return 0;
}

/* ~~ fat32_sync ~~ */
/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
void fat32_sync(void) {
}




/* ♥ fat32.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
