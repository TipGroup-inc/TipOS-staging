/* moe moe kyun <3 */
/* ~~ vfs.c ~ a fundação da #70 ~~
 * Backend único na raiz (ext2 preferido, fat32 de fallback) + resolução
 * de caminhos absolutos/relativos. Os handlers de syscall chamam SÓ
 * essas funções — nenhum deles sabe mais qual FS tá por trás~ ☆ */

#include "vfs.h"
#include <stdint.h>
#include <stdbool.h>
#define NULL ((void *)0)

/* ~~ backends ~~
 * FAT32: fs/fat32.c (C) — legacy, 8.3+LFN-read, cwd global (só leitura aqui)
 * EXT2:  fs/ext2.zig (Zig) — POSIX tree, caminhos absolutos nativos~ */
extern int fat32_read_file(const char *name, uint8_t *buffer, uint32_t size);
extern int fat32_stat(const char *name, uint32_t *size, uint8_t *attr,
                      uint16_t *mtime, uint16_t *mdate);
extern int fat32_write_file(const char *name, const uint8_t *buffer, uint32_t size);
extern int fat32_create_file(const char *name);
extern int fat32_delete_file(const char *name);
extern int fat32_mkdir(const char *name);

extern int ext2new_mount(void);
extern int ext2new_read_file(const char *path, unsigned char *buffer, unsigned int size);
extern int ext2new_stat(const char *path, unsigned int *size, bool *is_dir);
extern int ext2new_read_at(const char *path, unsigned char *buffer, unsigned int size, unsigned int offset);
extern void ext2new_sync(void);
extern int ext2_create_file(const char *path);
extern int ext2_write_at(const char *path, unsigned char *buffer,
                         unsigned int size, unsigned int offset);

int g_vfs_backend = -1; /* 0=ext2 · 1=fat32 · -1=nada */

/* ~~ serial direto (evita dependência do console aqui) ~~ */
extern void serial_puts(const char *s);

int vfs_mount(void) {
    if (ext2new_mount() == 0) {
        g_vfs_backend = 0;
        serial_puts("[VFS] backend = EXT2\r\n");
    } else {
        g_vfs_backend = 1;
        serial_puts("[VFS] backend = FAT32\r\n");
    }
    return g_vfs_backend;
}

/* ~~ vfs_abs_path ~ junta cwd+path e normaliza / e .. ~~
 * Regras: path absoluto substitui; relativo concatena; ".." sobe um~
 * Não usa libc — freestanding, tudo na mão mesmo~ kyun! */
int vfs_abs_path(const char *cwd, const char *path, char *out, int outlen) {
    if (!path || !path[0]) return -1;

    char base[VFS_MAX_PATH];
    if (path[0] == '/') {
        int i = 0;
        while (path[i] && i < VFS_MAX_PATH - 1) { base[i] = path[i]; i++; }
        base[i] = '\0';
    } else {
        int i = 0;
        const char *c = cwd ? cwd : "/";
        while (*c && i < VFS_MAX_PATH - 1) { base[i++] = *c++; }
        if (i > 0 && base[i - 1] != '/') base[i++] = '/';
        while (*path && i < VFS_MAX_PATH - 1) { base[i++] = *path++; }
        base[i] = '\0';
    }

    /* normaliza: separa por '/', processa "" "." ".." com stack manual */
    char comps[32][64];
    int depths[32];
    int n = 0;
    char cur[64];
    int ci = 0;
    int depth_now = 0;

    for (int i = 0; ; i++) {
        char ch = base[i];
        if (ch == '/' || ch == '\0') {
            cur[ci] = '\0';
            if (ci == 0 || (ci == 1 && cur[0] == '.')) {
                /* vazio ou "." — ignora~ */
            } else if (ci == 2 && cur[0] == '.' && cur[1] == '.') {
                if (n > 0) n--; /* sobe~ */
                if (n > 0) depth_now = depths[n - 1];
            } else {
                if (n < 32) {
                    for (int j = 0; j <= ci; j++) comps[n][j] = cur[j];
                    depths[n] = depth_now++;
                    n++;
                }
            }
            ci = 0;
            if (ch == '\0') break;
            continue;
        }
        if (ci < 63) cur[ci++] = ch;
    }

    /* monta out: '/' + comps[0..n-1] separados por '/' */
    int pos = 0;
    out[pos++] = '/';
    for (int c = 0; c < n; c++) {
        for (int j = 0; comps[c][j]; j++)
            if (pos < outlen - 2) out[pos++] = comps[c][j];
        if (c < n - 1 && pos < outlen - 2) out[pos++] = '/';
    }
    if (pos == 1) out[pos++] = '/'; /* raiz pura~ */
    out[pos] = '\0';
    return 0;
}

/* ~~ dispatch genérico~~ */
int vfs_read_file(const char *abs, uint8_t *buf, uint32_t count) {
    return vfs_read_at(abs, buf, count, 0);
}

int vfs_read_at(const char *abs, uint8_t *buf, uint32_t count, uint32_t offset) {
    if (g_vfs_backend == 0) return ext2new_read_at(abs, buf, count, offset);
    /* fat32 MVP: lê do início sempre (arquivos pequenos só~) */
    return fat32_read_file(abs, buf, count);
}

int vfs_stat_size_attr(const char *abs, uint32_t *size, uint8_t *attr) {
    if (g_vfs_backend == 0) {
        unsigned int sz = 0;
        bool is_dir = false;
        if (ext2new_stat(abs, &sz, &is_dir) != 0) return -1;
        if (size) *size = sz;
        if (attr) *attr = is_dir ? 0x10 : 0x20;
        return 0;
    }
    return fat32_stat(abs, size, attr, NULL, NULL);
}

int vfs_write_at(const char *abs, const uint8_t *buf, uint32_t count, uint32_t offset) {
    if (g_vfs_backend == 0)
        return ext2_write_at(abs, (unsigned char *)buf, count, offset);
    /* fat32 MVP: escreve do zero (sem offset), criando se preciso~ */
    if (offset != 0) {
        if (fat32_create_file(abs) == 0) { /* já existia */ }
    }
    return fat32_write_file(abs, buf, count);
}

int vfs_create_file(const char *abs) {
    if (g_vfs_backend == 0) return ext2_create_file(abs);
    return fat32_create_file(abs);
}

int vfs_unlink(const char *abs) {
    if (g_vfs_backend == 0) return -1; /* TODO no ext2 write phase 2 */
    extern int fat32_delete_file(const char *name);
    return fat32_delete_file((char *)abs);
}

int vfs_mkdir(const char *abs) {
    if (g_vfs_backend == 0) return -1; /* TODO fase 2 */
    extern int fat32_mkdir(const char *name);
    return fat32_mkdir((char *)abs);
}

int vfs_chdir_isdir(const char *abs) {
    uint32_t sz = 0; uint8_t at = 0;
    if (vfs_stat_size_attr(abs, &sz, &at) != 0) return 0;
    if (g_vfs_backend == 0) return 1; /* ext2 stat só dá size hoje — assume dir navegável */
    return (at & 0x10) ? 1 : 0; /* attr FAT32 bit diretório */
}
