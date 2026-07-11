#ifndef FAT32_H
#define FAT32_H
#include <stdint.h>

/*
 * Códigos de erro das operações FAT32.
 * Usados internamente pelo driver e expostos aos comandos do shell
 * para gerar mensagens de erro específicas (ex: "Nao encontrado", "Disco cheio").
 */
#define FAT_ERR_OK       0   /* Operação concluída com sucesso */
#define FAT_ERR_NOTFOUND -1  /* Arquivo ou diretório não existe */
#define FAT_ERR_EXISTS   -2  /* Já existe entrada com este nome */
#define FAT_ERR_IO       -3  /* Falha de leitura/gravação no disco */
#define FAT_ERR_NOSPACE  -4  /* Disco cheio (sem clusters livres) */
#define FAT_ERR_NOTDIR   -5  /* Entrada não é um diretório */

typedef struct {
    uint8_t  jump[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;
    uint16_t fsinfo_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved2;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
    uint8_t  boot_code[420];
    uint16_t boot_sector_signature;
} __attribute__((packed)) fat32_boot_t;

typedef struct {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t access_date;
    uint16_t first_cluster_high;
    uint16_t modification_time;
    uint16_t modification_date;
    uint16_t first_cluster_low;
    uint32_t size;
} __attribute__((packed)) fat32_dir_entry_t;

int fat32_init(void);
int fat32_create_file(const char *name);
int fat32_delete_file(const char *name);
int fat32_read_file(const char *name, uint8_t *buffer, uint32_t size);
int fat32_write_file(const char *name, const uint8_t *buffer, uint32_t size);
int fat32_list_dir(void);

// new: directory navigation
int fat32_change_dir(const char *name);
void fat32_get_cwd_name(char *out, int maxlen);
uint32_t fat32_get_cwd(void);
int fat32_mkdir(const char *name);

#endif
