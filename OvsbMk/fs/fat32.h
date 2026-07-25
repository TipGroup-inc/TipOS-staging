/* moe moe kyun <3 */
/* ♥ filesystem ~ onde os byte moram organizadinho (ou n) ~
 * arquivo: fat32.h ~ funcoes anotadas: 0
 */
/* ♥ fat32.h ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ FAT32 HEADER ~ "Sistema de arquivos gordinho~ mas funciona!"
 * Dica: O FAT32 tem 3 areas: Boot Sector, FAT, Dados~
 * Os clusters sao numerados a partir de 2~ entao nao confunda!
 * Cada entrada de diretorio tem 32 bytes~ nome 8.3 maiusculo!
 * Se der erro, veja os FAT_ERR_* codigos~ facil de debugar~ kyun! */

#ifndef FAT32_H
#define FAT32_H
#include <stdint.h>

#define FAT_ERR_OK       0
#define FAT_ERR_NOTFOUND -1
#define FAT_ERR_EXISTS   -2
#define FAT_ERR_IO       -3
#define FAT_ERR_NOSPACE  -4
#define FAT_ERR_NOTDIR   -5
#define FAT_ERR_NOTEMPTY -6

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

int fat32_change_dir(const char *name);
void fat32_get_cwd_name(char *out, int maxlen);
uint32_t fat32_get_cwd(void);
void fat32_set_cwd(uint32_t cluster);
int fat32_mkdir(const char *name);
int fat32_rename(const char *oldname, const char *newname);
int fat32_rmdir(const char *name);
/* ~~ fat32_stat ~~ */
int fat32_stat(const char *name, uint32_t *size, uint8_t *attr,
               uint16_t *mtime, uint16_t *mdate);
void fat32_sync(void);

typedef struct {
    char name[64];
    uint8_t attr;
} fat32_dirent_t;

/* ~~ fat32_match_prefix ~~ */
int fat32_match_prefix(const char *prefix, uint32_t dir_cluster,
                       fat32_dirent_t *entries, int max_entries);

#endif




/* ♥ fat32.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
