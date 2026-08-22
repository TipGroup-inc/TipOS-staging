/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: syscall.c ~ funcoes anotadas: 6
 */
/* ~*~ syscall.c ~ "O kernel faz tudo pra vc~ hihi!" ~*~
 * Aqui é onde a magica acontece: as syscalls que o userspace chama via int 0x80.
 * Cada caso é uma chamada de sistema diferente, tipo um menu de restaurante
 * onde o usuario pede "me da um mmap" e o kernel responde "sim senhor!".
 * 
 * Mudei varias coisas aqui durante a sessão de debugging:
 * - SYS_disp_get_fb: tava com a2 e a4 trocados (width vs pitch), arrumei~
 * - SYS_disp_get_fb: adicionei guard pra g_fb.width==0 (nao divida por zero!)
 * - SYS_mouse_read: implementei de verdade (tava so retornando 0)
 * - SYS_spawn: cria processo filho com PML4 propria (isolamento!)
 * - SYS_execve: carrega binario via mach_o_load direto
 * - Debug: print [SC:...] pra cada syscall (polui o log mas ajuda)
 * 
 * Se quebrar, chora pro /dev/null~ kyun! <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#include "syscall.h"
#include "../fs/vfs.h"
#include "process.h"
#include "memory.h"
#include "console.h"
#include "serial.h"
#include "pit.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/virtio_gpu.h"
#include "../fs/fat32.h"
#include "../kernel/utils.h"
#include "../kernel/mach_o.h"
#include "../kernel/vm_map.h"
#include "../lib/gui/vesa.h"

extern void execute(const char *cmd);

/* ~~ raw input pro Xorg (Zig) ~~
 * Scan codes crus do teclado e bytes crus do mouse~ */
extern int  kbd_raw_avail(void);
extern unsigned char kbd_raw_read(void);
extern int  mice_avail(void);
extern unsigned char mice_read(void);

#define MAX_FDS 16
static struct {
    int used;
    int type;          /* 0=arquivo, 1=eventfd, 2=timerfd, 3=pipe, 4=socket,
                          5=/dev/fb0, 6=/dev/ttyN, 7=/dev/input/mice */
    int flags;         /* O_NONBLOCK etc (fcntl) */
    int pipe_idx;      /* pipe: indice na tabela de pipes */
    int sock_idx;      /* socket: indice na tabela de sockets */
    char name[256];
    uint32_t pos;
    uint64_t counter;  /* eventfd: contador; timerfd: intervalo (ms) */
} fds[MAX_FDS];

/* ~~ device fds pro Xorg ~~
 * type 5 = /dev/fb0 (framebuffer: ioctls FBIO, mmap)
 * type 6 = /dev/ttyN (console: ioctls VT e KD, read = scan codes crus)
 * type 7 = /dev/input/mice (read = bytes crus do PS/2) */
#define FD_FB    5
#define FD_TTY   6
#define FD_MICE  7

/* ~~ pipes (issue #52) ~~ um buffer por pipe, read/write pos separados */
#define MAX_PIPES 8
static struct {
    int used;
    uint8_t buf[4096];
    uint32_t rpos, wpos;
} pipes[MAX_PIPES];

/* ~~ sockets AF_UNIX (issue #49) ~~
 * Cada socket tem um buffer próprio (como um pipe) e aponta pro peer.
 * write() grava no buffer do peer; read() lê do próprio buffer.
 * socketpair cria dois fds já conectados (peer <-> peer).
 * listener tem fila de pending: clientes que conectaram e esperam accept~ */
#define MAX_SOCKS 16
#define SOCK_QUEUE 8
static struct {
    int used;
    char path[256];      /* path do bind (ex: /tmp/test.sock) */
    int listening;       /* chamou listen() */
    int peer;            /* índice do socket parceiro, -1 se sem */
    int nonblock;        /* O_NONBLOCK herdado do fd */
    int pending[SOCK_QUEUE]; /* fila de conexões aguardando accept */
    int npending;
    uint8_t buf[4096];   /* buffer de leitura (o peer escreve aqui) */
    uint32_t rpos, wpos;
} socks[MAX_SOCKS];

/* ~~ O_NONBLOCK (fcntl) ~~ */
#define O_NONBLOCK 0x800
#define O_CLOEXEC  0x80000

struct timeval { uint64_t tv_sec; uint64_t tv_usec; };
struct stat   { uint32_t st_size; };

/* ~~ ELF64 headers (pro Linux ABI) ~~ */
typedef struct {
    unsigned char e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum;
    uint16_t e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;
typedef struct {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;
#define PT_LOAD 1

extern void *elf64_load_into_pml4(const uint8_t *data, uint32_t len, uint64_t pml4);
extern uint64_t elf64_pie_base(void);

/* ~ cuidado que essa aqui morde ~ */
void syscall_init(void) {
    for (int i = 0; i < MAX_FDS; i++) fds[i].used = 0;
    for (int i = 0; i < MAX_PIPES; i++) pipes[i].used = 0;
    for (int i = 0; i < MAX_SOCKS; i++) { socks[i].used = 0; socks[i].peer = -1; socks[i].npending = 0; }
}

/* ~ essa demorou pra debugar, respeita ~ */
void fds_cleanup(void) {
    for (int i = 3; i < MAX_FDS; i++) fds[i].used = 0;
}

/* ~ cuidado que essa aqui morde ~ */
static int alloc_fd(void) {
    for (int i = 3; i < MAX_FDS; i++) if (!fds[i].used) return i;
    return -1;
}

/* ~~ open_dev ~ "Abre um device file" ~~
 * /dev/fb0, /dev/ttyN e /dev/input/mice pro Xorg.
 * Retorna o fd (ja com O_NONBLOCK setado, que e como o Xorg abre)
 * ou -1 se o path nao for um device conhecido. */
static int str_equal(const char *a, const char *b);
static int open_dev(const char *path) {
    if (!path || !path[0]) return -1;
    int type = -1;
    if (str_equal(path, "/dev/fb0")) {
        type = FD_FB;
    } else if (str_equal(path, "/dev/input/mice")) {
        type = FD_MICE;
    } else if (path[0] == '/' && path[1] == 'd' && path[2] == 'e' && path[3] == 'v' &&
               path[4] == '/' && path[5] == 't' && path[6] == 't' && path[7] == 'y' &&
               path[8] >= '0' && path[8] <= '9' && path[9] == '\0') {
        type = FD_TTY;
    }
    if (type < 0) return -1;
    int fd = alloc_fd();
    if (fd < 0) return -1;
    int i = 0;
    while (path[i] && i < 255) { fds[fd].name[i] = path[i]; i++; }
    fds[fd].name[i] = '\0';
    fds[fd].type = type;
    fds[fd].flags = O_NONBLOCK;
    fds[fd].pos = 0;
    fds[fd].used = 1;
    return fd;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void close_fd(int fd) {
    if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
        if (fds[fd].type == 3) {
            int p = fds[fd].pipe_idx;
            if (p >= 0 && p < MAX_PIPES) pipes[p].used = 0;
        }
        if (fds[fd].type == 4) {
            int s = fds[fd].sock_idx;
            if (s >= 0 && s < MAX_SOCKS) socks[s].used = 0;
        }
        fds[fd].used = 0;
    }
}

/* ~~ do_readlink ~ resolve symlinks de /proc que o Xorg precisa ~~
 * /proc/self/exe → nome do processo; /proc/self/fd/N → nome do fd.
 * O driver fbdev usa isso pra detectar se /dev/fb0 e PCI ou nao~
 * Devolve bytes escritos (sem NUL, igual readlink real) ou -1. */
static int do_readlink(const char *path, char *buf, size_t bufsiz) {
    if (!path || !buf || bufsiz == 0) return -1;
    const char *target = NULL;
    if (str_equal(path, "/proc/self/exe")) {
        pcb_t *cur = process_current();
        if (cur) target = cur->name;
    } else if (strncmp(path, "/proc/self/fd/", 14) == 0) {
        int fd = 0;
        for (int i = 14; path[i] >= '0' && path[i] <= '9'; i++)
            fd = fd * 10 + (path[i] - '0');
        if (fd >= 0 && fd < MAX_FDS && fds[fd].used)
            target = fds[fd].name;
    } else if (strncmp(path, "/sys/class/graphics/fb", 22) == 0) {
        /* ~~ sysfs do fbdev ~ o driver do Xorg le /sys/class/graphics/fbN
         * pra ver se e device PCI/DRM (strstr(buf,"devices/pci")). Aqui
         * devolvemos "/dev/fbN" — sem "devices/pci" — pra ele tratar como
         * fbdev puro e seguir pro FBIOGET_FSCREENINFO~ */
        static char tmp[32];
        tmp[0] = '/'; tmp[1] = 'd'; tmp[2] = 'e'; tmp[3] = 'v'; tmp[4] = '/';
        tmp[5] = 'f'; tmp[6] = 'b';
        int n = 7, i = 22;
        while (path[i] >= '0' && path[i] <= '9' && n < 31) tmp[n++] = path[i++];
        tmp[n] = '\0';
        target = tmp;
    }
    if (!target) return -1;
    int i = 0;
    while (target[i] && i < (int)bufsiz - 1) { buf[i] = target[i]; i++; }
    buf[i] = '\0';
    return i;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int str_equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

int g_use_ext2 = 0;
extern int ext2_init(void);
extern int ext2_read_file(const char *path, unsigned char *buffer, unsigned int size);
extern int ext2_stat(const char *path, unsigned int *size);
extern int ext2_create_file(const char *path);
extern int ext2_write_at(const char *path, unsigned char *buffer, unsigned int size, unsigned int offset);
extern int elf64_find_interp(unsigned char *data, unsigned int len, char *out, unsigned int out_max);
extern void *elf64_load_at(unsigned char *data, unsigned int len, uint64_t pml4, uint64_t force_base);

int fs_read_file(const char *name, uint8_t *buf, uint32_t count) {
    if (g_use_ext2) return ext2_read_file(name, buf, count);
    return fat32_read_file(name, buf, count);
}
int vfs_stat_size(const char *name, uint32_t *size, uint8_t *attr) {
    if (g_use_ext2) {
        unsigned int sz = 0;
        if (ext2_stat(name, &sz) != 0) return -1;
        if (size) *size = sz;
        if (attr) *attr = 0;
        return 0;
    }
    return fat32_stat(name, size, attr, NULL, NULL);
}

/* ~~ do_write_fd ~ escreve num fd (eventfd/pipe/socket/arquivo/console) ~~
 * Devolve bytes escritos (ou -1 em erro). Compartilhado entre
 * SYS_write e SYS_writev pra nao duplicar a logica~ */
static int sock_push(int s, const uint8_t *buf, int count);
static int do_write_fd(int fd, const char *buf, int count) {
    if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 1) {
        /* eventfd write: soma u64 no contador */
        if (count >= 8) {
            uint64_t val = 0;
            for (int i = 0; i < 8; i++) val |= (uint64_t)(uint8_t)buf[i] << (8 * i);
            fds[fd].counter += val;
        }
        return count;
    }
    if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 3) {
        /* pipe write: copia pro buffer compartilhado */
        int p = fds[fd].pipe_idx;
        if (p < 0 || p >= MAX_PIPES || !pipes[p].used) return -1;
        if (pipes[p].wpos - pipes[p].rpos >= 4096) {
            if (fds[fd].flags & O_NONBLOCK) return -11;
            return -1;
        }
        int n = 0;
        while (n < count && pipes[p].wpos - pipes[p].rpos < 4096) {
            pipes[p].buf[pipes[p].wpos & 4095] = (uint8_t)buf[n++];
            pipes[p].wpos++;
        }
        return n;
    }
    if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 4) {
        /* socket write: empurra pro buffer do peer */
        int s = fds[fd].sock_idx;
        int peer = (s >= 0 && s < MAX_SOCKS) ? socks[s].peer : -1;
        return sock_push(peer, (const uint8_t *)buf, count);
    }
    if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
        if (g_use_ext2) {
            /* ~~ VFS: create-if-missing + write_at com offset do stdio~~ */
            unsigned int cursz = 0;
            if (vfs_stat_size_attr(fds[fd].name, &cursz, NULL) != 0 &&
                vfs_create_file(fds[fd].name) != 0) return -1;
            if (fds[fd].pos == 0) fds[fd].pos = cursz;
            if (vfs_write_at(fds[fd].name, (const uint8_t *)buf, count, fds[fd].pos) < 0)
                return -1;
            fds[fd].pos += count;
            return count;
        }
        int r = fat32_write_file(fds[fd].name, (const uint8_t *)buf, count);
        if (r < 0) {
            /* ~~ arquivo novo? cria e tenta de novo (O_CREAT!) ~~
             * O XKB escreve o keymap compilado (.xkm) em arquivo que
             * ainda não existe — sem isso o teclado não ativa~ */
            if (fat32_create_file(fds[fd].name) == 0)
                r = fat32_write_file(fds[fd].name, (const uint8_t *)buf, count);
        }
        {
            static char wseen[6][96]; static int wn=0;
            int dup=0;
            for (int q=0;q<wn;q++) if (str_equal(wseen[q],fds[fd].name)){dup=1;break;}
            if (!dup && wn<6){
                int q2=0; while (fds[fd].name[q2] && q2<95){wseen[wn][q2]=fds[fd].name[q2];q2++;}
                wseen[wn][q2]='\0'; wn++;
                serial_puts("[wr] "); serial_puts(fds[fd].name);
                serial_puts(" n="); serial_puthex((uint32_t)count);
                serial_puts(" r="); serial_puthex((uint32_t)r);
                serial_puts("\r\n");
            }
        }
        return (r >= 0) ? count : -1;
    }
    if (fd == 1 || fd == 2) {
        for (int i = 0; i < count; i++) console_putchar(buf[i]);
        return count;
    }
    return -1;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
/* ~~ resolve_at_path ~ pra syscalls *at ~~
 * Resolve um path a partir de um dirfd (ou AT_FDCWD = cwd atual).
 * O FAT32 nao entende paths com '/' — entao separa o diretorio
 * do nome, como o spawn faz, e deixa o cwd no dir certo.
 * Retorna 0 e grava o nome base em out (ate maxlen) ou -1.
 * O caller DEVE restaurar o cwd depois (fat32_set_cwd(saved)). */
/* ~~ fixpath ~ remap do prefixo de build do Xorg pra raiz do disco ~~
 * O Xorg foi compilado com prefixo ABSOLUTO do host: paths tipo
 * "/home/vinberkuko/tipos-musl/prefix/share/X11/xkb" chegam crus.
 * No FAT32 a gente serve isso da raiz: "/share/X11/xkb"~ */
#define XPREFIX "/home/vinberkuko/tipos-musl/prefix"
static const char *fixpath(const char *path, char *buf, int buflen) {
    if (!path) return path;
    int pl = sizeof(XPREFIX) - 1;
    int i = 0;
    while (XPREFIX[i] && path[i] == XPREFIX[i]) i++;
    if (i != pl) return path;                       /* sem prefixo ~ */
    if (path[i] != '\0' && path[i] != '/') return path;
    if (path[i] == '/') i++;                        /* come a barra dupla */
    int j = 0;
    buf[j++] = '/';
    while (path[i] && j < buflen - 1) buf[j++] = path[i++];
    buf[j] = '\0';
    return buf;
}

static int resolve_at_path(int dirfd, const char *path, char *out, int maxlen) {
    if (!path || !path[0]) return -1;
    char pbuf[256];
    path = fixpath(path, pbuf, sizeof(pbuf));
    if (dirfd != AT_FDCWD) return -1;  /* dirfd != cwd ainda nao suportado */
    char _p[256]; int _i = 0;
    while (path[_i] && _i < 255) { _p[_i] = path[_i]; _i++; }
    _p[_i] = '\0';
    char *_base = _p, *_last_slash = 0, *_s = _p;
    if (*_base == '/') { fat32_change_dir("/"); _base++; _s = _base; }
    while (*_s) { if (*_s == '/') _last_slash = _s; _s++; }
    char *_file = _base;
    if (_last_slash) {
        *_last_slash = '\0';
        _file = _last_slash + 1;
        char *_w = _base;
        while (_w && *_w) {
            char *_n = _w;
            while (*_n && *_n != '/') _n++;
            int _end = (*_n == '\0');
            if (*_n) *_n = '\0';
            if (_w[0] && fat32_change_dir(_w) < 0) return -1;
            if (_end) break;
            _w = _n + 1;
        }
    }
    if (!_file[0]) return -1;
    int _j = 0;
    while (_file[_j] && _j < maxlen - 1) { out[_j] = _file[_j]; _j++; }
    out[_j] = '\0';
    return 0;
}

/* ~~ socket helpers (issue #49) ~~ kyun~ */
static int sock_alloc(void) {
    for (int i = 0; i < MAX_SOCKS; i++) if (!socks[i].used) return i;
    return -1;
}

static int sock_by_path(const char *path) {
    if (!path) return -1;
    for (int i = 0; i < MAX_SOCKS; i++)
        if (socks[i].used && socks[i].listening && str_equal(socks[i].path, path))
            return i;
    return -1;
}

/* devolve o endereço de destino (sun_path) do sockaddr_un */
static const char *sock_path_from_addr(uint64_t addr, uint64_t len) {
    if (!addr || len < 2) return NULL;
    /* sockaddr_un: sa_family (2 bytes) + sun_path (cstring) */
    return (const char *)(uintptr_t)addr + 2;
}

/* escreve count bytes no buffer do socket s (chamado pelo peer) */
static int sock_push(int s, const uint8_t *buf, int count) {
    if (s < 0 || s >= MAX_SOCKS || !socks[s].used) return -1;
    if (socks[s].wpos - socks[s].rpos >= 4096) {
        if (socks[s].nonblock) return -11; /* -EAGAIN */
        return -1;
    }
    int n = 0;
    while (n < count && socks[s].wpos - socks[s].rpos < 4096) {
        socks[s].buf[socks[s].wpos & 4095] = buf[n++];
        socks[s].wpos++;
    }
    return n;
}

/* lê count bytes do buffer do socket s */
static int sock_pop(int s, uint8_t *buf, int count) {
    if (s < 0 || s >= MAX_SOCKS || !socks[s].used) return -1;
    if (socks[s].rpos == socks[s].wpos) {
        if (socks[s].nonblock) return -11; /* -EAGAIN */
        return 0;
    }
    int n = 0;
    while (n < count && socks[s].rpos != socks[s].wpos) {
        buf[n++] = socks[s].buf[socks[s].rpos & 4095];
        socks[s].rpos++;
    }
    return n;
}

/* 1 se o fd tem dados pra ler (socket com buffer cheio ou listener com pending) */
static int fd_readable(int fd) {
    if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == FD_TTY)
        return kbd_raw_avail();
    if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == FD_MICE)
        return mice_avail();
    if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 4) {
        int s = fds[fd].sock_idx;
        if (s >= 0 && s < MAX_SOCKS)
            return (socks[s].rpos != socks[s].wpos || socks[s].npending > 0);
    }
    return 0;
}

/* ~~ syscall_trace ~~
 * Rastreia syscalls Linux unicos (bitmap). Imprime so a primeira
 * ocorrencia de cada numero no serial — debug do Xorg no QEMU.
 * Chamado do dispatcher Zig antes da traducao. */
#define TRACE_BITS 1024
static uint8_t trace_seen[TRACE_BITS / 8];
static int q0_len(const char *t) { int n=0; while (t[n]) n++; return n; }
void syscall_trace(uint64_t num, uint64_t rip) {
    if (num >= TRACE_BITS) return;
    /* ~~ FS-calls: loga SEMPRE (bitmap come no first-shot do boot) ~~ */
    if (num == 83 || num == 87 || num == 258 || num == 262 || num == 263 ||
        num == 266 || num == 332 || num == 257 || num == 2 || num == 6) {
        serial_puts("[fs");
        serial_puthex((uint32_t)num);
        serial_puts(" rip=");
        serial_puthex((uint32_t)rip);
        serial_puts("]\r\n");
        return;
    }
    if (trace_seen[num / 8] & (1 << (num % 8))) return;
    trace_seen[num / 8] |= (1 << (num % 8));
    serial_puts("[SYS ");
    serial_puthex((uint32_t)num);
    serial_puts(" RIP=");
    serial_puthex((uint32_t)rip);
    serial_puts("]\n");
}

void syscall_handler(uint64_t *regs) {
    uint64_t num  = regs[0];
    uint64_t a1 = regs[4];
    uint64_t a2 = regs[3];
    uint64_t a3 = regs[2];
    uint64_t a4 = regs[1];
    uint64_t ret = -1;

    switch (num) {
    case SYS_exit:
        process_exit_current((int)a1);
        for (;;) __asm__ volatile("hlt");
        break;

    case SYS_write: {
        int fd = (int)a1;
        const char *buf = (const char *)a2;
        int count = (int)a3;
        ret = do_write_fd(fd, buf, count);
        break;
    }

    case SYS_writev: {
        /* ~~ writev ~ o musl usa pra escrever no log (2 iovecs: buffer
         * velho + novo). Antes o writev caia no identity mapping do Zig
         * e virava getpid (20=20)! O __stdio_write do musl recebia o PID
         * como "bytes escritos" e ficava num loop infinito~ rssrsrs */
        int fd = (int)a1;
        const struct iovec *iov = (const struct iovec *)a2;
        int iovcnt = (int)a3;
        {
            static int wvn=0;
            if (wvn<14){
                wvn++;
                serial_puts("[wv] fd="); serial_puthex((uint32_t)fd);
                serial_puts(" n="); serial_puthex((uint32_t)iovcnt);
                if (fd>=3 && fd<MAX_FDS && fds[fd].used){ serial_puts(" name="); serial_puts(fds[fd].name); }
                serial_puts("\r\n");
            }
        }
        if (!iov || iovcnt <= 0) { ret = 0; break; }
        uint64_t total = 0;
        int err = 0;
        for (int i = 0; i < iovcnt; i++) {
            int r = do_write_fd(fd, (const char *)iov[i].iov_base, (int)iov[i].iov_len);
            if (r < 0) { err = 1; break; }
            total += (uint64_t)r;
        }
        ret = (err && total == 0) ? (uint64_t)-1 : total;
        break;
    }

    case SYS_read: {
        int fd = (int)a1;
        char *buf = (char *)a2;
        int count = (int)a3;
        if (fd == 0 && buf && count > 0) {
            /* ~~ O_NONBLOCK no stdin ~ weston configura nonblock e
             * espera EAGAIN em vez de travar a thread~ */
            if ((fds[0].flags & O_NONBLOCK) && !keyboard_avail()) {
                ret = (uint64_t)-11; /* -EAGAIN */
                break;
            }
            int i;
            for (i = 0; i < count; i++) {
                while (!keyboard_avail())
                    for (volatile int j = 0; j < 100; j++);
                buf[i] = keyboard_read();
                if (buf[i] == '\n') { i++; break; }
                if (buf[i] == 3) break;
            }
            ret = i;
            break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == FD_TTY) {
            /* ~~ /dev/ttyN read ~ scan codes crus pro driver kbd do Xorg ~~
             * O Xorg abre O_NONBLOCK e espera via select(); se nao tem
             * dado e nao eh nonblock, espera (spin com pause~) */
            int n = 0;
            while (n < count && kbd_raw_avail()) buf[n++] = (char)kbd_raw_read();
            if (n > 0) { ret = n; break; }
            if (fds[fd].flags & O_NONBLOCK) { ret = (uint64_t)-11; break; } /* -EAGAIN */
            while (!kbd_raw_avail())
                for (volatile int j = 0; j < 100; j++);
            buf[0] = (char)kbd_raw_read();
            ret = 1;
            break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == FD_MICE) {
            /* ~~ /dev/input/mice read ~ bytes crus do PS/2 ~~
             * O driver mouse do Xorg decodifica o protocolo sozinho~ */
            int n = 0;
            while (n < count && mice_avail()) buf[n++] = (char)mice_read();
            if (n > 0) { ret = n; break; }
            if (fds[fd].flags & O_NONBLOCK) { ret = (uint64_t)-11; break; } /* -EAGAIN */
            while (!mice_avail())
                for (volatile int j = 0; j < 100; j++);
            buf[0] = (char)mice_read();
            ret = 1;
            break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == FD_FB) {
            /* ~~ /dev/fb0 read ~ o Xorg nao le, so mmap+ioctl~~ */
            ret = -1;
            break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 3) {
            /* pipe read: copia do buffer, respeita O_NONBLOCK */
            int p = fds[fd].pipe_idx;
            if (p < 0 || p >= MAX_PIPES || !pipes[p].used) { ret = -1; break; }
            if (pipes[p].rpos == pipes[p].wpos) {
                if (fds[fd].flags & O_NONBLOCK) ret = (uint64_t)-11;
                else ret = 0;
                break;
            }
            int n = 0;
            while (n < count && pipes[p].rpos != pipes[p].wpos) {
                buf[n++] = pipes[p].buf[pipes[p].rpos & 4095];
                pipes[p].rpos++;
            }
            ret = n;
            break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 1) {
            /* eventfd read: devolve contador e zera */
            if (count >= 8) {
                uint64_t v = fds[fd].counter;
                fds[fd].counter = 0;
                for (int i = 0; i < 8; i++) buf[i] = (char)(v >> (8 * i));
            }
            ret = count;
            break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 2) {
            /* timerfd read: "expirou" — retorna 1 (contagem de expirações) */
            if (count >= 8) {
                uint64_t one = 1;
                for (int i = 0; i < 8; i++) buf[i] = (char)(one >> (8 * i));
            }
            ret = count;
            break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 4) {
            /* socket read: lê do próprio buffer */
            int s = fds[fd].sock_idx;
            ret = sock_pop(s, (uint8_t *)buf, count);
            break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
            /* ~~ VFS: leitura com offset REAL (sem buffer estático)~~
             * O ext2 lê direto do disco pro buffer do usuário usando
             * os blocos do inode — suporta arquivos de qualquer tamanho~ */
            int r = vfs_read_file(fds[fd].name, (uint8_t *)buf, count);
            if (r < 0) {
                serial_puts("[rdFAIL] ");
                serial_puts(fds[fd].name);
                serial_puts("\r\n");
                ret = -1; break;
            }
            fds[fd].pos += r;
            ret = r;
            break;
        }
        ret = -1;
        break;
    }

    case SYS_open: {
        const char *path = (const char *)a1;
        char fpbuf[512];
        path = fixpath(path, fpbuf, sizeof(fpbuf));
        if (str_equal(path, "/dev/tty") || str_equal(path, "/dev/stdin")) { ret = 0; break; }
        if (str_equal(path, "/dev/stdout")) { ret = 1; break; }
        if (str_equal(path, "/dev/stderr")) { ret = 2; break; }
        {
            int devfd = open_dev(path);
            if (devfd >= 0) { ret = devfd; break; }
        }
        int fd = alloc_fd();
        if (fd < 0) { ret = -1; break; }
        int i = 0;
        while (path[i] && i < 255) { fds[fd].name[i] = path[i]; i++; }
        fds[fd].name[i] = '\0';
        fds[fd].pos = 0;
        fds[fd].used = 1;
        {
            static char seen_names[48][96];
            static int seen_n = 0;
            int dup = 0;
            for (int q = 0; q < seen_n; q++)
                if (str_equal(seen_names[q], path)) { dup = 1; break; }
            if (!dup && seen_n < 48) {
                for (int q = 0; path[q] && q < 95; q++) seen_names[seen_n][q] = path[q];
                seen_names[seen_n][q0_len(path)] = '\0';
                seen_n++;
                serial_puts("[open] ");
                serial_puts(path);
                serial_puts("\r\n");
            }
        }
        ret = fd;
        break;
    }

    case SYS_close:
        close_fd((int)a1);
        ret = 0;
        break;

    case SYS_access: {
        const char *path = (const char *)a1;
        if (str_equal(path, "/dev/tty") || str_equal(path, "/dev/stdin") ||
            str_equal(path, "/dev/stdout") || str_equal(path, "/dev/stderr"))
            ret = 0;
        else
            ret = 0;
        break;
    }

    case SYS_fstat: {
        struct stat *st = (struct stat *)a2;
        if (!st) { ret = -1; break; }
        int fd = (int)a1;
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type >= FD_FB && fds[fd].type <= FD_MICE) {
            st->st_size = 0; ret = 0; break;
        }
        /* ~~ /tmp virtual ~ a checagem final do Xtrans abre o diretorio
         * e faz fstat no fd — sem isso ele desiste do listener unix~~ */
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used &&
            (str_equal(fds[fd].name, "/tmp/.X11-unix") || str_equal(fds[fd].name, "/tmp"))) {
            *(unsigned int *)((uint8_t *)st + 24) = 040755;  /* S_IFDIR|0755 */
            *(uint64_t *)((uint8_t *)st + 48) = 4096;
            ret = 0; break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
            uint32_t size; uint8_t attr;
            if (vfs_stat_size(fds[fd].name, &size, &attr) == 0) {
                st->st_size = size; ret = 0; break;
            }
        }
        if (fd >= 0 && fd <= 2) { st->st_size = 0; ret = 0; break; }
        ret = -1;
        break;
    }

    case SYS_lstat:
    case SYS_stat: {
        const char *path = (const char *)a1;
        struct stat *st = (struct stat *)a2;
        char fpbuf[256];
        if (path) path = fixpath(path, fpbuf, sizeof(fpbuf));
        if (!path || !st) { ret = -1; break; }
        /* ~~ /tmp virtual ~ o Xorg usa o lstat ANTIGO (6→199) e sem
         * isso ele acha que o dir do socket não existe e nem tenta
         * o mkdir~ (mesma struct stat do musl: mode@24 size@48)~~ */
        if (str_equal(path, "/tmp/.X11-unix") || str_equal(path, "/tmp")) {
            *(unsigned int *)((uint8_t *)st + 24) = 040755;  /* S_IFDIR|0755 */
            *(uint64_t *)((uint8_t *)st + 48) = 4096;
            ret = 0; break;
        }
        uint32_t size = 0; uint8_t attr = 0;
        int stok = 0;
        {
            pcb_t *curp = process_current();
            char absp[VFS_MAX_PATH];
            if (vfs_abs_path(curp ? curp->cwd : "/", path, absp, sizeof(absp)) == 0 &&
                vfs_stat_size_attr(absp, &size, &attr) == 0)
                stok = 1;
        }
        if (stok) {
            st->st_size = size; ret = 0; break;
        }
        ret = -1;
        break;
    }

    case SYS_lseek: {
        int fd = (int)a1;
        int off = (int)a2;
        int whence = (int)a3;
        if (fd < 3 || fd >= MAX_FDS || !fds[fd].used) { ret = -1; break; }
        switch (whence) {
            case 0: fds[fd].pos = off; ret = fds[fd].pos; break;
            case 1: fds[fd].pos += off; ret = fds[fd].pos; break;
            case 2: {
                uint32_t size = 0;
                uint8_t attr;
                if (vfs_stat_size(fds[fd].name, &size, &attr) == 0)
                    fds[fd].pos = size + off;
                ret = fds[fd].pos;
                break;
            }
            default: ret = -1; break;
        }
        break;
    }

    case SYS_unlink:
        if (fat32_delete_file((const char *)a1) == 0) ret = 0;
        break;

    case SYS_mkdir2:
        /* ~~ /tmp virtual ~ o musl usa o mkdir antigo (83→136), e o
         * Xorg cria o diretorio do socket unix nele~ */
        { const char *mp=(const char *)a1;
          if (mp && mp[0]=='/' && mp[1]=='t') {
              serial_puts("[mkdir2] "); serial_puts(mp); serial_puts("\r\n"); } }
        if (str_equal((const char *)a1, "/tmp/.X11-unix") ||
            str_equal((const char *)a1, "/tmp")) { ret = 0; break; }
        if (fat32_mkdir((const char *)a1) == 0) ret = 0;
        break;

    case SYS_rmdir2:
        if (fat32_rmdir((const char *)a1) == 0) ret = 0;
        break;

    /* ~~ *at syscalls (issue #53) ~~
     * Dirfd relativo: suportamos AT_FDCWD (o cwd atual do processo).
     * Como o FAT32 nao tem fd de diretorio de verdade, dirfd != AT_FDCWD
     * cai no helper que retorna -1. O padrão é igual ao spawn: salva
     * cwd, resolve o path, opera, restaura. kyun~ */
    case SYS_openat: {
        int dirfd = (int)a1;
        char fpbuf2[VFS_MAX_PATH];
        const char *path = (const char *)a2;
        path = fixpath(path, fpbuf2, sizeof(fpbuf2));
        {
            int devfd = open_dev(path);
            if (devfd >= 0) { ret = devfd; break; }
        }
        /* ~~ VFS #70: caminho absoluto via cwd POR PROCESSO~~ */
        pcb_t *curp = process_current();
        char abspath[VFS_MAX_PATH];
        if (vfs_abs_path(curp ? curp->cwd : "/", path, abspath, sizeof(abspath)) != 0) { ret = -1; break; }

        char base[VFS_MAX_PATH > 255 ? 255 : VFS_MAX_PATH];
        if (!g_use_ext2) {
            uint32_t saved = fat32_get_cwd();
            if (resolve_at_path(dirfd, abspath, base, sizeof(base)) < 0) {
                /* ~~ ENOENT de verdade: fd cego enganava o loader do Xorg~~ */
                fat32_set_cwd(saved); ret = -1; break;
            }
            fat32_set_cwd(saved);
        } else {
            int bi = 0;
            while (abspath[bi] && bi < 254) { base[bi] = abspath[bi]; bi++; }
            base[bi] = '\0';
        }
        int fd = alloc_fd();
        if (fd < 0) { ret = -1; break; }
        int i = 0;
        while (base[i] && i < 255) { fds[fd].name[i] = base[i]; i++; }
        fds[fd].name[i] = '\0';
        fds[fd].type = 0;
        fds[fd].pos = 0;
        fds[fd].used = 1;
        ret = fd;
        break;
    }

    case SYS_mkdirat: {
        int dirfd = (int)a1;
        const char *path = (const char *)a2;
        /* ~~ /tmp virtual ~ o socket unix do Xorg mora em /tmp/.X11-unix,
         * mas o FAT32 n entende nome com ponto no inicio (".X11-unix"
         * vira "X11" no name_to_83~). Como o bind so guarda o path como
         * string em socks[].path, o diretorio pode ser puramente
         * virtual — aceita o mkdir e segue o baile~ kyun~ */
        if (path && path[0]=='/' && path[1]=='t') {
            serial_puts("[mkdirat] "); serial_puts(path); serial_puts("\r\n");
        }
        if (str_equal(path, "/tmp/.X11-unix") || str_equal(path, "/tmp")) {
            ret = 0; break;
        }
        uint32_t saved = fat32_get_cwd();
        char base[256];
        if (resolve_at_path(dirfd, path, base, sizeof(base)) < 0) {
            fat32_set_cwd(saved); ret = -1; break;
        }
        if (fat32_mkdir(base) == 0) ret = 0;
        fat32_set_cwd(saved);
        break;
    }

    case SYS_newfstatat: {
        int dirfd = (int)a1;
        const char *path = (const char *)a2;
        struct stat *st = (struct stat *)a3;
        /* ~~ stat do /tmp virtual ~ o Xtrans confere se o diretorio
         * existe e e um diretorio de verdade antes do bind~~
         * (a struct stat do musl tem st_mode em +24 e st_size em +48) */
        if (path && path[0]=='/' && path[1]=='t') {
            serial_puts("[fstatat] "); serial_puts(path); serial_puts("\r\n");
        }
        if (path && st && (str_equal(path, "/tmp/.X11-unix") || str_equal(path, "/tmp"))) {
            *(unsigned int *)((uint8_t *)st + 24) = 040755;  /* S_IFDIR|0755 */
            *(uint64_t *)((uint8_t *)st + 48) = 4096;        /* st_size */
            ret = 0; break;
        }
        if (g_use_ext2) {
            unsigned int sz = 0;
            char fpbuf[256];
            path = fixpath(path, fpbuf, sizeof(fpbuf));
            if (!path || !st || ext2_stat(path, &sz) != 0) { ret = -1; break; }
            *(unsigned int *)((uint8_t *)st + 24) = 0100644;
            *(uint64_t *)((uint8_t *)st + 48) = sz;
            ret = 0; break;
        }
        uint32_t saved = fat32_get_cwd();
        char base[256];
        if (g_use_ext2) {
            unsigned int sz = 0;
            char fpb[512];
            const char *fx = fixpath(path, fpb, sizeof(fpb));
            pcb_t *curp2 = process_current();
            char absp[VFS_MAX_PATH];
            if (!st || vfs_abs_path(curp2 ? curp2->cwd : "/", fx, absp, sizeof(absp)) != 0 ||
                ext2_stat(absp, &sz) != 0) {
                fat32_set_cwd(saved); ret = -1; break;
            }
            *(unsigned int *)((uint8_t *)st + 24) = 0100644;
            *(uint64_t *)((uint8_t *)st + 48) = sz;
            fat32_set_cwd(saved); ret = 0; break;
        }
        if (!st || resolve_at_path(dirfd, path, base, sizeof(base)) < 0) {
            fat32_set_cwd(saved); ret = -1; break;
        }
        uint32_t size; uint8_t attr;
        if (fat32_stat(base, &size, &attr, NULL, NULL) == 0) {
            st->st_size = size; ret = 0;
        } else ret = -1;
        fat32_set_cwd(saved);
        break;
    }

    case SYS_unlinkat: {
        int dirfd = (int)a1;
        const char *path = (const char *)a2;
        uint32_t saved = fat32_get_cwd();
        char base[256];
        if (resolve_at_path(dirfd, path, base, sizeof(base)) < 0) {
            fat32_set_cwd(saved); ret = -1; break;
        }
        if (fat32_delete_file(base) == 0) ret = 0;
        fat32_set_cwd(saved);
        break;
    }

    case SYS_renameat: {
        int dirfd = (int)a1;
        const char *path = (const char *)a2;
        int newdirfd = (int)a3;
        const char *newpath = (const char *)a4;
        uint32_t saved = fat32_get_cwd();
        char base[256], newbase[256];
        if (resolve_at_path(dirfd, path, base, sizeof(base)) < 0) {
            fat32_set_cwd(saved); ret = -1; break;
        }
        uint32_t mid = fat32_get_cwd();
        fat32_set_cwd(saved);
        if (resolve_at_path(newdirfd, newpath, newbase, sizeof(newbase)) < 0) {
            fat32_set_cwd(saved); ret = -1; break;
        }
        uint32_t newdir = fat32_get_cwd();
        if (newdir != mid) {
            fat32_set_cwd(saved); ret = -1; break;
        }
        if (fat32_rename(base, newbase) == 0) ret = 0;
        fat32_set_cwd(saved);
        break;
    }

    case SYS_readlinkat: {
        const char *path = (const char *)a2;
        char *buf = (char *)a3;
        size_t bufsiz = (size_t)a4;
        ret = do_readlink(path, buf, bufsiz);
        break;
    }

    case SYS_readlink: {
        /* ~~ readlink (89) ~ musl usa o syscall antigo (nao o *at!) ~~
         * O driver fbdev do Xorg faz readlink("/proc/self/fd/N") pra
         * ver se /dev/fb0 e um device PCI — se der ENOSYS ele desiste
         * e o servidor morre com "no screens found"~ rssrsrs */
        const char *path = (const char *)a1;
        char *buf = (char *)a2;
        size_t bufsiz = (size_t)a3;
        ret = do_readlink(path, buf, bufsiz);
        break;
    }

    case SYS_chmod:
    case SYS_fchmod:
        ret = 0;  /* no-op realista — FAT32 sem permissões */
        break;

    case SYS_chown:
    case SYS_fchown:
        ret = 0;  /* no-op realista — FAT32 sem dono */
        break;

    case SYS_statfs:
    case SYS_fstatfs: {
        /* struct statfs { f_type, f_bsize, f_blocks, f_bfree, ... } */
        uint64_t *sf = (uint64_t *)a2;
        if (!sf) { ret = -1; break; }
        uint32_t total = 131072;   /* 64MB / 512B = 131072 setores */
        uint32_t bsize = 512;
        uint32_t blocks = total / 1;
        sf[0] = 0x4d44;            /* f_type: MSDOS_SUPER_MAGIC */
        sf[1] = bsize;             /* f_bsize */
        sf[2] = blocks;            /* f_blocks */
        sf[3] = blocks - 8192;     /* f_bfree (com uma folguinha~) */
        sf[4] = blocks - 8192;     /* f_bavail */
        sf[5] = 0;                 /* f_files */
        sf[6] = 0;                 /* f_ffree */
        ret = 0;
        break;
    }

    case SYS_mmap: {
        uint64_t hint = a1;
        uint64_t len = a2;
        int prot = (int)a3;
        int flags = (int)a4;
        int mfd = (int)regs[5]; /* Linux mmap: 5o arg = fd (r8) */
        /* ~~ /dev/fb0: mapeia o framebuffer no processo ~~
         * O driver fbdev do Xorg faz mmap(..., MAP_SHARED, fd, 0).
         * Mapeamos o LFB identity (mesma ideia do vesa_init), com U/S
         * pra userland acessar. Retorna o proprio endereco do LFB. */
        if (!(flags & 0x20) && mfd >= 3 && mfd < MAX_FDS && fds[mfd].used && fds[mfd].type == FD_FB) {
            extern framebuffer_t g_fb;
            pcb_t *cur = process_current();
            if (!cur) { ret = (uint64_t)-1; break; }
            uint64_t fb_size = (uint64_t)g_fb.pitch * g_fb.height;
            fb_size = (fb_size + 0x1FFFFF) & ~0x1FFFFFULL;
            int r = pml4_map_phys(cur->pml4, g_fb.addr, g_fb.addr, fb_size, 1);
            if (r < 0) { ret = (uint64_t)-1; break; }
            serial_puts("mmap: fb mapped at ");
            serial_puthex((uint32_t)g_fb.addr);
            serial_puts("\r\n");
            ret = g_fb.addr;
            break;
        }
        serial_puts("mmap: len="); serial_puthex((uint32_t)len);
        serial_puts(" prot="); serial_puthex((uint32_t)prot);
        serial_puts(" flags="); serial_puthex((uint32_t)flags);
        serial_puts("\r\n");
        uint64_t addr = hint;
        if (len < 0x1000) len = 0x1000;
        if (!(flags & MAP_ANON)) {
            flags |= MAP_ANON;
        }
        int r = vm_mmap(&addr, len, prot, flags);
        if (r < 0) {
            serial_puts("mmap: falhou!\r\n");
            ret = (uint64_t)-1;
        } else {
            serial_puts("mmap->va="); serial_puthex((uint32_t)addr); serial_puts("\r\n");
            ret = addr;
        }
        break;
    }

    case SYS_munmap:
        ret = vm_munmap(a1, a2);
        break;

    case SYS_mprotect:
        ret = vm_mprotect(a1, a2, (int)a3);
        break;

    case SYS_madvise:
        ret = 0;  /* no-op documentado — paginas sao wired de uma vez */
        break;

    case SYS_msync:
        ret = 0;  /* no-op — sem cache de paginas */
        break;

    case SYS_mremap:
        ret = (uint64_t)-1;  /* ainda nao suportado — musl cai pro malloc */
        break;

    case SYS_kbhit:
        ret = keyboard_avail();
        break;

    case SYS_getpid:
        ret = process_current_pid();
        break;

    case SYS_getuid:
    case SYS_geteuid:
    case SYS_getgid:
    case SYS_getegid:
        ret = 0;
        break;

    case SYS_ioctl: {
        /* ~~ ioctl ~ "O terminal tem quantas linhas?" ~~
         * Linux ioctl codes que o st precisa:
         * TIOCGWINSZ (0x5413) → devolve struct winsize { ws_row, ws_col, ws_xpixel, ws_ypixel }
         * TCGETS (0x5401) → devolve struct termios (modo cooked básico)
         * TCSETSW (0x5403) / TCSANOW (0x5402) → só aceita e ignora (modo raw, confia~)
         * Se não conhece o código, retorna 0 (o programa tenta de novo~) */
        int fd = (int)a1;
        unsigned long req = (unsigned long)a2;
        void *argp = (void *)a3;
        (void)fd;
        /* ~~ Linux VT/console ioctls (pro Xorg) ~~
         * O Xorg abre /dev/tty0 e /dev/ttyN e espera a maquina VT do
         * Linux. Aqui aceitamos tudo e devolvemos valores plausiveis
         * (nao temos VT de verdade no TipOS~) */
        if (req == 0x5604) { /* VT_OPENQRY -> devolve um VT livre */
            int *v = (int *)argp;
            if (v) *v = 1;  /* tty1, sempre livre~ */
            ret = 0; break;
        }
        if (req == 0x5603) { /* VT_GETSTATE -> struct vt_stat {v_active,v_signal,v_state; int v_mode} */
            unsigned short *vs = (unsigned short *)argp;
            if (vs) { vs[0] = 1; vs[1] = 0; vs[2] = 0; }
            ret = 0; break;
        }
        if (req == 0x5601) { /* VT_GETMODE -> struct vt_mode {char mode,waitv,relsig,acqsig,frsig; int frsig_pending} */
            unsigned char *vm = (unsigned char *)argp;
            if (vm) { vm[0] = 0; vm[1] = 0; vm[2] = 0; vm[3] = 0; vm[4] = 0; }
            ret = 0; break;
        }
        if (req == 0x5602 || req == 0x5606 || req == 0x5607) { /* VT_SETMODE / VT_ACTIVATE / VT_WAITACTIVE */
            ret = 0; break;
        }
        if (req == 0x4B3A || req == 0x4B3B) { /* KDSETMODE / KDGETMODE */
            if (req == 0x4B3B) { int *m = (int *)argp; if (m) *m = 0; } /* KD_TEXT */
            ret = 0; break;
        }
        if (req == 0x4B44) { /* KDGKBMODE -> devolve K_RAW (1) */
            int *m = (int *)argp;
            if (m) *m = 1;
            ret = 0; break;
        }
        if (req == 0x4B45) { /* KDSKBMODE -> aceita qualquer modo */
            ret = 0; break;
        }
        if (req == 0x4B31 || req == 0x4B32) { /* KDGETLED / KDSETLED */
            if (req == 0x4B31) { int *l = (int *)argp; if (l) *l = 0; }
            ret = 0; break;
        }
        if (req == 0x5410 || req == 0x540F) { /* TIOCSPGRP / TIOCGPGRP */
            ret = 0; break;
        }
        if (req == 0x4600) { /* FBIOGET_VSCREENINFO (pro driver fbdev do Xorg) */
            extern framebuffer_t g_fb;
            uint32_t *v = (uint32_t *)argp;
            if (!v) { ret = -1; break; }
            uint32_t W = g_fb.width, H = g_fb.height;
            /* ~~ Timings estilo VESA 1024x768@60 escalados pela resolucao ~~
             * Antes voltava tudo zerado: o modo "current" do driver fbdev
             * falhava na validacao do xf86 e o Xorg dava free() no nome
             * estatico dele ("current" em .rodata!) — crash no malloc~
             * Com timing de verdade o modo valida e ninguem e liberado~ */
            uint32_t lm = W * 24 / 1024, rm = W * 160 / 1024,
                     hs = W * 136 / 1024, um = H * 3 / 768,
                     bmm = H * 29 / 768, vs = H * 6 / 768;
            if (!lm) lm = 24; if (!rm) rm = 160; if (!hs) hs = 136;
            if (!um) um = 3; if (!bmm) bmm = 29; if (!vs) vs = 6;
            uint32_t htotal = W + lm + rm + hs;
            uint32_t vtotal = H + um + bmm + vs;
            uint64_t pixclk = 1000000000000ULL / (60ULL * htotal * vtotal);
            v[0]  = W;                       /* xres */
            v[1]  = H;                       /* yres */
            v[2]  = W;                       /* xres_virtual */
            v[3]  = H;                       /* yres_virtual */
            v[4]  = 0;                       /* xoffset */
            v[5]  = 0;                       /* yoffset */
            v[6]  = 16;                      /* bits_per_pixel — RGB565 evita o shadow framebuffer forçado do driver em 24bpp~ */
            v[7]  = 0;                       /* grayscale */
            /* fb_bitfield {offset, length, msb_right} — RGB565 */
            v[8]  = 11; v[9]  = 5;  v[10] = 0;  /* red */
            v[11] = 5;  v[12] = 6;  v[13] = 0;  /* green */
            v[14] = 0;  v[15] = 5;  v[16] = 0;  /* blue */
            v[17] = 0;  v[18] = 0;  v[19] = 0;  /* transp */
            v[20] = 0;                       /* nonstd */
            v[21] = 0;                       /* activate */
            v[22] = W * 304 / 1024;          /* height (mm) */
            v[23] = H * 228 / 768;           /* width (mm) */
            v[24] = 0;                       /* accel_flags */
            v[25] = (uint32_t)pixclk;        /* pixclock (ps) */
            v[26] = lm; v[27] = rm;          /* left/right margin */
            v[28] = um; v[29] = bmm;         /* upper/lower margin */
            v[30] = hs; v[31] = vs;          /* hsync/vsync len */
            v[32] = 0; v[33] = 0;            /* sync, vmode=NONINTERLACED */
            v[34] = 0;                       /* rotate */
            v[35] = 0;                       /* colorspace */
            ret = 0; break;
        }
        if (req == 0x4601) { /* FBIOPUT_VSCREENINFO -> aceita e ignora */
            ret = 0; break;
        }
        if (req == 0x4602) { /* FBIOGET_FSCREENINFO (pro driver fbdev do Xorg) */
            extern framebuffer_t g_fb;
            uint8_t *f = (uint8_t *)argp;
            if (!f) { ret = -1; break; }
            for (int i = 0; i < 16; i++) f[i] = 0;
            const char *id = "TipOS fb";
            for (int i = 0; i < 16 && id[i]; i++) f[i] = (uint8_t)id[i];
            *(uint64_t *)(f + 16) = g_fb.addr;        /* smem_start */
            *(uint32_t *)(f + 24) = g_fb.pitch * g_fb.height; /* smem_len */
            *(uint32_t *)(f + 28) = 0;                /* type = FB_TYPE_PACKED_PIXELS */
            *(uint32_t *)(f + 32) = 0;                /* type_aux */
            *(uint32_t *)(f + 36) = 2;                /* visual = FB_VISUAL_TRUECOLOR */
            *(uint16_t *)(f + 40) = 0;                /* xpanstep */
            *(uint16_t *)(f + 42) = 0;                /* ypanstep */
            *(uint16_t *)(f + 44) = 0;                /* ywrapstep */
            *(uint32_t *)(f + 46) = g_fb.pitch;       /* line_length */
            *(uint64_t *)(f + 48) = 0;                /* mmio_start */
            *(uint32_t *)(f + 56) = 0;                /* mmio_len */
            *(uint32_t *)(f + 60) = 0;                /* accel */
            *(uint16_t *)(f + 64) = 0;                /* capabilities */
            ret = 0; break;
        }
        if (req == 0x5413) { /* TIOCGWINSZ */
            /* struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; } */
            unsigned short *ws = (unsigned short *)argp;
            if (ws) {
                ws[0] = 45; /* ws_row — 45 linhas ~ cabe mais que VGA! */
                ws[1] = 80; /* ws_col — 80 colunas, padrao~ */
                ws[2] = 640; /* ws_xpixel */
                ws[3] = 720; /* ws_ypixel */
            }
            ret = 0; break;
        }
        if (req == 0x5401) { /* TCGETS */
            /* struct termios ~ so bits de modo cooked ~ */
            unsigned int *t = (unsigned int *)argp;
            if (t) {
                t[0] = 0x000005FD; /* c_iflag  ~ BRKINT|ICRNL|IXON */
                t[1] = 0x00000000; /* c_oflag  ~ 0 */
                t[2] = 0x00000000; /* c_cflag  ~ 0 */
                t[3] = 0x00000000; /* c_lflag  ~ 0 */
                t[4] = 0;          /* c_cc[0]  ~ VINTR = ^C */
                t[5] = 0;          /* c_cc[1]  ~ VQUIT */
                t[6] = 0;          /* c_cc[2]  ~ VERASE */
                t[7] = 0x7F;       /* c_cc[3]  ~ VKILL */
                t[8] = 0;          /* c_cc[4]  ~ VEOF */
                t[9] = 0;          /* c_cc[5]  ~ VTIME */
                t[10] = 1;         /* c_cc[6]  ~ VMIN = 1 */
            }
            ret = 0; break;
        }
        if (req == 0x5402 || req == 0x5403 || req == 0x5404) { /* TCSANOW / TCSETSW / TCSETSF */
            ret = 0; break;
        }
        ret = 0;
        break;
    }

    case SYS_poll: {
        /* ~~ poll ~ "Tem tecla ai?" ~~
         * struct pollfd { int fd; short events; short revents; }
         * events: POLLIN = 1
         * retorna quantos fds tem evento, ou 0 se timeout */
        uint64_t *fds_arr = (uint64_t *)a1;
        int nfds = (int)a2;
        int timeout = (int)a3;
        (void)timeout;

        int ready = 0;
        for (int i = 0; i < nfds && i < 16; i++) {
            int pfd_fd = *((int *)fds_arr + i * 2);
            short *revents = (short *)fds_arr + i * 4 + 3; /* offset 8i+6 */
            *revents = 0;
            if (pfd_fd == 0 && keyboard_avail()) {
                *revents = 1; /* POLLIN */
                ready++;
            }
            if (pfd_fd >= 3 && pfd_fd < MAX_FDS && fds[pfd_fd].used && fds[pfd_fd].type == 1 && fds[pfd_fd].counter > 0) {
                *revents = 1;
                ready++;
            }
            if (fd_readable(pfd_fd)) {
                *revents = 1;
                ready++;
            }
        }
        ret = ready;
        break;
    }

    /* ~~ select (23) ~ igual poll, mas com fd_set ~~
     * args Linux: rdi=nfds, rsi=readfds, rdx=writefds, rcx=exceptfds
     * fd_set = 1024 bits (128 bytes), FD_SETSIZE=1024 */
    case SYS_select: {
        int nfds = (int)a1;
        uint8_t *rd_set = (uint8_t *)a2;
        (void)nfds;

        /* fd 0 (stdin) + device fds (tty/mice pro Xorg) */
        int ready = 0;
        if (!rd_set) { ret = 0; break; }
        if (keyboard_avail()) {
            rd_set[0] |= 1;
            ready++;
        }
        for (int i = 3; i < nfds && i < MAX_FDS; i++) {
            if (!fds[i].used) continue;
            int readable = 0;
            if (fds[i].type == FD_TTY) readable = kbd_raw_avail();
            else if (fds[i].type == FD_MICE) readable = mice_avail();
            else readable = fd_readable(i);
            if (readable) {
                rd_set[i >> 3] |= (uint8_t)(1 << (i & 7));
                ready++;
            }
        }
        ret = ready;
        break;
    }

    /* ~~ ppoll (271) ~ poll com signal mask ~~
     * struct pollfd* + nfds + timespec* + sigset_t* + sizet
     * ignora a máscara (não temos sinais de verdade) */
    case 271: {
        uint64_t *fds_arr = (uint64_t *)a1;
        int nfds = (int)a2;
        int ready = 0;
        for (int i = 0; i < nfds && i < 16; i++) {
            int pfd_fd = *((int *)fds_arr + i * 2);
            short *revents = (short *)fds_arr + i * 4 + 3; /* offset 8i+6 */
            *revents = 0;
            if (pfd_fd == 0 && keyboard_avail()) {
                *revents = 1;
                ready++;
            }
            if (pfd_fd >= 3 && pfd_fd < MAX_FDS && fds[pfd_fd].used && fds[pfd_fd].type == 1 && fds[pfd_fd].counter > 0) {
                *revents = 1;
                ready++;
            }
            if (fd_readable(pfd_fd)) {
                *revents = 1;
                ready++;
            }
        }
        ret = ready;
        break;
    }

    /* ~~ eventfd2 (290) ~ contador 8 bytes, leitura consuma ~~
     * cria um fd virtual que só conta — suficiente pro
     * wayland/event loop avisar "tem coisa pra processar" */
    case 290: {
        if (a1 == 0 || a1 == 1) { /* eventfd_create(initval, flags) */
            int fd = alloc_fd();
            if (fd < 0) { ret = -1; break; }
            fds[fd].used = 1;
            fds[fd].type = 1;
            fds[fd].counter = a1; /* initval */
            ret = fd;
        } else {
            ret = -1;
        }
        break;
    }

    /* ~~ timerfd_create (283) ~~
     * clockid=1 (CLOCK_MONOTONIC), retorna fd */
    case 283: {
        int fd = alloc_fd();
        if (fd < 0) { ret = -1; break; }
        fds[fd].used = 1;
        fds[fd].type = 2;
        fds[fd].counter = 0;
        ret = fd;
        break;
    }

    /* ~~ timerfd_settime (286) ~ armazena intervalo (ms) ~~
     * itimerspec: {it_interval{sec,nsec}, it_value{sec,nsec}} */
    case 286: {
        int fd = (int)a1;
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 2) {
            uint64_t *spec = (uint64_t *)a3; /* new_value */
            uint64_t ms = 0;
            if (spec) {
                uint64_t sec  = spec[0] * 1000 + spec[1] / 1000000ULL; /* interval */
                uint64_t vsec = spec[2] * 1000 + spec[3] / 1000000ULL; /* value */
                ms = vsec ? vsec : sec;
            }
            fds[fd].counter = ms ? ms : 1;
        }
        ret = 0;
        break;
    }

    /* ~~ timerfd_gettime (287) ~ devolve o que sobrou ~~
     * sem timer real: devolve o intervalo como "restante" */
    case 287: {
        int fd = (int)a1;
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 2 && a2) {
            uint64_t *spec = (uint64_t *)a2;
            spec[0] = 0; spec[1] = 0;                      /* interval */
            spec[2] = fds[fd].counter / 1000;              /* value sec */
            spec[3] = (fds[fd].counter % 1000) * 1000000ULL; /* value nsec */
        }
        ret = 0;
        break;
    }

    /* ~~ epoll_create (213) / epoll_create1 (291) ~~
     * retorna um fd fake; ctl/wait são no-ops */
    case 213:
    case 291: {
        int fd = alloc_fd();
        if (fd < 0) { ret = -1; break; }
        fds[fd].used = 1;
        fds[fd].type = 0;
        fds[fd].name[0] = '\0';
        ret = fd;
        break;
    }

    case 233: /* epoll_ctl */
        ret = 0;
        break;

    case 232: /* epoll_wait */
    case 281: /* epoll_pwait */
        ret = 0;
        break;

    case SYS_sigaction:
    case SYS_sigreturn:
        ret = 0;
        break;

    case SYS_gettimeofday: {
        struct timeval *tv = (struct timeval *)a1;
        if (tv) {
            uint64_t t = timer_ticks;
            tv->tv_sec = t / 100;
            tv->tv_usec = (t % 100) * 10000;
        }
        ret = 0;
        break;
    }

    /* ~~ SYS_disp_get_fb (200) ~ "Me da o framebuffer!!" ~~
     * Mapeia o framebuffer VESA/virtio no espaco do usuario e devolve
     * as dimensoes. CUIDADO: a ordem dos parametros eh:
     *   a1 (RDI) = &fb_addr  (uint64_t*)
     *   a2 (RSI) = &width    (uint32_t*)  <-- ANTES tava como pitch!!!
     *   a3 (RDX) = &height   (uint32_t*)
     *   a4 (RCX) = &pitch    (uint32_t*)  <-- ANTES tava como width!!!
     * Sim, eu troquei a2 e a4. O debug de 3 horas foi culpa disso. Desculpa~
     * Adicionei guard pra g_fb.width==0 tbm, pq o VESA as vezes acorda
     * desligado e nao quer passar largura. */
    case SYS_disp_get_fb: {
        extern int g_fb_active;
        extern framebuffer_t g_fb;
        if (!g_fb_active || g_fb.bpp != 32) { ret = -1; break; }
        if (g_fb.width == 0 || g_fb.height == 0) { ret = -1; break; }
        uint64_t user_fb_va = 0xFFFFFFFF80000000ULL;
        uint64_t fb_size = (uint64_t)g_fb.pitch * g_fb.height;
        fb_size = (fb_size + 0x1FFFFF) & ~0x1FFFFF;
        pcb_t *cur = process_current();
        int r = pml4_map_phys(cur->pml4, user_fb_va, g_fb.addr, fb_size, 1);
        if (r < 0) { ret = -1; break; }
        uint64_t *addr   = (uint64_t *)a1;
        uint32_t *width  = (uint32_t *)a2;
        uint32_t *height = (uint32_t *)a3;
        uint32_t *pitch  = (uint32_t *)a4;
        if (addr)   *addr   = user_fb_va;
        if (width)  *width  = g_fb.width;
        if (height) *height = g_fb.height;
        if (pitch)  *pitch  = g_fb.pitch;
        ret = 0;
        break;
    }

    /* ~~ SYS_mouse_read (202) ~ "Mexe o mouse, baka!" ~~
     * Le o acumulado do mouse (dx, dy, botoes) e devolve pro usuario.
     * Implementei de verdade agora (antes tava so retornando 0, o que
     * fazia o cursor ficar parado igual estátua. Triste.) */
    case SYS_mouse_read: {
        int *dx      = (int *)a1;
        int *dy      = (int *)a2;
        int *buttons = (int *)a3;
        mouse_read_delta(dx, dy, buttons);
        ret = 0;
        break;
    }

    /* ~~ SYS_disp_flush (201) ~ "Joga o backbuffer na tela!" ~~
     * Copia o backbuffer inteiro pro framebuffer usando rep movsl.
     * Nao usa SSE aqui porque o kernel nao salva xmm registers.
     * rep movsl é otimizado pelo CPU (ERMSB) e pelo QEMU (memcpy host). */
    case SYS_disp_flush: {
        extern int g_fb_active;
        extern framebuffer_t g_fb;
        extern int g_virtio_active;
        void *backbuffer = (void *)a1;
        if (!g_fb_active || !backbuffer) break;
        uint64_t fb_va = g_virtio_active ? g_fb.addr : 0xFFFFFFFF80000000ULL;
        uint32_t *fb_virt = (uint32_t *)fb_va;
        size_t pixels = (size_t)g_fb.pitch * g_fb.height / 4;
        __asm__ volatile (
            "rep movsl"
            : "+D"(fb_virt), "+S"(backbuffer), "+c"(pixels)
            : : "memory"
        );
        if (g_virtio_active)
            virtio_gpu_flush();
        ret = 0;
        break;
    }

    /* ~~ SYS_disp_flush_rect (205) ~ "So uma parte, nao tudo!" ~~
     * Versao otimizada do flush: so copia um retangulo do backbuffer.
     * Embrulha x,y em a2 e w,h em a3 (cada um em 16 bits). Sim, é
     * uma gambiarra, mas economiza registrador~ >_< */
    case SYS_disp_flush_rect: {
        extern int g_fb_active;
        extern framebuffer_t g_fb;
        extern int g_virtio_active;
        void *backbuffer = (void *)a1;
        int x = (int)(a2 & 0xFFFF);
        int y = (int)((a2 >> 16) & 0xFFFF);
        int w = (int)(a3 & 0xFFFF);
        int h = (int)((a3 >> 16) & 0xFFFF);
        if (!g_fb_active || !backbuffer || w <= 0 || h <= 0) break;
        uint64_t fb_va = g_virtio_active ? g_fb.addr : 0xFFFFFFFF80000000ULL;
        uint32_t stride_px = g_fb.pitch / 4;
        uint32_t *src = (uint32_t *)backbuffer + (uint32_t)y * stride_px + (uint32_t)x;
        uint32_t *dst = (uint32_t *)fb_va + (uint32_t)y * stride_px + (uint32_t)x;
        for (int row = 0; row < h; row++) {
            uint32_t *row_src = src;
            uint32_t *row_dst = dst;
            int cw = w;
            __asm__ volatile ("rep movsl" : "+D"(row_dst), "+S"(row_src), "+c"(cw) : : "memory");
            src += stride_px;
            dst += stride_px;
        }
        if (g_virtio_active)
            virtio_gpu_flush();
        ret = 0;
        break;
    }

    /* ~~ SYS_kb_mod (203) ~ "Shift? Ctrl? Me diz ai!" ~~
     * Devolve o estado das teclas modificadoras (shift, ctrl).
     * Pro disp-wm saber se o usuario quer abrir menu ou matar janela~ */
    case SYS_kb_mod: {
        extern volatile int shift_pressed;
        extern volatile int ctrl_pressed;
        uint32_t mods = 0;
        if (shift_pressed) mods |= 1;
        if (ctrl_pressed)  mods |= 2;
        ret = mods;
        break;
    }

    /* ~~ SYS_readdir (207) ~ "Lista os arquivos, por favor!" ~~
     * Le o diretorio FAT32 e preenche um array de entradas.
     * Salva/restaura o CWD pra nao atrapalhar o processo chamador.
     * Se o path nao existir, retorna -1 e chora baixinho~ */
    case SYS_readdir: {
        const char *path = (const char *)a1;
        fat32_dirent_t *entries = (fat32_dirent_t *)a2;
        int max_entries = (int)a3;
        if (!path || !entries || max_entries <= 0) { ret = -1; break; }
        uint32_t saved = fat32_get_cwd();
        if (fat32_change_dir(path) < 0) { ret = -1; break; }
        int n = fat32_match_prefix("", fat32_get_cwd(), entries, max_entries);
        fat32_set_cwd(saved);
        ret = n;
        break;
    }

    /* ~~ SYS_execve (208) ~ "Executa um programa!" ~~
     * Carrega um binario Mach-O do FAT32 e substitui o processo atual.
     * Faz a separacao do path em diretorio + nome (sim, eu implementei
     * dirname na mao pq nao tinha libc no kernel). Le o arquivo, passa
     * pro mach_o_load, e configura o iretq stack frame pro entry point.
     * É tipo um fork+exec mas sem o fork~ economia de recursos! >_< */
    case SYS_execve: {
        const char *path = (const char *)a1;
        if (!path || !path[0]) { ret = -1; break; }
        uint32_t saved_cwd = fat32_get_cwd();
        const char *fname = path;
        char dirbuf[256];
        int last_slash = -1;
        for (int i = 0; path[i]; i++)
            if (path[i] == '/') last_slash = i;
        if (last_slash >= 0) {
            int dirlen = last_slash;
            if (dirlen > 255) dirlen = 255;
            for (int i = 0; i < dirlen; i++) dirbuf[i] = path[i];
            dirbuf[dirlen] = '\0';
            fname = path + last_slash + 1;
            if (dirbuf[0] && fat32_change_dir(dirbuf) < 0) {
                fat32_set_cwd(saved_cwd); ret = -1; break;
            }
        }
        uint32_t fsize; uint8_t attr;
        if (fat32_stat(fname, &fsize, &attr, 0, 0) < 0) {
            fat32_set_cwd(saved_cwd); ret = -1; break;
        }
        uint8_t *buf = kmalloc(fsize + 1);
        if (!buf) { fat32_set_cwd(saved_cwd); ret = -1; break; }
        if (fat32_read_file(fname, buf, fsize) < 0) {
            kfree(buf); fat32_set_cwd(saved_cwd); ret = -1; break;
        }
        fat32_set_cwd(saved_cwd);
        void *entry = mach_o_load(buf, fsize);
        kfree(buf);
        if (!entry) { ret = -1; break; }
        void *new_stack = kmalloc(65536);
        if (!new_stack) { ret = -1; break; }
        /* Split 2MB huge pages into 4KB pages and add U/S bit.
         * Huge pages were causing spurious P=0 page faults on KVM. */
        uint64_t cur_pml4 = pml4_get_current();
        serial_puts("exec: splitting PD\r\n");
        int r0 = split_2mb_pde(cur_pml4, 0xA00000);
        serial_puts("exec: split A00000="); serial_puthex((uint32_t)r0); serial_puts("\r\n");
        /* Read back PD[5] after split */
        {
            uint64_t *pml4v = (uint64_t *)(uintptr_t)cur_pml4;
            uint64_t *pdptv = (uint64_t *)(uintptr_t)(pml4v[0] & ~0xFFFULL);
            uint64_t *pdv = (uint64_t *)(uintptr_t)(pdptv[0] & ~0xFFFULL);
            serial_puts("exec: PD[5] after split=");
            serial_puthex((uint32_t)pdv[5]);
            serial_puts("\r\n");
        }
        pml4_add_user_4kb(cur_pml4, 0x500000);
        pml4_add_user_4kb(cur_pml4, 0xA00000);
        pml4_add_user_4kb(cur_pml4, 0x6000000);
        pml4_add_user_4kb(cur_pml4, 0xE00000);
        for (int i = 1; i < 15; i++) regs[i] = 0;
        uint64_t *iretq = (uint64_t *)(regs + 15);
        iretq[0] = (uint64_t)entry;
        iretq[1] = 0x1B;
        iretq[2] = 0x202;
        iretq[3] = (uint64_t)new_stack + 65536;
        iretq[4] = 0x23;
        ret = 0;
        break;
    }

    /* ~~ SYS_shell_cmd (209) ~ "Roda um comando e pega a saida!" ~~
     * Executa um comando do shell e captura a saida num buffer.
     * Usa o console_set_output_buffer pra redirecionar a saida.
     * Depois volta ao normal. É tipo um popen() mas mais ~gambiarra~ */
    case SYS_shell_cmd: {
        const char *cmd = (const char *)a1;
        char *outbuf = (char *)a2;
        int outsize = (int)a3;
        if (!cmd || !cmd[0] || !outbuf || outsize <= 0) { ret = -1; break; }
        int outlen = 0;
        console_set_output_buffer(outbuf, &outlen, outsize);
        execute(cmd);
        console_set_output_buffer(NULL, NULL, 0);
        ret = outlen;
        break;
    }

    /* ~~ SYS_spawn (210) ~ "Cria um processo filho!" ~~
     * A syscall mais nova do pedaço! Carrega um binario, cria uma PML4
     * propria pro processo filho (isolamento de verdade!), aloca pilha,
     * e usa o proc_spawn pra criar o processo. O filho vai ter o proprio
     * espaco de enderecamento, entao se ele crashar, o pai nao morre junto~
     * (Diferente do SYS_execve que substitui o processo atual.)
     * Usei clone_identity_tables() pra copiar as tabelas de paginacao
     * e mach_o_load_into_pml4 pra carregar o binario na PML4 do filho. */
    case SYS_spawn: {
        const char *fname = (const char *)a1;
        if (!fname || !fname[0]) { ret = -1; break; }
        uint32_t fsize; uint8_t attr;
        if (fat32_stat(fname, &fsize, &attr, 0, 0) < 0) { ret = -1; break; }
        uint8_t *buf = kmalloc(fsize + 1);
        if (!buf) { ret = -1; break; }
        if (fat32_read_file(fname, buf, fsize) < 0) { kfree(buf); ret = -1; break; }
        uint64_t child_pml4 = clone_identity_tables();
        if (!child_pml4) { kfree(buf); ret = -1; break; }

        /* ~~ ELF vs Mach-O: dois jeitos de ser um binary ~~
         * ELF = Linux padrao, Mach-O = nosso formato legado~
         * Se tiver os 4 bytes magicos "\x7fELF", é um~ */
        uint64_t dyn_interp_entry = 0;
        int dyn_is_dynamic = 0;
        int is_elf = (fsize >= 4 && buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F');
        uint64_t elf_phdr = 0, elf_phent = 0, elf_phnum = 0, elf_bss_end = 0;
        void *entry;
        uint64_t elf_base = 0;
        if (is_elf) {
            Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buf;
            elf_phent = ehdr->e_phentsize;
            elf_phnum = ehdr->e_phnum;
            elf_base = (ehdr->e_type == 3) ? elf64_pie_base() : 0; /* ET_DYN = PIE */
            /* ~~ Calcula o endereco virtual da tabela PHDR + fim do BSS ~~
             * O BSS end vira o program_break inicial (o musl chama
             * brk(0) pra descobrir onde o heap começa — e antes o
             * kernel respondia 0x201000 pra todo mundo, um chute~) */
            if (elf_phnum) {
                Elf64_Phdr *pp = (Elf64_Phdr *)(buf + ehdr->e_phoff);
                for (uint32_t k = 0; k < elf_phnum; k++) {
                    if (pp->p_type == PT_LOAD) {
                        if (!elf_phdr)
                            elf_phdr = elf_base + pp->p_vaddr + (ehdr->e_phoff - pp->p_offset);
                        uint64_t seg_end = elf_base + pp->p_vaddr + pp->p_memsz;
                        if (seg_end > elf_bss_end) elf_bss_end = seg_end;
                    }
                    pp = (Elf64_Phdr *)((uint8_t *)pp + elf_phent);
                }
                elf_bss_end = (elf_bss_end + 0xFFF) & ~0xFFFULL;
            }
            entry = elf64_load_into_pml4(buf, fsize, child_pml4);

            /* ~~ ELF DINÂMICO (#71): se tem PT_INTERP, carrega o ld.so~~
             * O interpretador roda PRIMEIRO (entry dele), resolve as
             * libs do DT_NEEDED e só depois pula pro entry do binário.
             * AT_BASE passa a apontar pro INTERPRETADOR~ */
            char interp_path[256];
            int ilen = -1;
            if (is_elf)
                ilen = elf64_find_interp(buf, fsize, interp_path, sizeof(interp_path));
            if (ilen > 0) {
                serial_puts("[dyn] interp=");
                serial_puts(interp_path);
                serial_puts("\r\n");
                unsigned char *idata = kmalloc(4 * 1024 * 1024);
                if (idata && vfs_read_file(interp_path, idata, 4 * 1024 * 1024) > 0) {
                    const uint64_t INTERP_BASE = 0x60000000ULL; /* longe do PIE(0x40000000) e do mmap(0x70000000) */
                    void *ientry = elf64_load_at(idata, 4 * 1024 * 1024, child_pml4, INTERP_BASE);
                    if (ientry) {
                        dyn_interp_entry = (uint64_t)ientry;
                        dyn_is_dynamic = 1;
                        serial_puts("[dyn] ld.so no ar\r\n");
                    } else {
                        serial_puts("[dyn] falha ao mapear interp\r\n");
                    }
                } else {
                    serial_puts("[dyn] interp não encontrado no FS\r\n");
                }
                if (idata) kfree(idata);
            }
        } else {
            entry = mach_o_load_into_pml4(buf, fsize, child_pml4);
        }
        kfree(buf);
        if (!entry) { ret = -1; break; }
        if (dyn_is_dynamic) entry = (void *)dyn_interp_entry;
        void *user_stack = kmalloc(65536);
        if (!user_stack) { ret = -1; break; }
        /* ~~ Torna a pilha visivel pro usuario ~~
         * clone_identity_tables tirou o U/S da memoria identitaria~
         * A gente devolve pro processo filho acessar a pilha dele! */
        {
            uint64_t sa = (uint64_t)user_stack;
            for (uint64_t va = sa & ~0x1FFFFFULL; va < sa + 65536; va += 0x200000)
                pml4_add_user(child_pml4, va);
        }
        int pid = proc_spawn(fname, entry, (uint8_t *)user_stack + 65536);
        if (pid < 0) { kfree(user_stack); ret = -1; break; }
        for (int i = 0; i < MAX_PROC; i++) {
            if (pcb_table[i].pid == pid) {
                pml4_destroy(pcb_table[i].pml4);
                pcb_table[i].pml4 = child_pml4;
                /* ~~ Setup do vetor auxiliar do Linux ~~
                 * argc, argv, envp, e os AT_* pros binarios ELF~
                 * Assim o codigo do usuario acha os argumentos~ */
                if (is_elf) {
                    /* ~~ Program break começa no fim do BSS do ELF ~~
                     * (era 0x201000 hardcoded do proc_spawn — o musl
                     *  brk(0) respondia 2MB e o heap ia parar lá~) */
                    if (elf_bss_end) pcb_table[i].program_break = elf_bss_end;
                    uint64_t *kframe = (uint64_t *)pcb_table[i].kernel_rsp;
                    uint64_t old_rsp = kframe[18];
                    /* ~~ AT_BASE: dinâmico → base do ld.so (interp)~~
                     * AT_ENTRY(9): entry do binário principal pro ld.so~ */
                    kframe[18] = setup_linux_user_stack_dyn(&pcb_table[i], old_rsp,
                                                           elf_phdr, elf_phent, elf_phnum,
                                                           elf_base, NULL, 0, NULL, 0,
                                                           dyn_interp_entry, (uint64_t)entry,
                                                           dyn_is_dynamic);
                }
            }
        }
        ret = pid;
        break;
    }

    /* ~~ SYS_spawn_shared (211) ~ spawn com PML4 clonada ~~
     * Cria um processo filho com uma COPIA da PML4 do pai.
     * O filho enxerga os mesmos buffers/paginas que o pai
     * (incluindo 0x500000 e buffers alocados via mmap),
     * mas tem suas proprias tabelas de paginacao. Isso significa
     * que quando carregamos um binario DIFERENTE (ex: TERM em
     * vez do compositor), o codigo do pai nao eh sobrescrito.
     *
     * ANTES usava mach_o_load() que copiava o binario direto
     * em 0x10000000 — o MESMO endereco onde o compositor vive.
     * O compositor morria na volta da syscall porque o codigo
     * dele tinha virado codigo do TERM. affs, 3 dias de debug
     * por causa disso. >_<
     *
     * AGORA: clona a PML4, carrega o binario na PML4 do filho
     * (mach_o_load_into_pml4), e o pai continua intacto. O filho
     * ve as mesmas paginas de memoria compartilhada (0x500000,
     * buffer de pixel) porque a PML4 clonada copia todas as
     * entradas de paginacao, incluindo o mapeamento identitario.
     * rssrsrs, quem diria que clonar a PML4 resolve TUDO. */
    case SYS_spawn_shared: {
        const char *fname = (const char *)a1;
        if (!fname || !fname[0]) { serial_puts("spawn: nome vazio\n"); ret = -1; break; }
        serial_puts("spawn: path="); serial_puts(fname); serial_puts("\n");

        /* ~~ Path resolution: separa diretorio do nome do arquivo ~~
         * O FAT32 so procura nomes simples (sem '/') no diretorio
         * atual. O shell mudou pra /BIN antes de rodar DISP, entao
         * current_dir_cluster aponta pra /BIN, nao pra raiz. E a
         * funcao find_entry() nao entende paths com '/'. Entao
         * precisamos fazer o chdir manualmente antes de stat/read. */
        uint32_t saved_cwd = fat32_get_cwd();
        char _p[256]; int _i = 0;
        while (fname[_i] && _i < 255) { _p[_i] = fname[_i]; _i++; }
        _p[_i] = '\0';
        char *_base = _p, *_last_slash = 0, *_s = _p;
        if (*_base == '/') { fat32_change_dir("/"); _base++; _s = _base; }
        while (*_s) { if (*_s == '/') _last_slash = _s; _s++; }
        char *_file = _base;
        if (_last_slash) {
            *_last_slash = '\0';
            _file = _last_slash + 1;
            /* Walk each directory component */
            char *_w = _base;
            while (_w && *_w) {
                char *_n = _w;
                while (*_n && *_n != '/') _n++;
                int _end = (*_n == '\0');
                if (*_n) *_n = '\0';
                if (_w[0] && fat32_change_dir(_w) < 0) {
                    serial_puts("spawn: chdir "); serial_puts(_w); serial_puts(" falhou\n");
                    ret = -1; break;
                }
                if (_end) break;
                _w = _n + 1;
            }
        }
        if (ret < 0) { fat32_set_cwd(saved_cwd); break; }
        serial_puts("spawn: procurando '"); serial_puts(_file); serial_puts("'\n");
        uint32_t fsize; uint8_t attr;
        if (fat32_stat(_file, &fsize, &attr, 0, 0) < 0) {
            serial_puts("spawn: stat falhou\n");
            fat32_set_cwd(saved_cwd); ret = -1; break;
        }
        serial_puts("spawn: size="); serial_puthex(fsize); serial_puts("\n");
        uint8_t *buf = kmalloc(fsize + 1);
        if (!buf) { serial_puts("spawn: sem kmem\n"); fat32_set_cwd(saved_cwd); ret = -1; break; }
        if (fat32_read_file(_file, buf, fsize) < 0) {
            serial_puts("spawn: read falhou\n"); kfree(buf);
            fat32_set_cwd(saved_cwd); ret = -1; break;
        }
        fat32_set_cwd(saved_cwd);
        uint64_t child_pml4 = clone_identity_tables();
        if (!child_pml4) { kfree(buf); serial_puts("spawn: clone_pml4 falhou\n"); ret = -1; break; }
        serial_puts("spawn: child_pml4="); serial_puthex((uint32_t)child_pml4); serial_puts("\n");

        /* Detect ELF vs Mach-O */
        int is_elf = (fsize >= 4 && buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F');
        uint64_t elf_phdr = 0, elf_phent = 0, elf_phnum = 0;
        uint64_t elf_base = 0;
        void *entry;
        if (is_elf) {
            Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buf;
            elf_phent = ehdr->e_phentsize;
            elf_phnum = ehdr->e_phnum;
            elf_base = (ehdr->e_type == 3) ? elf64_pie_base() : 0; /* ET_DYN = PIE */
            if (elf_phnum) {
                Elf64_Phdr *pp = (Elf64_Phdr *)(buf + ehdr->e_phoff);
                for (uint32_t k = 0; k < elf_phnum; k++) {
                    if (pp->p_type == PT_LOAD) {
                        elf_phdr = elf_base + pp->p_vaddr + (ehdr->e_phoff - pp->p_offset);
                        break;
                    }
                    pp = (Elf64_Phdr *)((uint8_t *)pp + elf_phent);
                }
            }
            entry = elf64_load_into_pml4(buf, fsize, child_pml4);
        } else {
            entry = mach_o_load_into_pml4(buf, fsize, child_pml4);
        }
        kfree(buf);
        if (!entry) { serial_puts("spawn: load_into_pml4 falhou\n"); ret = -1; break; }
        serial_puts("spawn: entry="); serial_puthex((uint32_t)(uintptr_t)entry); serial_puts("\n");
        uint8_t *user_stack = kmalloc(65536);
        if (!user_stack) { serial_puts("spawn: sem stack\n"); ret = -1; break; }
        pml4_add_user(child_pml4, 0x500000);
        pml4_add_user(child_pml4, 0xA00000);
        pml4_add_user(child_pml4, 0x6000000);
        pml4_add_user(child_pml4, 0xE00000);
        int pid = proc_spawn(fname, entry, user_stack + 65536);
        if (pid < 0) { kfree(user_stack); serial_puts("spawn: proc_spawn falhou\n"); ret = -1; break; }
        serial_puts("spawn: pid="); serial_puthex((uint32_t)pid); serial_puts("\n");
        for (int i = 0; i < MAX_PROC; i++) {
            if (pcb_table[i].pid == pid) {
                pml4_destroy(pcb_table[i].pml4);
                pcb_table[i].pml4 = child_pml4;
                if (is_elf) {
                    uint64_t *kframe = (uint64_t *)pcb_table[i].kernel_rsp;
                    uint64_t old_rsp = kframe[18];
                    kframe[18] = setup_linux_user_stack(&pcb_table[i], old_rsp,
                                                          elf_phdr, elf_phent, elf_phnum,
                                                          elf_base, NULL, 0, NULL, 0);
                }
                break;
            }
        }
        ret = pid;
        break;
    }

    /* ~~ Linux arch_prctl (158) ~ TLS pra musl! ~~
     * ARCH_SET_FS (0x1002): seta o FS.base pro TLS do usuário~
     * ARCH_GET_FS (0x1003): devolve o FS.base atual~
     * (GS a gente nem usa, então ARCH_SET_GS/GET_GS são só um
     *  tapinha nas costas e retornam 0 sem fazer nada~ hihi)
     * Se o processo não existir, retorna -1 (você não tem corpo pra
     *  ter TLS, baka! >_<) */
    case SYS_arch_prctl: {
        int code = (int)a1;
        pcb_t *p = process_current();
        if (!p) { ret = -1; break; }
        switch (code) {
        case 0x1002: { /* ARCH_SET_FS — grava o FS.base! */
            p->fs_base = a2;
            __asm__ volatile("wrmsr" :: "c"(0xC0000100), "a"((uint32_t)a2), "d"((uint32_t)(a2 >> 32)));
            ret = 0;
            break;
        }
        case 0x1003: { /* ARCH_GET_FS — lê o FS.base! */
            uint64_t *out = (uint64_t *)a2;
            if (out) *out = p->fs_base;
            ret = 0;
            break;
        }
        case 0x1001: /* ARCH_SET_GS — nope, next~ */ ret = 0; break;
        case 0x1004: /* ARCH_GET_GS — tbm nope~ */ ret = 0; break;
        default: ret = -1; break;
        }
        break;
    }

    /* ~~ Linux brk (231) ~ "Me mais memoria!" ~~
     * Se arg for 0, retorna o break atual sem mudar nada~
     * Se arg > break, atualiza (assume que deu certo~ confia)
     * Musl chama brk(0) só pra saber onde o heap começa~
     * (tipo quando você pergunta "que horas são?" e o relógio
     *  responde "sim" — utilidade duvidosa mas todo mundo usa) */
    case SYS_brk: {
        pcb_t *p = process_current();
        if (!p) { ret = -1; break; }
        if (a1 == 0) {
            ret = p->program_break;
        } else if (a1 > p->program_break) {
            /* ~~ Crescendo: mapeia as páginas novas ANTES de aceitar! ~~ */
            serial_puts("[brk] ");
            serial_puthex((uint32_t)p->program_break);
            serial_puts(" -> ");
            serial_puthex((uint32_t)a1);
            serial_puts("\r\n");
            uint64_t start = (p->program_break + 0xFFF) & ~0xFFFULL;
            uint64_t end = (a1 + 0xFFF) & ~0xFFFULL;
            int ok = 1;
            for (uint64_t va = start; va < end; va += 0x1000) {
                if (map_user_4kb(p->pml4, va) < 0) {
                    serial_puts("[brk] MAPFAIL va=");
                    serial_puthex((uint32_t)va);
                    serial_puts("\r\n");
                    ok = 0; break;
                }
            }
            if (ok) {
                p->program_break = a1;
                ret = a1;
            } else {
                /* ~~ Linux devolve o break antigo quando falha ~~ */
                ret = p->program_break;
            }
        } else {
            /* ~~ Shrinking brk (raro mas permitido) ~~
             * Tipo devolver um pedaço do bolo depois de já
             * ter comido — estranho, mas a gente deixa~ */
            p->program_break = a1;
            ret = p->program_break;
        }
        break;
    }

    /* ~~ Linux exit_group (212) ~ "Mata todo mundo!" ~~
     * No nosso caso sem thread groups, vira um exit normal~
     * (você é grupo de um só, hihi~ solidão mode on) */
    case SYS_exit_group:
        process_exit_current((int)a1);
        for (;;) __asm__ volatile("hlt");
        break;

    /* ~~ Linux set_tid_address (218) ~ "Aqui, guarda meu TID!" ~~
     * Musl passa o endereço de uma variável onde o kernel
     * deveria escrever o TID quando a thread morre~
     * Mas como somos um kernel ~fofo~ e sem threads de verdade,
     * a gente só devolve o PID e ignora o endereço~ */
    case SYS_set_tid_address:
        ret = process_current_pid();
        break;

    /* ~~ threads e sinais (issue #50) ~~
     * weston/Xorg usam pthread (libwayland) e sinais pra encerrar.
     * Sem futex/robust_list a musl aborta em pthread_create.
     * Como ainda não temos threads de verdade (sem clone), os
     * stubs são "realistas": futex WAIT/WAKE real (pro lock do
     * malloc em memória compartilhada), sinais aceitam mas não
     * entregam, e wait4 liga no proc_waitpid que já existe~ */

    /* ~~ Linux wait4 (61) ~ "Espera o filho morrer~" ~~
     * Args: pid, &status, options, rusage.
     * Proc_waitpid já bloqueia até o filho virar zombie. */
    case SYS_wait4: {
        int pid = (int)a1;
        int *status = (int *)(uintptr_t)a2;
        int wstatus = 0;
        int r = proc_waitpid(pid, &wstatus);
        if (r >= 0 && status) *status = wstatus;
        ret = r;
        break;
    }

    /* ~~ Linux kill (62) / tgkill (234) / tkill (200) ~
     * "Mata o processo!" — mas sem thread de verdade, aceita e
     * ignora (retornar 0 é aceitável p/ boot, diz a issue~) */
    case SYS_kill:
    case SYS_tgkill:
        ret = 0;
        break;

    /* ~~ Linux rt_sigprocmask (14) ~ "Bloqueia/desbloqueia sinal" ~~
     * Stub realista: aceita e retorna 0 (sinais não são entregues
     * de verdade, então a máscara não muda nada~) */
    case SYS_rt_sigprocmask:
        ret = 0;
        break;

    /* ~~ Linux rt_sigreturn (15) ~ "Volta do handler de sinal" ~~
     * Sem handlers de verdade, nunca é chamado — mas musl pode
     * passar por aqui, então retorna 0~ */
    case SYS_rt_sigreturn:
        ret = 0;
        break;

    /* ~~ Linux sigaltstack (131) ~ "Stack alternativo p/ handler" ~~
     * Stub: aceita e retorna 0~ */
    case SYS_sigaltstack:
        ret = 0;
        break;

    /* ~~ Linux prctl (157) ~ "Opções de processo" ~~
     * PR_SET_NAME (15) / PR_SET_DUMPABLE (4) são os que o weston usa.
     * Aceita e retorna 0~ */
    case SYS_prctl:
        ret = 0;
        break;

    /* ~~ Linux set_robust_list (273) / get_robust_list (274) ~~
     * Lista de futexes pra cancelamento de thread.
     * Guarda o ponteiro pra devolver quando pedirem~ */
    case SYS_set_robust_list:
        ret = 0;
        break;
    case SYS_get_robust_list: {
        uint64_t *head = (uint64_t *)(uintptr_t)a2;
        if (head) *head = 0; /* lista vazia — sem threads~ */
        ret = 0;
        break;
    }

    /* ~~ Linux rseq (334) ~ "Restartable sequences" ~~
     * glibc usa; musl nem precisa. Se retornar ENOSYS, a libc
     * desativa sozinha. Retornar 0 é o caminho mais seguro~ */
    case SYS_rseq:
        ret = 0;
        break;

    /* ~~ Linux futex (202→96) ~ "Sincronização de threads" ~~
     * op: 0=FUTEX_WAIT, 1=FUTEX_WAKE, 2=FUTEX_FD, 4=REQUEUE...
     * Sem threads de verdade, o lock do malloc da musl em
     * memória compartilhada é o caso mais comum: WAIT compara
     * *uaddr com val, WAKE acorda n. Como processos rodam em
     * round-robin num core só, a memória compartilhada já é
     * "sequencial" — então só conferir o valor resolve~ */
    case SYS_futex: {
        uint32_t *uaddr = (uint32_t *)(uintptr_t)a1;
        int op = (int)a2;
        int val = (int)a3;
        int op_part = op & 0x7F; /* ignora FLAGS/PRIVATE bits */
        (void)val;
        if (op_part == 0 && uaddr) { /* FUTEX_WAIT: dorme até acordar */
            /* sem threads de verdade, sempre "acordado" — devolve 0 */
            ret = 0;
            break;
        }
        if (op_part == 1) { /* FUTEX_WAKE: acorda n threads */
            ret = 0; /* ninguém pra acordar~ */
            break;
        }
        ret = 0; /* outras ops: aceita e ignora~ */
        break;
    }

    /* ~~ Linux clock_gettime (228) ~ "Que horas são?" ~~
     * Recebe clock_id (a1) e struct timespec *tp (a2).
     * Ignora o clock_id (todo relógio é relógio quando se é
     *  um kernel minimalista~) e usa timer_ticks.
     * timespec: { tv_sec, tv_nsec } — sim, segundos e nanossegundos~
     * Pode não ser preciso mas pelo menos não é monotônico~ */
    case SYS_clock_gettime: {
        struct { uint64_t tv_sec; uint64_t tv_nsec; } *tp = (void *)a2;
        if (tp) {
            uint64_t t = timer_ticks;
            tp->tv_sec = t / 100;
            tp->tv_nsec = (t % 100) * 10000000ULL;
        }
        ret = 0;
        break;
    }

    /* ~~ Linux nanosleep (234) ~ "Dorme um pouquinho!" ~~
     * Só retorna 0 — sem sleep real nesse stub~
     * (dormir é pra fracos, a gente só finge que dormiu~
     *  igual quando você diz que vai dormir cedo e fica
     *  vendo video até as 3 da manhã~ hihi) */
    case SYS_nanosleep:
        ret = 0;
        break;

    /* ~~ Linux uname (63) ~ "Quem sou eu?" ~~
     * Preenche struct utsname com identidade do sistema.
     * sysname=Linux (pra libc não reclamar), release=TipOS~
     * machine=x86_64 (que é a verdade mesmo~ kyun) */
    case SYS_uname: {
        char *uts = (char *)a1;
        if (!uts) { ret = -1; break; }
        const char *fields[] = {
            "Linux", "tipos", "6.1-tipos", "TipOS 1.0", "x86_64", "(none)"
        };
        /* cada campo tem 65 bytes (__UTS_LEN+1) */
        for (int f = 0; f < 6; f++) {
            const char *s = fields[f];
            char *dst = uts + f * 65;
            int i = 0;
            while (s[i] && i < 64) { dst[i] = s[i]; i++; }
            dst[i] = '\0';
        }
        ret = 0;
        break;
    }

    /* ~~ Linux getrandom (318) ~ "Me da sorte aleatória!" ~~
     * Preenche o buffer com bytes do xorshift (PRNG do kernel)~
     * Não é criptográfico de verdade, mas pro boot do weston
     * (que usa pra seeds) já serve~ confia~ */
    case SYS_getrandom: {
        uint8_t *buf = (uint8_t *)a1;
        size_t len = (size_t)a2;
        static uint64_t x = 0x9E3779B97F4A7C15ULL;
        if (!buf || len == 0) { ret = 0; break; }
        for (size_t i = 0; i < len; i++) {
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            buf[i] = (uint8_t)x;
        }
        ret = (int)len;
        break;
    }

    /* ~~ Linux clock_getres (229) ~ "Quão preciso é teu relógio?" ~~
     * PIT a 100Hz → resolução de 10ms. Devidamente humilde~ */
    case SYS_clock_getres: {
        struct { uint64_t tv_sec; uint64_t tv_nsec; } *tp = (void *)a2;
        if (tp) {
            tp->tv_sec = 0;
            tp->tv_nsec = 10000000ULL; /* 10ms */
        }
        ret = 0;
        break;
    }

    /* ~~ Linux getrusage (98) ~ "Quanto CPU eu usei?" ~~
     * Zerado por preguiça — sem preempção real, ninguém mede~ */
    case SYS_getrusage: {
        uint8_t *ru = (uint8_t *)a2;
        if (ru) {
            /* struct rusage tem 18 longs (144 bytes) */
            for (int i = 0; i < 144; i++) ru[i] = 0;
        }
        ret = 0;
        break;
    }

    /* ~~ Linux times (100) ~ "Ticks do processo!" ~~
     * struct tms { tms_utime, tms_stime, tms_cutime, tms_cstime }
     * Devolve o relógio global e ticks zerados~ */
    case SYS_times: {
        uint64_t *tms = (uint64_t *)a1;
        if (tms) {
            tms[0] = timer_ticks; /* utime */
            tms[1] = 0;           /* stime */
            tms[2] = 0;           /* cutime */
            tms[3] = 0;           /* cstime */
        }
        ret = timer_ticks;
        break;
    }

    /* ~~ Linux sysinfo (99) ~ "Quanta RAM tem?" ~~
     * struct sysinfo: uptime, loads[3], totalram, freeram, ...~
     * O heap do kernel tem 64MB; reportamos isso honestamente~ */
    case SYS_sysinfo: {
        uint64_t *si = (uint64_t *)a1;
        if (si) {
            si[0] = timer_ticks / 100;   /* uptime (s) */
            si[1] = 0; si[2] = 0; si[3] = 0; /* loads[3] */
            si[4] = 64u * 1024u * 1024u;  /* totalram (64MB heap) */
            si[5] = 32u * 1024u * 1024u;  /* freeram (chute otimista~) */
            si[6] = 0;                     /* sharedram */
            si[7] = 0;                     /* bufferram */
            si[8] = 0; si[9] = 0;          /* totalswap, freeswap */
            /* si[10] = procs (unsigned short) — cai no byte alto de si[5]? */
        }
        ret = 0;
        break;
    }

    /* ~~ Linux getppid (110) ~ "Meu pai é o quê?" ~~
     * O PCB guarda parent_pid, é só devolver~ */
    case SYS_getppid: {
        pcb_t *p = process_current();
        ret = p ? p->parent_pid : 1;
        break;
    }

    /* ~~ Linux getpgid (121) ~ "Meu grupo?" ~~
     * Sem grupos de processo de verdade, cada um é grupo de si~ */
    case SYS_getpgid: {
        pcb_t *p = process_current();
        ret = p ? p->pid : 0;
        break;
    }

    /* ~~ Linux umask (95) ~ "Máscara de permissão!" ~~
     * Guarda e devolve a anterior. Fácil~ */
    case SYS_umask: {
        static int cur_umask = 0x1FF; /* 0777 — sem restrição */
        int old = cur_umask;
        cur_umask = (int)a1 & 0x1FF;
        ret = old;
        break;
    }

    /* ~~ Linux fcntl (72) ~ "Configura fd!" ~~
     * Mínimo: F_GETFD (1), F_SETFD (2), F_GETFL (3), F_SETFL (4).
     * F_SETFL O_NONBLOCK é o que weston pede no stdin~
     * (código 0x800 no Linux x86_64) */
    case SYS_fcntl: {
        int fd = (int)a1;
        int cmd = (int)a2;
        int arg = (int)a3;
        if (fd < 0 || fd >= MAX_FDS) { ret = -1; break; }
        switch (cmd) {
        case 1: /* F_GETFD */ ret = fds[fd].flags & O_CLOEXEC ? 1 : 0; break;
        case 2: /* F_SETFD */ fds[fd].flags = (fds[fd].flags & ~O_CLOEXEC) | (arg & O_CLOEXEC); ret = 0; break;
        case 3: /* F_GETFL */ ret = fds[fd].flags; break;
        case 4: /* F_SETFL */ fds[fd].flags = arg; ret = 0; break;
        default: ret = -1; break;
        }
        break;
    }

    /* ~~ Linux dup (32) ~ "Clona um fd!" ~~
     * Procura o menor fd livre e copia o apontamento~ */
    case SYS_dup: {
        int oldfd = (int)a1;
        if (oldfd < 0 || oldfd >= MAX_FDS || (!fds[oldfd].used && oldfd >= 3)) { ret = -1; break; }
        int nfd = alloc_fd();
        if (nfd < 0) { ret = -1; break; }
        fds[nfd] = fds[oldfd];
        ret = nfd;
        break;
    }

    /* ~~ Linux dup2 (91) ~ "Duplica num fd específico!" ~~
     * Se newfd já tá aberto, fecha antes~ (Linux semantics) */
    case SYS_dup2: {
        int oldfd = (int)a1;
        int newfd = (int)a2;
        if (oldfd < 0 || oldfd >= MAX_FDS || (!fds[oldfd].used && oldfd >= 3)) { ret = -1; break; }
        if (newfd < 0 || newfd >= MAX_FDS) { ret = -1; break; }
        if (newfd >= 3 && fds[newfd].used) {
            if (fds[newfd].type == 3) {
                int p = fds[newfd].pipe_idx;
                if (p >= 0 && p < MAX_PIPES) pipes[p].used = 0;
            }
            fds[newfd].used = 0;
        }
        fds[newfd] = fds[oldfd];
        ret = newfd;
        break;
    }

    /* ~~ Linux dup3 (292) ~ "dup2 + O_CLOEXEC!" ~~
     * Mesma coisa do dup2, guarda o flag de CLOEXEC~ */
    case SYS_dup3: {
        int oldfd = (int)a1;
        int newfd = (int)a2;
        int flags = (int)a3;
        if (oldfd < 0 || oldfd >= MAX_FDS || (!fds[oldfd].used && oldfd >= 3)) { ret = -1; break; }
        if (newfd < 0 || newfd >= MAX_FDS) { ret = -1; break; }
        if (newfd >= 3 && fds[newfd].used) {
            if (fds[newfd].type == 3) {
                int p = fds[newfd].pipe_idx;
                if (p >= 0 && p < MAX_PIPES) pipes[p].used = 0;
            }
            fds[newfd].used = 0;
        }
        fds[newfd] = fds[oldfd];
        fds[newfd].flags = flags;
        ret = newfd;
        break;
    }

    /* ~~ Linux pipe (22) ~ "Um canudinho pra conversar!" ~~
     * Cria um par de fds (leitura/escrita) com buffer compartilhado~ */
    case SYS_pipe: {
        int *out = (int *)a1;
        if (!out) { ret = -1; break; }
        int pi = -1;
        for (int i = 0; i < MAX_PIPES; i++) if (!pipes[i].used) { pi = i; break; }
        if (pi < 0) { ret = -1; break; }
        int rfd = alloc_fd();
        int wfd = alloc_fd();
        if (rfd < 0 || wfd < 0) { ret = -1; break; }
        pipes[pi].used = 1;
        pipes[pi].rpos = pipes[pi].wpos = 0;
        for (int i = 0; i < 4096; i++) pipes[pi].buf[i] = 0;
        fds[rfd].used = 1; fds[rfd].type = 3; fds[rfd].pipe_idx = pi; fds[rfd].flags = 0;
        fds[wfd].used = 1; fds[wfd].type = 3; fds[wfd].pipe_idx = pi; fds[wfd].flags = 0;
        out[0] = rfd;
        out[1] = wfd;
        ret = 0;
        break;
    }

    /* ~~ Linux pipe2 (293) ~ "pipe + O_NONBLOCK/O_CLOEXEC!" ~~
     * Mesma coisa do pipe, aplica os flags~ */
    case SYS_pipe2: {
        int *out = (int *)a1;
        int flags = (int)a2;
        if (!out) { ret = -1; break; }
        int pi = -1;
        for (int i = 0; i < MAX_PIPES; i++) if (!pipes[i].used) { pi = i; break; }
        if (pi < 0) { ret = -1; break; }
        int rfd = alloc_fd();
        int wfd = alloc_fd();
        if (rfd < 0 || wfd < 0) { ret = -1; break; }
        pipes[pi].used = 1;
        pipes[pi].rpos = pipes[pi].wpos = 0;
        for (int i = 0; i < 4096; i++) pipes[pi].buf[i] = 0;
        fds[rfd].used = 1; fds[rfd].type = 3; fds[rfd].pipe_idx = pi; fds[rfd].flags = flags;
        fds[wfd].used = 1; fds[wfd].type = 3; fds[wfd].pipe_idx = pi; fds[wfd].flags = flags;
        out[0] = rfd;
        out[1] = wfd;
        ret = 0;
        break;
    }

    /* ~~ sockets AF_UNIX (issue #49) ~~
     * A base pro wayland/X11: eles conversam por unix socket
     * (/run/wayland-0, /tmp/.X11-unix/X0). A implementação é um
     * "socket em memória": cada socket tem um buffer (como um pipe)
     * e um peer. write grava no buffer do peer, read lê o próprio.
     * SCM_RIGHTS (passar fd) é aceito mas não passa fd de verdade
     * (fds são por-processo — num kernel single-user nem precisamos). */

    case SYS_socket: {
        int domain = (int)a1;
        int type = (int)a2;
        int proto = (int)a3;
        (void)proto;
        if (domain != AF_UNIX) { ret = -1; break; }
        int s = sock_alloc();
        if (s < 0) { ret = -1; break; }
        int fd = alloc_fd();
        if (fd < 0) { socks[s].used = 0; ret = -1; break; }
        socks[s].used = 1;
        socks[s].listening = 0;
        socks[s].peer = -1;
        socks[s].nonblock = 0;
        socks[s].npending = 0;
        socks[s].rpos = socks[s].wpos = 0;
        socks[s].path[0] = '\0';
        fds[fd].used = 1;
        fds[fd].type = 4;
        fds[fd].sock_idx = s;
        fds[fd].flags = (type & O_NONBLOCK) ? O_NONBLOCK : 0;
        ret = fd;
        break;
    }

    case SYS_bind: {
        int fd = (int)a1;
        const char *path = sock_path_from_addr(a2, a3);
        if (fd < 3 || fd >= MAX_FDS || !fds[fd].used || fds[fd].type != 4 || !path) {
            ret = -1; break;
        }
        int s = fds[fd].sock_idx;
        if (s < 0 || s >= MAX_SOCKS) { ret = -1; break; }
        int i = 0;
        while (path[i] && i < 255) { socks[s].path[i] = path[i]; i++; }
        socks[s].path[i] = '\0';
        ret = 0;
        break;
    }

    case SYS_listen: {
        int fd = (int)a1;
        int backlog = (int)a2;
        (void)backlog;
        if (fd < 3 || fd >= MAX_FDS || !fds[fd].used || fds[fd].type != 4) {
            ret = -1; break;
        }
        int s = fds[fd].sock_idx;
        if (s >= 0 && s < MAX_SOCKS) socks[s].listening = 1;
        ret = 0;
        break;
    }

    case SYS_connect: {
        int fd = (int)a1;
        const char *path = sock_path_from_addr(a2, a3);
        if (fd < 3 || fd >= MAX_FDS || !fds[fd].used || fds[fd].type != 4 || !path) {
            ret = -1; break;
        }
        int s = fds[fd].sock_idx;
        int srv = sock_by_path(path);
        if (s < 0 || s >= MAX_SOCKS || srv < 0) { ret = -1; break; }
        if (socks[srv].npending >= SOCK_QUEUE) { ret = -1; break; }
        socks[s].peer = srv;
        socks[srv].pending[socks[srv].npending++] = s;
        ret = 0;
        break;
    }

    case SYS_accept4:
    case SYS_accept: {
        int fd = (int)a1;
        if (fd < 3 || fd >= MAX_FDS || !fds[fd].used || fds[fd].type != 4) {
            ret = -1; break;
        }
        int s = fds[fd].sock_idx;
        if (s < 0 || s >= MAX_SOCKS || socks[s].npending <= 0) { ret = -1; break; }
        /* tira o primeiro cliente da fila */
        int cli = socks[s].pending[0];
        for (int i = 1; i < socks[s].npending; i++) socks[s].pending[i - 1] = socks[s].pending[i];
        socks[s].npending--;
        /* cria fd novo pro lado do servidor, conectado ao cliente */
        int nfd = alloc_fd();
        if (nfd < 0) { ret = -1; break; }
        int ns = sock_alloc();
        if (ns < 0) { ret = -1; break; }
        socks[ns].used = 1;
        socks[ns].listening = 0;
        socks[ns].peer = cli;
        socks[ns].nonblock = 0;
        socks[ns].npending = 0;
        socks[ns].rpos = socks[ns].wpos = 0;
        socks[ns].path[0] = '\0';
        socks[cli].peer = ns; /* cliente agora fala com o servidor */
        fds[nfd].used = 1;
        fds[nfd].type = 4;
        fds[nfd].sock_idx = ns;
        fds[nfd].flags = (num == SYS_accept4 && (a4 & O_NONBLOCK)) ? O_NONBLOCK : 0;
        ret = nfd;
        break;
    }

    case SYS_sendto:
    case SYS_sendmsg: {
        int fd = (int)a1;
        const char *buf = (const char *)a2;
        int count = (int)a3;
        if (fd < 3 || fd >= MAX_FDS || !fds[fd].used || fds[fd].type != 4) {
            ret = -1; break;
        }
        int s = fds[fd].sock_idx;
        if (s < 0 || s >= MAX_SOCKS) { ret = -1; break; }
        int peer = socks[s].peer;
        ret = sock_push(peer, (const uint8_t *)buf, count);
        break;
    }

    case SYS_recvfrom:
    case SYS_recvmsg: {
        int fd = (int)a1;
        char *buf = (char *)a2;
        int count = (int)a3;
        if (fd < 3 || fd >= MAX_FDS || !fds[fd].used || fds[fd].type != 4) {
            ret = -1; break;
        }
        int s = fds[fd].sock_idx;
        if (s < 0 || s >= MAX_SOCKS) { ret = -1; break; }
        ret = sock_pop(s, (uint8_t *)buf, count);
        break;
    }

    case SYS_socketpair: {
        int domain = (int)a1;
        int type = (int)a2;
        int *out = (int *)(uintptr_t)a4;
        (void)type;
        if (domain != AF_UNIX || !out) { ret = -1; break; }
        int s1 = sock_alloc();
        int s2 = sock_alloc();
        int fd1 = alloc_fd();
        int fd2 = alloc_fd();
        if (s1 < 0 || s2 < 0 || fd1 < 0 || fd2 < 0) { ret = -1; break; }
        socks[s1].used = 1; socks[s1].listening = 0; socks[s1].peer = s2;
        socks[s1].nonblock = 0; socks[s1].npending = 0;
        socks[s1].rpos = socks[s1].wpos = 0; socks[s1].path[0] = '\0';
        socks[s2].used = 1; socks[s2].listening = 0; socks[s2].peer = s1;
        socks[s2].nonblock = 0; socks[s2].npending = 0;
        socks[s2].rpos = socks[s2].wpos = 0; socks[s2].path[0] = '\0';
        fds[fd1].used = 1; fds[fd1].type = 4; fds[fd1].sock_idx = s1; fds[fd1].flags = 0;
        fds[fd2].used = 1; fds[fd2].type = 4; fds[fd2].sock_idx = s2; fds[fd2].flags = 0;
        out[0] = fd1;
        out[1] = fd2;
        ret = 0;
        break;
    }

    case SYS_shutdown: {
        int fd = (int)a1;
        (void)fd;
        /* só aceita e ignora — sem conexão de verdade pra derrubar~ */
        ret = 0;
        break;
    }

    case SYS_getsockname: {
        int fd = (int)a1;
        if (fd < 3 || fd >= MAX_FDS || !fds[fd].used || fds[fd].type != 4) {
            ret = -1; break;
        }
        int s = fds[fd].sock_idx;
        /* devolve sockaddr_un { AF_UNIX, path } no buffer do caller */
        uint64_t addr = a2;
        if (addr && s >= 0 && s < MAX_SOCKS) {
            char *dst = (char *)(uintptr_t)addr;
            dst[0] = AF_UNIX;
            dst[1] = 0;
            int i = 0;
            while (socks[s].path[i] && i < 254) { dst[2 + i] = socks[s].path[i]; i++; }
            dst[2 + i] = '\0';
        }
        ret = 0;
        break;
    }

    default:
        ret = -1;
        break;
    }

    regs[0] = ret;
}

/* ♥ syscall.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
