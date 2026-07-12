/*
 * FAT32 — Driver de sistema de arquivos FAT32 para OvsbMkM.
 *
 * Histórico de bugs corrigidos:
 *
 * 1. name_to_83(): não respeitava terminador nulo da string de entrada.
 *    Para nomes curtos ("X", "APPS"), bytes após '\0' continham lixo da pilha,
 *    corrompendo o buffer name83 e fazendo find_entry() falhar.
 *    Corrigido: iterar char a char, parar no '\0', preencher resto com ' '.
 *
 * 2. read_chain()/write_chain(): divisão inteira to_read/512 truncava
 *    leituras/escritas < 512 bytes para 0 setores (ex: HELLO com 180 bytes
 *    era lido como 0 bytes, retornando sucesso sem ler dados).
 *    Corrigido: processar setores completos + remainder parcial.
 *
 * 3. create_entry_at(): retornava -1 genérico; agora retorna FAT_ERR_IO
 *    em falha de E/S e FAT_ERR_NOSPACE se diretório cheio.
 *
 * 4. fat32_mkdir(): entrada '.' e '..' tinha bytes nulos (0x00) no nome
 *    em vez de espaços (0x20), fazendo list_dir_at() mostrar caracteres
 *    estranhos. Corrigido: preencher com ' ' explicitamente.
 *
 * 5. Códigos de erro: shell_cmds.c agora usa print_err() para mostrar
 *    mensagens descritivas em português.
 */

#include "fat32.h"
#include "../drivers/ata.h"
#include "../kernel/memory.h"
#include "../kernel/kernel.h"
#include <stdint.h>

/* Funções VGA usadas para debug e saída */
extern void vga_putchar(char c);
extern void vga_puts(const char *s);
extern int strncmp(const char *a, const char *b, int n);

/* Estado global do driver FAT32 */
static fat32_boot_t boot;              /* Boot sector lido no init */
static uint32_t first_data_sector;     /* Primeiro setor da área de dados */
static uint32_t sectors_per_cluster;   /* Setores por cluster (do boot sector) */
static uint32_t current_dir_cluster;   /* Cluster do diretório atual (CWD) */
static uint32_t fat_start_sector;      /* Primeiro setor da FAT */

/*
 * Converte nome de arquivo para formato 8.3 do FAT32.
 *
 * BUG ORIGINAL (FIX crítico):
 * A implementação antiga iterava 11 posições fixas lendo de `name` sem parar
 * no '\0'. Para nomes curtos como "X" (1 char) ou "APPS" (4 chars), os índices
 * além do terminador liam lixo da CALL STACK ANTERIOR, que continha restos de
 * outras funções (endereços de retorno, variáveis locais, etc.). Esse lixo era
 * escrito no vetor `out` (name83).
 *
 * Consequência: find_entry() comparava o name83 corrompido contra a entrada
 * do diretório no disco (que estava correta porque create_entry_at ESCREVE
 * o nome corretamente). A comparação NUNCA correspondia, e cd/mkdir/exec
 * falhavam com "Nao encontrado" mesmo com a entrada visível no ls.
 *
 * Exemplo concreto: name="X"
 *   name[0]='X', name[1]='\0', name[2..10]=LIXO_DA_PILHA
 *   out[0]='X', out[1]=' ', out[2..10]=LIXO (ex: "\x01\x00\x00\x00...")
 *   Comparação com entrada "X          " no disco: falha no byte 2!
 *
 * FIX: iterar char a char parando no '\0', copiar máx. 8 chars do nome base
 * (antes do '.') e 3 da extensão, preencher resto com 0x20.
 */
static uint8_t name_to_83(const char *name, uint8_t out[11]) {
    /* FAT32 exige 0x20 (espaço) para posições não preenchidas */
    for (int i = 0; i < 11; i++) out[i] = ' ';
    /* Entradas especiais FAT32: "." e ".." */
    if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
        int i = 0;
        while (name[i] && i < 11) { out[i] = name[i]; i++; }
        return 0;
    }
    /* Localiza o ponto separador nome.ext (se houver) */
    const char *dot = 0;
    const char *p = name;
    while (*p) {
        if (*p == '.') { dot = p; break; }
        p++;
    }
    /* Copia o nome base (até 8 caracteres antes do ponto) */
    int pos = 0;
    while (name != dot && *name && pos < 8) {
        char c = *name++;
        if (c >= 'a' && c <= 'z') c -= 0x20; /* upper-case */
        out[pos++] = c;
    }
    /* Copia a extensão (até 3 caracteres depois do ponto) */
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

/* Converte número de cluster para setor LBA absoluto */
static uint32_t cluster_to_sector(uint32_t cluster) {
    return first_data_sector + (cluster - 2) * sectors_per_cluster;
}

/* Lê entrada da FAT (valor de 28 bits) para um dado cluster */
static uint32_t fat_read_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_sector + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    uint8_t sector[512];
    if (ata_read_sector(fat_sector, sector) != 0) return 0;
    uint32_t val = *(uint32_t *)(sector + ent_offset) & 0x0FFFFFFF;
    return val;
}

/* Escreve valor de 28 bits na FAT para um dado cluster */
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

/* Procura o primeiro cluster livre na FAT (valor 0 = livre) */
static uint32_t fat_find_free(void) {
    uint32_t total_clusters =
        (boot.total_sectors_32 - first_data_sector) / sectors_per_cluster;
    for (uint32_t c = 2; c < total_clusters + 2; c++) {
        if (fat_read_entry(c) == 0) return c;
    }
    return 0; /* disco cheio */
}

/* Retorna o último cluster de uma cadeia FAT */
static uint32_t fat_chain_last(uint32_t start) {
    uint32_t c = start;
    while (1) {
        uint32_t next = fat_read_entry(c);
        if (next >= 0x0FFFFFF8) return c; /* EOC (End Of Chain) */
        if (next == 0) return c;          /* livre (não deveria ocorrer) */
        c = next;
    }
}

/*
 * Lê uma cadeia de clusters do disco para um buffer.
 *
 * BUG ORIGINAL (FIX):
 * A divisão `to_read / 512` resultava em 0 quando to_read < 512,
 * fazendo o loop de setores NUNCA executar — mas offset ainda era
 * incrementado, retornando sucesso (bytes lidos = to_read) com buffer zerado.
 * Isso afetava qualquer arquivo < 512 bytes (ex: HELLO com 180 bytes).
 *
 * FIX: processar setores completos (num_sectors) e depois o resto parcial
 * (remainder) lendo o setor parcial e copiando apenas os bytes necessários.
 */
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

/*
 * Escreve dados em uma cadeia de clusters.
 * Mesmo bug do read_chain: to_write/512 = 0 para to_write < 512.
 * FIX: processar setores completos + remainder parcial,
 * com read-modify-write para o setor parcial (preservar dados existentes).
 */
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
            /* Lê setor existente, modifica bytes parciais, escreve de volta */
            uint8_t tmp[512];
            if (ata_read_sector(sector + num_sectors, tmp) != 0) return -1;
            for (uint32_t b = 0; b < remainder; b++)
                tmp[b] = buffer[offset + num_sectors * 512 + b];
            if (ata_write_sector(sector + num_sectors, tmp) != 0) return -1;
        }
        offset += to_write;
        size -= to_write;

        /* Avança para próximo cluster, alocando se necessário */
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

/* Aloca N clusters consecutivos a partir de um cluster inicial */
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

/* Extrai o número do primeiro cluster de uma entrada de diretório */
static uint32_t first_cluster_of(fat32_dir_entry_t *e) {
    return ((uint32_t)e->first_cluster_high << 16) | e->first_cluster_low;
}

/*
 * Busca entrada por nome em uma cadeia de clusters de diretório.
 * Retorna o número do setor onde a entrada foi encontrada,
 * ou -1 (FAT_ERR_NOTFOUND) se não existir.
 */
static int find_entry(uint32_t dir_cluster, const char *name,
                      fat32_dir_entry_t *out) {
    uint8_t name83[11];
    name_to_83(name, name83);

    uint32_t cluster = dir_cluster;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t sector = cluster_to_sector(cluster);
        for (uint32_t s = 0; s < sectors_per_cluster; s++) {
            uint8_t buf[512];
            if (ata_read_sector(sector + s, buf) != 0) return -1;
            fat32_dir_entry_t *entries = (fat32_dir_entry_t *)buf;
            for (int i = 0; i < 16; i++) {
                /* 0x00 = fim do diretório (entradas seguintes são vazias) */
                if (entries[i].name[0] == 0x00) return -1;
                /* 0xE5 = entrada deletada */
                if (entries[i].name[0] == 0xE5) continue;
                /* 0x0F = Long File Name (entrada LFN, ignorar) */
                if (entries[i].attr == 0x0F) continue;
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

/* Lista entradas de um diretório na saída VGA */
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
                /* Imprime nome 8.3 pulando espaços de padding */
                if (entries[i].attr & 0x10) set_vga_color(C_DIR);
                else set_vga_color(C_FILE);
                for (int j = 0; j < 11; j++) {
                    char c = entries[i].name[j];
                    if (c != ' ') vga_putchar(c);
                }
                if (entries[i].attr & 0x10) vga_puts("/");
                set_vga_color(C_OUTPUT);
                // timestamp
                uint16_t mt = entries[i].modification_time;
                uint16_t md = entries[i].modification_date;
                if (md) {
                    int hh = (mt >> 11) & 0x1F;
                    int mm = (mt >> 5) & 0x3F;
                    int mo = (md >> 5) & 0x0F;
                    int dd = md & 0x1F;
                    vga_putchar(' ');
                    vga_putchar('0' + mo/10); vga_putchar('0' + mo%10);
                    vga_putchar('-');
                    vga_putchar('0' + dd/10); vga_putchar('0' + dd%10);
                    vga_putchar(' ');
                    vga_putchar('0' + hh/10); vga_putchar('0' + hh%10);
                    vga_putchar(':');
                    vga_putchar('0' + mm/10); vga_putchar('0' + mm%10);
                }
                vga_putchar('\n');
                count++;
            }
        }
        cluster = fat_read_entry(cluster);
    }
done:
    return count;
}

/* Encontra entradas de diretório com prefixo correspondente */
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
                    // Check for common extension separators
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

/*
 * Cria uma nova entrada de diretório.
 * Encontra um slot livre (0x00 ou 0xE5) e escreve nome + atributos.
 * O cluster do arquivo é definido posteriormente pelo chamador.
 */
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
                    /* Slot livre: escreve nome e atributos */
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
    return FAT_ERR_NOSPACE; /* diretório cheio, sem slots livres */
}

/* Marca uma entrada como deletada (0xE5 no primeiro byte do nome) */
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

// ── API Pública ──

/* Inicializa o driver FAT32 lendo o boot sector e configurando geometria */
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

    current_dir_cluster = boot.root_cluster; /* CWD inicial = raiz */
    return 0;
}

/* Cria arquivo vazio no diretório atual */
int fat32_create_file(const char *name) {
    return create_entry_at(current_dir_cluster, name, 0x20);
}

/* Remove arquivo do diretório atual */
int fat32_delete_file(const char *name) {
    return delete_entry_at(current_dir_cluster, name);
}

/* Lê conteúdo de um arquivo para buffer */
int fat32_read_file(const char *name, uint8_t *buffer, uint32_t size) {
    fat32_dir_entry_t entry;
    if (find_entry(current_dir_cluster, name, &entry) < 0) return -1;
    uint32_t cluster = first_cluster_of(&entry);
    if (cluster == 0) return 0;
    uint32_t to_read = size < entry.size ? size : entry.size;
    return read_chain(cluster, buffer, to_read);
}

/* Escreve buffer em um arquivo existente */
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

    /* Só atualiza diretório se write_chain teve sucesso */
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

/* Lista o diretório atual na saída VGA */
int fat32_list_dir(void) {
    return list_dir_at(current_dir_cluster);
}

/*
 * Muda para um subdiretório.
 * Aceita caminhos absolutos (começando com /) e relativos.
 * "cd" sem argumentos ou "cd /" volta à raiz.
 */
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

/*
 * Reconstrói o caminho absoluto do diretório atual (para pwd).
 * Sobe pela cadeia de ".." até a raiz, coletando nomes.
 */
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

/* Retorna o cluster do diretório atual (para uso interno) */
uint32_t fat32_get_cwd(void) {
    return current_dir_cluster;
}

void fat32_set_cwd(uint32_t cluster) {
    current_dir_cluster = cluster;
}

/*
 * Cria um novo diretório.
 *
 * Fluxo:
 * 1. Aloca um cluster livre para o novo diretório
 * 2. Verifica se já existe entrada com o mesmo nome
 * 3. Cria entrada no diretório atual (attr=0x10, cluster=0 inicialmente)
 * 4. Atualiza a entrada com o número do cluster alocado
 * 5. Inicializa o novo diretório com as entradas . (self) e .. (parent)
 * 6. Marca o cluster como ocupado na FAT (0x0FFFFFFF = EOC)
 */
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

                    /* Formata o novo diretório: entradas . e .. */
                    uint8_t dirbuf[512] = {0};
                    fat32_dir_entry_t *de = (fat32_dir_entry_t *)dirbuf;
                    /* Entrada . (próprio diretório) */
                    de[0].name[0] = '.';
                    de[0].attr = 0x10;
                    for (int j = 1; j < 11; j++) de[0].name[j] = ' ';
                    de[0].first_cluster_low = new_cluster & 0xFFFF;
                    de[0].first_cluster_high =
                        (new_cluster >> 16) & 0xFFFF;
                    /* Entrada .. (diretório pai) */
                    de[1].name[0] = '.';
                    de[1].name[1] = '.';
                    de[1].attr = 0x10;
                    for (int j = 2; j < 11; j++) de[1].name[j] = ' ';
                    de[1].first_cluster_low = current_dir_cluster & 0xFFFF;
                    de[1].first_cluster_high =
                        (current_dir_cluster >> 16) & 0xFFFF;
                    /* Marca cluster como EOC na FAT */
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
                /* Verifica se está vazio: só . e .. */
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
                    /* Libera a cadeia de clusters */
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

int fat32_stat(const char *name, uint32_t *size, uint8_t *attr,
               uint16_t *mtime, uint16_t *mdate) {
    fat32_dir_entry_t e;
    if (find_entry(current_dir_cluster, name, &e) < 0) return FAT_ERR_NOTFOUND;
    if (size) *size = e.size;
    if (attr) *attr = e.attr;
    if (mtime) *mtime = e.modification_time;
    if (mdate) *mdate = e.modification_date;
    return 0;
}

void fat32_sync(void) {
    // No write cache yet — writes are direct to disk.
    // Reserved for future use when we add FAT caching.
}
