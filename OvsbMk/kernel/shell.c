/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: shell.c ~ funcoes anotadas: 17
 */
/* ♥ shell.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/


/* ♥ SHELL ~ console interativo! comandos, exec, ls, cd~ tudo aqui! ♥
 * Aqui é onde o usuario digita comandos e o kernel obedece (quando quer).
 * O cmd_exec carrega binarios Mach-O do FAT32 e cria processos pra rodar.
 * 
 * Durante o debug, adicionei uns dumps:
 * - Dump do codigo em 0x10001D20 (onde o DISP crashava)
 * - Dump de scr_w e next_cascade logo apos carregar o DISP
 * Isso ajudou a descobrir que scr_w tava sendo zerado entre o load e o crash
 * (spoiler: era a syscall number errada no libdisp.h). kyun~ <3 */
#include "shell.h"
#include "console.h"
#include "memory.h"
#include "mach_o.h"
#include "process.h"
extern int g_use_ext2;
extern int vfs_stat_size(const char *name, uint32_t *size, uint8_t *attr);
extern int fs_read_file(const char *name, unsigned char *buf, unsigned int count);
#include "../fs/vfs.h"
#include "serial.h"
#include "../lib/gui/vesa.h"
#include "../fs/fat32.h"
#include <stdint.h>
#include "../drivers/e1000.h"

/* ~ cuidado que essa aqui morde ~ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static int strieq(const char *a, const char *b, int n);

#define MAX_CMD 256

static char cmd_buf[MAX_CMD];
static int  cmd_pos = 0;

extern framebuffer_t g_fb;
extern void owt_demo(void);
extern void *elf64_load_into_pml4(const uint8_t *data, uint32_t len, uint64_t pml4);
extern uint64_t elf64_pie_base(void);

/* ELF64 header structs for Linux ABI */
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

/* ~ cuidado que essa aqui morde ~ */
static void prompt(void) {
    console_write("ovsb> ");
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void cmd_help(void) {
    console_write("Comandos:\n");
    console_write("  help                  Mostra esta ajuda\n");
    console_write("  clear                 Limpa a tela\n");
    console_write("  echo <texto>          Imprime o texto\n");
    console_write("  info                  Info do sistema (VESA, heap)\n");
    console_write("  hexdump <addr>        Exibe 64 bytes do endereco\n");
    console_write("  run                   Executa programa ring 3 (embutido)\n");
    console_write("  exec <arquivo>        Carrega e executa Mach-O do FAT32\n");
    console_write("  ls                    Lista diretorio FAT32\n");
    console_write("  cd <dir>              Muda diretorio FAT32\n");
    console_write("  owt                   Demo do OWT (widget toolkit)\n");
    console_write("  reboot                Reinicia o sistema\n");
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void cmd_clear(void) {
    console_clear();
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void cmd_echo(const char *args) {
    if (args) console_write(args);
    console_write("\n");
}

/* ~ cuidado que essa aqui morde ~ */
static void cmd_info(void) {
    console_printf("VESA: %dx%d %dbpp\n", g_fb.width, g_fb.height, g_fb.bpp);
    console_printf("Framebuffer: 0x%x\n", (unsigned int)g_fb.addr);
    console_printf("Pitch: %d\n", g_fb.pitch);
    console_printf("Heap: 64MB bump allocator\n");
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void cmd_hexdump(const char *args) {
    if (!args || !args[0]) { console_write("uso: hexdump <addr_hex>\n"); return; }
    unsigned int addr = 0;
    for (int i = 0; args[i] && args[i] != ' '; i++) {
        char c = args[i];
        if (c >= '0' && c <= '9') addr = addr * 16 + (c - '0');
        else if (c >= 'a' && c <= 'f') addr = addr * 16 + (c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') addr = addr * 16 + (c - 'A' + 10);
        else break;
    }
    uint8_t *p = (uint8_t *)(uintptr_t)addr;
    for (int row = 0; row < 4; row++) {
        console_printf("%x: ", addr + row * 16);
        for (int i = 0; i < 16; i++) {
            char tmp[8];
            unsigned char b = p[row * 16 + i];
            tmp[0] = "0123456789abcdef"[b >> 4];
            tmp[1] = "0123456789abcdef"[b & 0xF];
            tmp[2] = ' ';
            tmp[3] = 0;
            console_write(tmp);
        }
        console_write(" |");
        for (int i = 0; i < 16; i++) {
            unsigned char b = p[row * 16 + i];
            char ch = (b >= 32 && b < 127) ? b : '.';
            console_putchar(ch);
        }
        console_write("|\n");
    }
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
#define EXEC_USER_STACK_SIZE 65536

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
static void cmd_exec(const char *args) {
    if (!args || !args[0]) {
        console_write("uso: exec <arquivo> [args...]\n");
        return;
    }
    /* ~~ Separa o nome do arquivo dos argumentos ~~
     * argv[0] = binario, argv[1..] = args pro ELF (ex: exec XORG XORG.CONF)~ */
    char argv_bufs[8][64];
    char *argv[9];
    int argc = 0;
    const char *p = args;
    while (*p && argc < 8) {
        while (*p == ' ') p++;
        if (!*p) break;
        int len = 0;
        while (p[len] && p[len] != ' ') len++;
        int n = len < 63 ? len : 63;
        for (int i = 0; i < n; i++) argv_bufs[argc][i] = p[i];
        argv_bufs[argc][n] = '\0';
        argv[argc] = argv_bufs[argc];
        argc++;
        p += len;
    }
    argv[argc] = NULL;
    /* ~~ Background: "exec FOO &" nao trava o shell!~~
     * O Xorg roda em background enquanto a gente digita fvwm~ */
    int bg = 0;
    if (argc > 0 && argv[argc - 1][0] == '&' && argv[argc - 1][1] == '\0') {
        bg = 1;
        argc--;
        argv[argc] = NULL;
        if (argc == 0) { console_write("uso: exec <arquivo> [args...] &\n"); return; }
    }
    const char *fname = argv[0];
    uint32_t fsize;
    uint8_t attr;
    /* ~~ PATH-like: nome relativo procura em /bin~~ */
    char fbuf[128];
    if (fname[0] != '/') {
        int fi = 0;
        fbuf[0] = '/'; fbuf[1] = 'b'; fbuf[2] = 'i'; fbuf[3] = 'n'; fbuf[4] = '/';
        while (fname[fi] && fi < 120) { fbuf[5 + fi] = fname[fi]; fi++; }
        fbuf[5 + fi] = '\0';
        fname = fbuf;
    }
    char absp[VFS_MAX_PATH]; /* ~~ TEM que viver até o fim da função~~ */
    {
        pcb_t *me = process_current();
        char tmpcwd[VFS_MAX_PATH];
        const char *fcopy = fname;
        int ci = 0;
        while (fcopy[ci] && ci < VFS_MAX_PATH - 1) { tmpcwd[ci] = fcopy[ci]; ci++; }
        tmpcwd[ci] = '\0';
        if (vfs_abs_path(me ? me->cwd : "/", tmpcwd, absp, sizeof(absp)) == 0)
            fname = absp;
    }
    if (vfs_stat_size(fname, &fsize, &attr) < 0) {
        console_write("exec: arquivo nao encontrado\n");
        return;
    }
    if (fsize < 32) {
        console_write("exec: arquivo muito pequeno\n");
        return;
    }
    uint8_t *buf = kmalloc(fsize + 1);
    if (!buf) {
        console_write("exec: sem memoria\n");
        return;
    }
    if (vfs_read_file(fname, buf, fsize) < 0) {
        console_write("exec: erro de leitura\n");
        kfree(buf);
        return;
    }
    console_printf("exec: carregando %s (%d bytes)\n", fname, (unsigned)fsize);

    /* Detect ELF vs Mach-O */
    int is_elf = (fsize >= 4 && buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F');

    if (is_elf) {
        /* ~~ ELF binary: o novo queridinho do pedaço! ~~
         * Clona a PML4, carrega o binario, monta a pilha Linux~
         * e cria o processo com proc_spawn (a API nova!) */
        uint64_t child_pml4 = clone_identity_tables();
        if (!child_pml4) { kfree(buf); console_write("exec: pml4 falhou\n"); return; }

        Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buf;
        uint64_t elf_base = (ehdr->e_type == 3) ? elf64_pie_base() : 0; /* ET_DYN = PIE */
        uint64_t elf_phdr = 0, elf_phent = ehdr->e_phentsize, elf_phnum = ehdr->e_phnum;
        uint64_t elf_bss_end = 0;
        if (elf_phnum) {
            Elf64_Phdr *pp = (Elf64_Phdr *)(buf + ehdr->e_phoff);
            for (uint32_t k = 0; k < elf_phnum; k++) {
                if (pp->p_type == PT_LOAD) {
                    /* Guarda o primeiro PHDR pra por no auxv~ */
                    if (elf_phdr == 0)
                        elf_phdr = elf_base + pp->p_vaddr + (ehdr->e_phoff - pp->p_offset);
                    uint64_t seg_end = pp->p_vaddr + pp->p_memsz;
                    uint64_t a_start = (elf_base + pp->p_vaddr) & ~(0x1FFFFFULL);
                    uint64_t a_end = (elf_base + seg_end + 0x1FFFFF) & ~(0x1FFFFFULL);
                    /* ~~ Fim do BSS = program_break inicial do processo ~~
                     * (o musl chama brk(0) pra saber onde o heap começa) */
                    if (elf_base + seg_end > elf_bss_end)
                        elf_bss_end = elf_base + seg_end;
                    /* ~~ Libera o U/S pros segmentos do ELF ~~
                     * clone_identity_tables tirou o bit de usuario~
                     * A gente devolve pros paginas que o programa
                     * precisa acessar~ */
                    for (uint64_t va = a_start; va < a_end; va += 0x200000)
                        pml4_add_user(child_pml4, va);
                }
                pp = (Elf64_Phdr *)((uint8_t *)pp + elf_phent);
            }
            elf_bss_end = (elf_bss_end + 0xFFF) & ~0xFFFULL;
        }

        void *entry = elf64_load_into_pml4(buf, fsize, child_pml4);
        kfree(buf);
        if (!entry) { console_write("exec: ELF invalido\n"); return; }
        console_printf("exec: entry=%x\n", (unsigned int)(uint64_t)entry);

        /* ~~ Aloca pilha do usuario ~~
         * O kmalloc pega do heap (0xA00000+)~
         * Depois a gente libera o U/S pro processo acessar~ */
        void *ustack = kmalloc(EXEC_USER_STACK_SIZE);
        if (!ustack) { console_write("exec: sem stack\n"); return; }
        uint64_t stack_addr = (uint64_t)ustack;
        for (uint64_t va = stack_addr & ~0x1FFFFFULL;
             va < stack_addr + EXEC_USER_STACK_SIZE; va += 0x200000)
            pml4_add_user(child_pml4, va);

        int pid = proc_spawn(fname, entry, (uint8_t *)ustack + EXEC_USER_STACK_SIZE);
        if (pid < 0) { console_write("exec: spawn falhou\n"); kfree(ustack); return; }

        /* ~~ Troca a PML4 e monta o vetor auxiliar ~~
         * A PML4 vazia do proc_spawn é substituida pela
         * child_pml4 que tem o binario carregado~
         * E o setup_linux_user_stack escreve argc, argv,
         * envp, auxv na pilha do usuario~ (formato Linux!) */
        for (int i = 0; i < MAX_PROC; i++) {
            if (pcb_table[i].pid == pid) {
                pml4_destroy(pcb_table[i].pml4);
                pcb_table[i].pml4 = child_pml4;
                /* ~~ Program break começa no fim do BSS do ELF ~~ */
                if (elf_bss_end) pcb_table[i].program_break = elf_bss_end;
                uint64_t *kframe = (uint64_t *)pcb_table[i].kernel_rsp;
                /* ~~ Ambiente minimo: DISPLAY=:0 pro FVWM achar o
                 * servidor X~ (o musl le da area envp da pilha)~~ */
                static char *exec_env[] = { "DISPLAY=:0" };
                kframe[18] = setup_linux_user_stack(&pcb_table[i], kframe[18],
                                                     elf_phdr, elf_phent, elf_phnum,
                                                     elf_base, argv, argc,
                                                     exec_env, 0);
                break;
            }
        }

        console_printf("exec: PID %d rodando\n", pid);
        if (bg) {
            /* ~~ Background: o shell volta pro prompt NA HORA!~~
             * NAO da kfree(ustack) — o processo ta usando ela~
             * O yield salva o contexto ATUAL do shell no PCB dele~
             * (sem isso o scheduler restaurava um KRNL_RSP velho,
             *  de frame de exec antigo, e o shell voltava executando
             *  lixo no meio da pilha~ rssrsrs) */
            console_printf("exec: PID %d em background (&)\n", pid);
            {
                /* ~~ debug: estado do filho na hora do yield ~~ */
                for (int q = 0; q < MAX_PROC; q++)
                    if (pcb_table[q].pid == pid) {
                        serial_puts("[bg] slot=");
                        serial_puthex((uint32_t)q);
                        serial_puts(" state=");
                        serial_puthex((uint32_t)pcb_table[q].state);
                        serial_puts(" cur=");
                        serial_puthex((uint32_t)process_current_pid());
                        serial_puts("\r\n");
                        break;
                    }
            }
            proc_yield();
            return;
        }
        process_switch_to(pid);
        int code = -1;
        for (int i = 0; i < MAX_PROC; i++)
            if (pcb_table[i].pid == pid) { code = pcb_table[i].exit_code; break; }
        kfree(ustack);
        console_printf("exec: processo encerrou (exit code %d)\n", code);
        return;
    }

    /* ~~ Mach-O binary: existing path ~~ */
    void *entry = mach_o_load(buf, fsize);
    if (!entry) {
        console_write("exec: formato Mach-O invalido\n");
        kfree(buf);
        return;
    }
    void *ustack = kmalloc(EXEC_USER_STACK_SIZE);
    if (!ustack) {
        console_write("exec: sem memoria para pilha\n");
        kfree(buf);
        return;
    }
    serial_puts("exec: pages ok\r\n");
    console_printf("exec: entry=%x\n", (unsigned int)(uint64_t)entry);
    int pid = process_create_user(args, entry, ustack, EXEC_USER_STACK_SIZE);
    if (pid < 0) {
        console_write("exec: erro ao criar processo\n");
        kfree(buf);
        kfree(ustack);
        return;
    }
    kfree(buf);
    console_printf("exec: PID %d rodando\n", pid);
    process_switch_to(pid);
    int code = -1;
    for (int i = 0; i < MAX_PROC; i++)
        if (pcb_table[i].pid == pid) { code = pcb_table[i].exit_code; break; }
    kfree(ustack);
    console_printf("exec: processo encerrou (exit code %d)\n", code);
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void cmd_desktop(void) {
    cmd_exec("DESKTOP.BIN");
}

/* ~ essa demorou pra debugar, respeita ~ */
static void cmd_ls(void) {
    fat32_list_dir();
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static void cmd_cd(const char *args) {
    if (!args || !args[0]) {
        fat32_change_dir("..");
        return;
    }
    /* ~~ VFS #70: cwd POR PROCESSO~~ */
    {
        pcb_t *me = process_current();
        char newcwd[VFS_MAX_PATH];
        if (me && vfs_abs_path(me->cwd, args, newcwd, sizeof(newcwd)) == 0 &&
            vfs_chdir_isdir(newcwd)) {
            int ci = 0;
            while (newcwd[ci] && ci < 254) { me->cwd[ci] = newcwd[ci]; ci++; }
            me->cwd[ci] = '\0';
        }
    }
    char cwd[256];
    fat32_get_cwd_name(cwd, 256);
    console_write(cwd);
    console_write("\n");
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static void cmd_reboot(void) {
    console_write("Reiniciando...\n");
    outb(0x64, 0xFE);
    for(;;);
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
static char *trim(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') text++;
    char *end = text;
    while (*end) end++;
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = 0;
    return text;
}

static void execute_one(char *cmd) {
    cmd = trim(cmd);
    if (!*cmd) return;

    char *args = cmd;
    while (*args && *args != ' ' && *args != '\t') args++;
    int cmd_len = args - cmd;
    while (*args == ' ' || *args == '\t') args++;

    if (strieq(cmd, "help", 4) && cmd_len == 4) cmd_help();
    else if (strieq(cmd, "clear", 5) && cmd_len == 5) cmd_clear();
    else if (strieq(cmd, "echo", 4) && cmd_len == 4) cmd_echo(args);
    else if (strieq(cmd, "info", 4) && cmd_len == 4) cmd_info();
    else if (strieq(cmd, "hexdump", 7) && cmd_len == 7) cmd_hexdump(args);
    else if (strieq(cmd, "exec", 4) && cmd_len == 4) cmd_exec(args);
    else if (strieq(cmd, "ls", 2) && cmd_len == 2) cmd_ls();
    else if (strieq(cmd, "cd", 2) && cmd_len == 2) cmd_cd(args);
        else if (strieq(cmd, "desktop", 7) && cmd_len == 7) cmd_desktop();
    else if (strieq(cmd, "owt", 3) && cmd_len == 3) { owt_demo(); console_write("OWT ok\n"); }
    else if (strieq(cmd, "sendpacket", 10) && cmd_len == 10) {
        uint8_t frame[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0x52,0x54,0x00,0x12,0x34,0x56, 0x08,0x06, 0x00,0x01,0x08,0x00,0x06,0x04,0x00,0x01, 0x52,0x54,0x00,0x12,0x34,0x56, 0x0A,0x00,0x02,0x02, 0x00,0x00,0x00,0x00,0x00,0x00, 0x0A,0x00,0x02,0x01};
        e1000_send(frame, sizeof(frame));
        console_write("Pacote enviado!\n");
    }
    else if (strieq(cmd, "reboot", 6) && cmd_len == 6) cmd_reboot();
    else {
        /* ~~ Fallback: comando desconhecido = tenta executar o arquivo!~~
         * "fvwm" no PATH (cwd) vira exec fvwm~ estilo shell de verdade~ */
        char line[256];
        int i = 0;
        while (cmd[i] && i < 255) { line[i] = cmd[i]; i++; }
        line[i] = '\0';
        uint32_t fsize; uint8_t attr;
        if (vfs_stat_size(line, &fsize, &attr) == 0)
            cmd_exec(line);
        else {
            console_write("comando nao encontrado: ");
            console_write(cmd);
            console_write("\n");
        }
    }
}

void execute(const char *cmd) {
    char segment[MAX_CMD];
    int length = 0;
    while (*cmd) {
        if (*cmd == ';' || *cmd == '\n') {
            segment[length] = 0;
            execute_one(segment);
            length = 0;
        } else if (length < MAX_CMD - 1) {
            segment[length++] = *cmd;
        }
        cmd++;
    }
    segment[length] = 0;
    execute_one(segment);
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
static int strieq(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;
        if (!a[i]) return 1;
    }
    return 1;
}

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ essa demorou pra debugar, respeita ~ */
void shell_init(void) {
    cmd_pos = 0;
    console_write("OvsbMkM Kernel Console\n");
    /* First test Linux ELF compatibility */
    cmd_exec("/bin/HELLO");
    /* Then try the terminal test (threads/sinais) */
    cmd_exec("/bin/TTEST");
    /* ~~ cwd fake sob ext2 — os paths de exec são absolutos~~ */
    /* TIPOS-TEST: DISP desligado pra liberar o framebuffer pro Xorg.
     * Reverter antes de mergear: cmd_exec("DISP"); */
    prompt();
}

/* ~~ shell_input ~~ */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void shell_input(char c) {
    if (c == '\n') {
        cmd_buf[cmd_pos] = 0;
        console_write("\n");
        execute(cmd_buf);
        cmd_pos = 0;
        prompt();
    } else if (c == '\b') {
        if (cmd_pos > 0) {
            cmd_pos--;
            console_write("\b \b");
        }
    } else {
        if (cmd_pos < MAX_CMD - 1) {
            cmd_buf[cmd_pos++] = c;
            console_putchar(c);
        }
    }
}

/* ♥ TODO: adicionar suporte a argumentos pra exec ~ argv futuramente! */

/* ♥ HELLO ~ kernel funcionando! */




/* ♥ shell.c ~ se bugar me chama, se n bugar tb me chama ~ >u< */
