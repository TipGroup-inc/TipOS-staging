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

#define MAX_FDS 16
static struct {
    int used;
    int type;          /* 0=arquivo, 1=eventfd, 2=timerfd, 3=pipe */
    int flags;         /* O_NONBLOCK etc (fcntl) */
    int pipe_idx;      /* pipe: indice na tabela de pipes */
    char name[256];
    uint32_t pos;
    uint64_t counter;  /* eventfd: contador; timerfd: intervalo (ms) */
} fds[MAX_FDS];

/* ~~ pipes (issue #52) ~~ um buffer por pipe, read/write pos separados */
#define MAX_PIPES 8
static struct {
    int used;
    uint8_t buf[4096];
    uint32_t rpos, wpos;
} pipes[MAX_PIPES];

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

/* ~ cuidado que essa aqui morde ~ */
void syscall_init(void) {
    for (int i = 0; i < MAX_FDS; i++) fds[i].used = 0;
    for (int i = 0; i < MAX_PIPES; i++) pipes[i].used = 0;
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

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void close_fd(int fd) {
    if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
        if (fds[fd].type == 3) {
            int p = fds[fd].pipe_idx;
            if (p >= 0 && p < MAX_PIPES) pipes[p].used = 0;
        }
        fds[fd].used = 0;
    }
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int str_equal(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
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
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 1) {
            /* eventfd write: soma u64 no contador */
            if (count >= 8) {
                uint64_t val = 0;
                for (int i = 0; i < 8; i++) val |= (uint64_t)(uint8_t)buf[i] << (8 * i);
                fds[fd].counter += val;
            }
            ret = count;
            break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used && fds[fd].type == 3) {
            /* pipe write: copia pro buffer compartilhado */
            int p = fds[fd].pipe_idx;
            if (p < 0 || p >= MAX_PIPES || !pipes[p].used) { ret = -1; break; }
            if (pipes[p].wpos - pipes[p].rpos >= 4096) {
                if (fds[fd].flags & O_NONBLOCK) ret = (uint64_t)-11;
                else ret = -1;
                break;
            }
            int n = 0;
            while (n < count && pipes[p].wpos - pipes[p].rpos < 4096) {
                pipes[p].buf[pipes[p].wpos & 4095] = (uint8_t)buf[n++];
                pipes[p].wpos++;
            }
            ret = n;
            break;
        }
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
            int r = fat32_write_file(fds[fd].name, (const uint8_t *)buf, count);
            ret = (r >= 0) ? count : -1;
            break;
        }
        if (fd == 1 || fd == 2) {
            for (int i = 0; i < count; i++) console_putchar(buf[i]);
            ret = count;
            break;
        }
        ret = -1;
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
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
            static uint8_t tmp[4096];
            int r = fat32_read_file(fds[fd].name, tmp, 4096);
            if (r < 0) { ret = -1; break; }
            int off = fds[fd].pos;
            int n = (count < r - off) ? count : (r - off);
            if (n < 0) { ret = 0; break; }
            for (int i = 0; i < n; i++) buf[i] = tmp[off + i];
            fds[fd].pos += n;
            ret = n;
            break;
        }
        ret = -1;
        break;
    }

    case SYS_open: {
        const char *path = (const char *)a1;
        if (str_equal(path, "/dev/tty") || str_equal(path, "/dev/stdin")) { ret = 0; break; }
        if (str_equal(path, "/dev/stdout")) { ret = 1; break; }
        if (str_equal(path, "/dev/stderr")) { ret = 2; break; }
        int fd = alloc_fd();
        if (fd < 0) { ret = -1; break; }
        int i = 0;
        while (path[i] && i < 255) { fds[fd].name[i] = path[i]; i++; }
        fds[fd].name[i] = '\0';
        fds[fd].pos = 0;
        fds[fd].used = 1;
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
        if (fd >= 3 && fd < MAX_FDS && fds[fd].used) {
            uint32_t size; uint8_t attr;
            if (fat32_stat(fds[fd].name, &size, &attr, NULL, NULL) == 0) {
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
        if (!path || !st) { ret = -1; break; }
        uint32_t size; uint8_t attr;
        if (fat32_stat(path, &size, &attr, NULL, NULL) == 0) {
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
                if (fat32_stat(fds[fd].name, &size, &attr, NULL, NULL) == 0)
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
        if (fat32_mkdir((const char *)a1) == 0) ret = 0;
        break;

    case SYS_rmdir2:
        if (fat32_rmdir((const char *)a1) == 0) ret = 0;
        break;

    case SYS_mmap: {
        uint64_t hint = a1;
        uint64_t len = a2;
        int prot = (int)a3;
        int flags = (int)a4;
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

        /* Simplificação: só fd 0 (stdin) é monitorável */
        int ready = 0;
        if (rd_set && keyboard_avail()) {
            rd_set[0] |= 1;
            ready++;
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
        int is_elf = (fsize >= 4 && buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F');
        uint64_t elf_phdr = 0, elf_phent = 0, elf_phnum = 0;
        void *entry;
        if (is_elf) {
            Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buf;
            elf_phent = ehdr->e_phentsize;
            elf_phnum = ehdr->e_phnum;
            /* ~~ Calcula o endereco virtual da tabela PHDR ~~
             * Primeiro PT_LOAD que achamos, a gente usa de base~ */
            if (elf_phnum) {
                Elf64_Phdr *pp = (Elf64_Phdr *)(buf + ehdr->e_phoff);
                for (uint32_t k = 0; k < elf_phnum; k++) {
                    if (pp->p_type == PT_LOAD) {
                        elf_phdr = pp->p_vaddr + (ehdr->e_phoff - pp->p_offset);
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
        if (!entry) { ret = -1; break; }
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
                    uint64_t *kframe = (uint64_t *)pcb_table[i].kernel_rsp;
                    uint64_t old_rsp = kframe[18];
                    kframe[18] = setup_linux_user_stack(&pcb_table[i], old_rsp,
                                                           elf_phdr, elf_phent, elf_phnum);
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
        void *entry;
        if (is_elf) {
            Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buf;
            elf_phent = ehdr->e_phentsize;
            elf_phnum = ehdr->e_phnum;
            if (elf_phnum) {
                Elf64_Phdr *pp = (Elf64_Phdr *)(buf + ehdr->e_phoff);
                for (uint32_t k = 0; k < elf_phnum; k++) {
                    if (pp->p_type == PT_LOAD) {
                        elf_phdr = pp->p_vaddr + (ehdr->e_phoff - pp->p_offset);
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
                                                          elf_phdr, elf_phent, elf_phnum);
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
            p->program_break = a1;
            ret = p->program_break;
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

    /* ~~ Linux set_tid_address (258) ~ "Aqui, guarda meu TID!" ~~
     * Musl passa o endereço de uma variável onde o kernel
     * deveria escrever o TID quando a thread morre~
     * Mas como somos um kernel ~fofo~ e sem threads de verdade,
     * a gente só devolve o PID e ignora o endereço~ */
    case SYS_set_tid_address:
        ret = process_current_pid();
        break;

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

    default:
        ret = -1;
        break;
    }

    regs[0] = ret;
}

/* ♥ syscall.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
