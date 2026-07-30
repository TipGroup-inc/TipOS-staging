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
    char name[256];
    uint32_t pos;
} fds[MAX_FDS];

struct timeval { uint64_t tv_sec; uint64_t tv_usec; };
struct stat   { uint32_t st_size; };

/* ~ cuidado que essa aqui morde ~ */
void syscall_init(void) {
    for (int i = 0; i < MAX_FDS; i++) fds[i].used = 0;
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
    if (fd >= 3 && fd < MAX_FDS) fds[fd].used = 0;
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
        ret = 0;
        break;

    case SYS_mprotect:
        ret = 0;
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

    case SYS_ioctl:
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
        void *entry = mach_o_load_into_pml4(buf, fsize, child_pml4);
        kfree(buf);
        if (!entry) { ret = -1; break; }
        void *user_stack = kmalloc(65536);
        if (!user_stack) { ret = -1; break; }
        int pid = proc_spawn(fname, entry, (uint8_t *)user_stack + 65536);
        if (pid < 0) { kfree(user_stack); ret = -1; break; }
        for (int i = 0; i < MAX_PROC; i++) {
            if (pcb_table[i].pid == pid) {
                pml4_destroy(pcb_table[i].pml4);
                pcb_table[i].pml4 = child_pml4;
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
        /* ~~ Cria PML4 independente (copia identidade), carrega TERM,
         * cria processo (com PML4 do kernel), troca PML4 no PCB ~~
         * clone_identity_tables() cria PML4+PDP+PD novos com copia
         * das entradas identitarias. mach_o_load_into_pml4() modifica
         * PD[128] no PD novo — nao afeta o kernel/DISP!
         * Depois proc_spawn() cria o processo normal, e trocamos a
         * PML4 do PCB pela independente. O entry point ja vai certo
         * no frame iretq porque passamos entry pro proc_spawn. */
        uint64_t child_pml4 = clone_identity_tables();
        if (!child_pml4) { kfree(buf); serial_puts("spawn: clone_pml4 falhou\n"); ret = -1; break; }
        serial_puts("spawn: child_pml4="); serial_puthex((uint32_t)child_pml4); serial_puts("\n");
        void *entry = mach_o_load_into_pml4(buf, fsize, child_pml4);
        kfree(buf);
        if (!entry) { serial_puts("spawn: load_into_pml4 falhou\n"); ret = -1; break; }
        serial_puts("spawn: entry="); serial_puthex((uint32_t)(uintptr_t)entry); serial_puts("\n");
        uint8_t *user_stack = kmalloc(65536);
        if (!user_stack) { serial_puts("spawn: sem stack\n"); ret = -1; break; }
        /* ~~ Adiciona U/S seletivamente ~~
         * clone_identity_tables() limpou o U/S de todas as entradas.
         * Agora restauramos apenas para as paginas que o filho precisa:
         * - framebuffer (0x500000, PD[2]) via pml4_add_user
         * - stack do usuario (split PD[5] em paginas 4KB, add U/S no range)
         * - buffers compartilhados (0x6000000 PD[48], 0xE00000 PD[7])
         * - codigo (0x10000000, PD[128]) via mach_o_load_into_pml4
         * Nao damos U/S pra PD[0] ou PD[5] inteira pra evitar que o
         * filho escreva nos stacks/PCB do kernel ou do pai. */
        pml4_add_user(child_pml4, 0x500000);
        pml4_add_user(child_pml4, 0xA00000);
        pml4_add_user(child_pml4, 0x6000000);
        pml4_add_user(child_pml4, 0xE00000);
        int pid = proc_spawn(fname, entry, user_stack + 65536);
        if (pid < 0) { kfree(user_stack); serial_puts("spawn: proc_spawn falhou\n"); ret = -1; break; }
        serial_puts("spawn: pid="); serial_puthex((uint32_t)pid); serial_puts("\n");
        /* ~~ Troca a PML4 que o proc_spawn criou pela nossa ~~ */
        for (int i = 0; i < MAX_PROC; i++) {
            if (pcb_table[i].pid == pid) {
                pml4_destroy(pcb_table[i].pml4);
                pcb_table[i].pml4 = child_pml4;
                break;
            }
        }
        ret = pid;
        break;
    }

    default:
        ret = -1;
        break;
    }

    regs[0] = ret;
}

/* ♥ syscall.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
