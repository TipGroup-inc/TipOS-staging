/* moe moe kyun <3 */
/* ~~ VFS mínima do TipOS ~ camada fina entre syscalls e backends ~~
 * Fase 1 da #70: dispatch por backend + caminhos ABSOLUTOS.
 * Fase 2 (com o ext2 write): vnodes de verdade e cwd por processo~ */

#ifndef VFS_H
#define VFS_H
#include <stdint.h>

#define VFS_MAX_PATH 512

/* ~~ monta o backend: ext2 se der, senão fat32 continua~~
 * Chamar UMA vez no boot depois do ata_init. Retorna o backend ativo:
 * 0 = ext2 · 1 = fat32 · -1 = nenhum montou (chora)~ */
int vfs_mount(void);

/* ~~ resolve path (absoluto ou relativo ao cwd do processo) pra abs~~
 * cwd vem do PCB do processo chamador — acabou o global compartilhado~
 * Retorna out NUL-terminado ou 0 em erro. "." e ".." suportados~ */
int vfs_abs_path(const char *cwd, const char *path, char *out, int outlen);

/* ~~ operações de arquivo (todas recebem path ABSOLUTO) ~~ */
int vfs_read_file(const char *abs, uint8_t *buf, uint32_t count);
int vfs_write_at(const char *abs, const uint8_t *buf, uint32_t count, uint32_t offset); /* cria se não existe */
int vfs_stat_size_attr(const char *abs, uint32_t *size, uint8_t *attr);
int vfs_create_file(const char *abs);
int vfs_unlink(const char *abs);
int vfs_mkdir(const char *abs);
int vfs_chdir_isdir(const char *abs); /* 1 se é diretório navegável */

#endif /* VFS_H */
