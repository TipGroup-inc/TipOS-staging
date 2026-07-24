#include <stdint.h>
#include "shell_cmds.h"
#include "../kernel/kernel.h"
#include "../kernel/ring3.h"
#include "../kernel/memory.h"
#include "../kernel/process.h"
#include "../kernel/vga.h"
#include "../kernel/env.h"
#include "../kernel/utils.h"
#include "../kernel/key.h"
#include "../fs/fat32.h"
#include "../kernel/colors.h"
#include "../lib/gui/vesa.h"

extern int keyboard_avail(void);

// ─── Job control ──────────────────────────────────────────
#define MAX_JOBS 16
#define JOB_CMD_MAX 64
typedef struct { int used; int jid; int pid; char cmd[JOB_CMD_MAX]; } job_t;
static job_t job_table[MAX_JOBS];

static int job_add(const char *cmd) {
    for (int i = 0; i < MAX_JOBS; i++)
        if (!job_table[i].used) {
            job_table[i].used = 1;
            job_table[i].jid = i + 1;
            int j; for (j = 0; cmd[j] && j < JOB_CMD_MAX-1; j++)
                job_table[i].cmd[j] = cmd[j];
            job_table[i].cmd[j] = '\0';
            job_table[i].pid = 100 + i;
            return job_table[i].jid;
        }
    return -1;
}

static void job_remove(int jid) {
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].used && job_table[i].jid == jid)
            { job_table[i].used = 0; return; }
}

static int job_find(int jid) {
    for (int i = 0; i < MAX_JOBS; i++)
        if (job_table[i].used && job_table[i].jid == jid) return i;
    return -1;
}

// ─── Redirection state ───────────────────────────────────
static int redir_active = 0;
static char redir_buf[16384];
static int redir_len = 0;
static char *redir_file = 0;
static int redir_append = 0;
static int pipe_mode = 0;
static char pipe_cmd_buf[256];

#define CMD_MAX 256
#define HIST_MAX 64
static char history[HIST_MAX][CMD_MAX];
static int hist_count = 0;
static int last_exit_code = 0;

static void shell_flush(void);

void get_input(char *buf, int maxlen) {
    int pos = 0;
    int browse_idx = -1;
    while (1) {
        int k = read_key();
        char c = (char)k;
        if (c == '\n' || c == '\r') {
            buf[pos] = '\0';
            vga_putchar('\n');
            if (pos > 0) {
                int hi = hist_count % HIST_MAX;
                int j; for (j = 0; j < pos && j < CMD_MAX-1; j++)
                    history[hi][j] = buf[j];
                history[hi][j] = '\0';
                hist_count++;
            }
            return;
        }
        if (k == K_UP) {
            if (browse_idx == -1) browse_idx = (hist_count > 0) ? (hist_count - 1) : 0;
            else if (browse_idx > 0) browse_idx--;
            if (hist_count > 0 && browse_idx >= 0) {
                int src = browse_idx % HIST_MAX;
                while (pos > 0) { pos--; vga_puts("\b \b"); }
                int j; for (j = 0; history[src][j] && j < maxlen-1; j++)
                    buf[j] = history[src][j];
                buf[j] = '\0';
                pos = j;
                vga_puts(buf);
            }
            continue;
        }
        if (k == K_DOWN) {
            if (browse_idx == -1) continue;
            if (browse_idx < hist_count - 1) browse_idx++;
            if (browse_idx >= hist_count) { browse_idx = -1; while (pos > 0) { pos--; vga_puts("\b \b"); } continue; }
            int src = browse_idx % HIST_MAX;
            while (pos > 0) { pos--; vga_puts("\b \b"); }
            int j; for (j = 0; history[src][j] && j < maxlen-1; j++)
                buf[j] = history[src][j];
            buf[j] = '\0';
            pos = j;
            vga_puts(buf);
            continue;
        }
        browse_idx = -1;
        if (c == '\b' || c == 127) {
            if (pos > 0) { pos--; vga_puts("\b \b"); }
            continue;
        }
        if (c >= ' ' && c < 127) {
            if (pos < maxlen - 1) {
                buf[pos++] = c;
                vga_putchar(c);
            }
        }
    }
}

void build_prompt(char *prompt, int maxlen) {
    const char *p = env_get("PS1");
    if (!p) p = "[\\w]\\$ ";
    int o = 0;
    for (int i = 0; p[i] && o < maxlen-1; i++) {
        if (p[i] == '\\' && p[i+1]) {
            i++;
            if (p[i] == '\\')      { prompt[o++] = '\\'; }
            else if (p[i] == 'u')  { const char *u = "root"; for (int j = 0; u[j] && o < maxlen-1; j++) prompt[o++] = u[j]; }
            else if (p[i] == 'h')  { const char *h = "ovsb"; for (int j = 0; h[j] && o < maxlen-1; j++) prompt[o++] = h[j]; }
            else if (p[i] == 's')  { const char *sh = "Mk"; for (int j = 0; sh[j] && o < maxlen-1; j++) prompt[o++] = sh[j]; }
            else if (p[i] == '$')  { prompt[o++] = '#'; }
            else if (p[i] == 'w')  {
                char path[256];
                fat32_get_cwd_name(path, 256);
                for (int j = 0; path[j] && o < maxlen-1; j++) prompt[o++] = path[j];
            }
            else if (p[i] == 'W')  {
                char path[256];
                fat32_get_cwd_name(path, 256);
                int last = 0;
                for (int j = 0; path[j]; j++) if (path[j] == '/') last = j;
                const char *base = (last > 0) ? path + last + 1 : path;
                for (int j = 0; base[j] && o < maxlen-1; j++) prompt[o++] = base[j];
            }
        } else {
            prompt[o++] = p[i];
        }
    }
    prompt[o] = '\0';
}

static void shell_flush(void) {
    extern int g_fb_active;
    if (g_fb_active) vesa_flush();
}

void shell_loop(void) {
    while (1) {
        char prompt[256];
        vga_puts("\n");
        build_prompt(prompt, sizeof(prompt));
        vga_puts(prompt);
        shell_flush();
        char cmd[CMD_MAX];
        get_input(cmd, sizeof(cmd));
        execute_command(cmd);
        shell_flush();
        env_set("?", last_exit_code ? "1" : "0");
    }
}

void execute_command(const char *cmd) {
    if (!cmd || !*cmd) return;

    char line[CMD_MAX];
    int li; for (li = 0; cmd[li] && li < CMD_MAX-1; li++) line[li] = cmd[li];
    line[li] = '\0';

    // ── Alias expansion ───────────────────────────────────
    char aexp[CMD_MAX]; int ai = 0;
    const char *p = line;
    while (*p == ' ') p++;
    char first[64]; int fi = 0;
    while (*p && *p != ' ' && fi < 63) first[fi++] = *p++;
    first[fi] = '\0';
    const char *rest = p;
    const char *aval = alias_get(first);
    if (aval) {
        for (int i = 0; aval[i] && ai < CMD_MAX-2; i++) aexp[ai++] = aval[i];
        if (*rest && ai < CMD_MAX-2) aexp[ai++] = ' ';
        while (*rest && ai < CMD_MAX-2) aexp[ai++] = *rest++;
        aexp[ai] = '\0';
    } else {
        for (int i = 0; line[i] && ai < CMD_MAX-1; i++) aexp[ai++] = line[i];
        aexp[ai] = '\0';
    }

    // ── Parse redirection ─────────────────────────────────
    char work[CMD_MAX]; int wi = 0;
    char in_file[CMD_MAX]; int in_f = 0;
    redir_file = 0;
    redir_append = 0;
    pipe_mode = 0;

    p = aexp;
    while (*p == ' ') p++;
    int after_redir = 0;
    for (int i = 0; p[i]; i++) {
        if (p[i] == '>' && !after_redir) {
            work[wi] = '\0';
            static char rfile[CMD_MAX];
            int j = i+1;
            if (p[j] == '>') { redir_append = 1; j++; }
            while (p[j] == ' ') j++;
            int ri = 0;
            while (p[j] && p[j] != ' ' && p[j] != '|' && ri < CMD_MAX-1)
                rfile[ri++] = p[j++];
            rfile[ri] = '\0';
            redir_file = rfile;
            const char *rest2 = p + j;
            if (rest2) {
                int k = wi;
                for (int idx = 0; rest2[idx] && k < CMD_MAX-1; idx++)
                    work[k++] = rest2[idx];
                work[k] = '\0';
            }
            after_redir = 1;
            break;
        }
        else if (p[i] == '|' && !after_redir) {
            work[wi] = '\0';
            int j = i+1;
            while (p[j] == ' ') j++;
            int pi = 0;
            while (p[j] && pi < CMD_MAX-1) pipe_cmd_buf[pi++] = p[j++];
            pipe_cmd_buf[pi] = '\0';
            pipe_mode = 1;
            break;
        }
        else {
            work[wi++] = p[i];
        }
    }
    if (!after_redir && !pipe_mode) {
        wi = 0;
        for (int i = 0; aexp[i] && wi < CMD_MAX-1; i++) work[wi++] = aexp[i];
        work[wi] = '\0';
    }

    while (wi > 0 && work[wi-1] == ' ') work[--wi] = '\0';
    while (*work == ' ') { for (int i = 0; work[i]; i++) work[i] = work[i+1]; }

    p = work;
    wi = 0;
    int got_in = 0;
    for (int i = 0; work[i]; i++) {
        if (work[i] == '<' && !got_in) {
            work[wi] = '\0';
            int j = i+1;
            while (work[j] == ' ') j++;
            int ri = 0;
            while (work[j] && work[j] != ' ' && ri < CMD_MAX-1)
                in_file[ri++] = work[j++];
            in_file[ri] = '\0';
            int k = wi;
            while (work[j]) work[k++] = work[j++];
            work[k] = '\0';
            got_in = 1;
            break;
        }
        wi++;
    }

    // ── Dispatch built-in commands ────────────────────────
    if (strcmp(work, "ls") == 0) {
        cmd_ls();
    }
    else if (strncmp(work, "ls ", 3) == 0) {
        const char *args = work + 3;
        while (*args == ' ') args++;
        if (*args) {
            uint32_t saved = fat32_get_cwd();
            if (fat32_change_dir(args) == 0) { cmd_ls(); fat32_set_cwd(saved); }
            else { set_vga_color(C_ERROR); vga_puts("Diretorio nao encontrado: "); vga_puts(args); vga_putchar('\n'); set_vga_color(C_OUTPUT); }
        } else cmd_ls();
    }
    else if (strcmp(work, "cd") == 0) { cmd_cd(""); }
    else if (strncmp(work, "cd ", 3) == 0) { const char *dir = work + 3; while (*dir == ' ') dir++; cmd_cd(dir); }
    else if (strcmp(work, "cat") == 0) { set_vga_color(C_ERROR); vga_puts("Uso: cat <arquivo>\n"); set_vga_color(C_OUTPUT); }
    else if (strncmp(work, "cat ", 4) == 0) { cmd_cat(work + 4); }
    else if (strcmp(work, "echo") == 0) { cmd_echo(""); }
    else if (strncmp(work, "echo ", 5) == 0) { cmd_echo(work + 5); }
    else if (strcmp(work, "pwd") == 0) { cmd_pwd(); }
    else if (strcmp(work, "clear") == 0) { cmd_clear(); }
    else if (strcmp(work, "help") == 0 || strcmp(work, "?") == 0) { cmd_help(); }
    else if (strcmp(work, "reboot") == 0) { cmd_reboot(); }
    else if (strcmp(work, "shutdown") == 0) { cmd_shutdown(); }
    else if (strcmp(work, "about") == 0 || strcmp(work, "ver") == 0) { cmd_about(); }
    else if (strncmp(work, "stat ", 5) == 0) { cmd_stat(work + 5); }
    else if (strcmp(work, "stat") == 0) { set_vga_color(C_ERROR); vga_puts("Uso: stat <arquivo>\n"); set_vga_color(C_OUTPUT); }
    else if (strncmp(work, "mkdir ", 6) == 0) { cmd_mkdir(work + 6); }
    else if (strncmp(work, "touch ", 6) == 0) { cmd_touch(work + 6); }
    else if (strncmp(work, "rm ", 3) == 0) { cmd_rm(work + 3); }
    else if (strncmp(work, "rmdir ", 6) == 0) { cmd_rmdir(work + 6); }
    else if (strncmp(work, "mv ", 3) == 0) { cmd_mv(work + 3); }
    else if (strncmp(work, "cp ", 3) == 0) { cmd_cp(work + 3); }
    else if (strcmp(work, "date") == 0) { cmd_date(); }
    else if (strcmp(work, "uptime") == 0) { cmd_uptime(); }
    else if (strcmp(work, "disp") == 0) { cmd_disp(); }
    else if (strncmp(work, "cc ", 3) == 0) { cmd_cc(work + 3); }
    else if (strncmp(work, "make ", 5) == 0 || strcmp(work, "make") == 0) {
        const char *args = (strncmp(work, "make ", 5) == 0) ? work + 5 : "";
        while (*args == ' ') args++;
        cmd_make(args);
    }

    // ── Environment commands ──────────────────────────────
    else if (strcmp(work, "export") == 0) {
        set_vga_color(C_ERROR); vga_puts("Uso: export NOME=valor\n"); set_vga_color(C_OUTPUT);
    }
    else if (strncmp(work, "export ", 7) == 0) {
        const char *eq = work + 7;
        const char *sep = eq;
        while (*sep && *sep != '=') sep++;
        if (*sep == '=') {
            char ename[64]; int i; for (i = 0; eq+i < sep && i < 63; i++) ename[i] = eq[i];
            ename[i] = '\0';
            env_set(ename, sep + 1);
            set_vga_color(C_SUCCESS);
            vga_puts(ename); vga_puts("="); vga_puts(sep+1); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        } else {
            set_vga_color(C_ERROR); vga_puts("Uso: export NOME=valor\n"); set_vga_color(C_OUTPUT);
        }
    }
    else if (strcmp(work, "unset") == 0) {
        set_vga_color(C_ERROR); vga_puts("Uso: unset <NOME>\n"); set_vga_color(C_OUTPUT);
    }
    else if (strncmp(work, "unset ", 6) == 0) {
        // env_unset not available; just set to empty
        env_set(work + 6, "");
        set_vga_color(C_SUCCESS);
        vga_puts("Unset: "); vga_puts(work + 6); vga_putchar('\n');
        set_vga_color(C_OUTPUT);
    }
    else if (strcmp(work, "env") == 0 || strcmp(work, "set") == 0) {
        int n = env_count_get();
        for (int i = 0; i < n; i++) {
            set_vga_color(C_OUTPUT);
            vga_puts(env_name_get(i)); vga_puts("="); vga_puts(env_val_get(i)); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        }
    }
    else if (strncmp(work, "mount ", 6) == 0) {
        set_vga_color(C_OUTPUT); vga_puts("mount: not implemented\n"); set_vga_color(C_OUTPUT);
    }

    // ── Alias commands ───────────────────────────────────
    else if (strcmp(work, "alias") == 0) {
        int n = alias_count_get();
        for (int i = 0; i < n; i++) {
            set_vga_color(C_OUTPUT);
            vga_puts(alias_name_get(i)); vga_puts("='"); vga_puts(alias_val_get(i)); vga_puts("'\n");
            set_vga_color(C_OUTPUT);
        }
    }
    else if (strncmp(work, "alias ", 6) == 0) {
        const char *eq = work + 6;
        const char *sep = eq;
        while (*sep && *sep != '=') sep++;
        if (*sep == '=') {
            char aname[64]; int i; for (i = 0; eq+i < sep && i < 63; i++) aname[i] = eq[i];
            aname[i] = '\0';
            alias_set(aname, sep + 1);
            set_vga_color(C_SUCCESS);
            vga_puts("Alias definido: "); vga_puts(aname); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        } else {
            set_vga_color(C_ERROR); vga_puts("Uso: alias nome='comando'\n"); set_vga_color(C_OUTPUT);
        }
    }
    else if (strcmp(work, "unalias") == 0) {
        set_vga_color(C_ERROR); vga_puts("Uso: unalias <nome>\n"); set_vga_color(C_OUTPUT);
    }
    else if (strncmp(work, "unalias ", 8) == 0) {
        if (alias_unset(work + 8)) {
            set_vga_color(C_SUCCESS); vga_puts("Alias removido: "); vga_puts(work + 8); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        } else {
            set_vga_color(C_ERROR); vga_puts("Alias nao encontrado: "); vga_puts(work + 8); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        }
    }
    else if (strncmp(work, "source ", 7) == 0) {
        const char *fn = work + 7;
        static uint8_t sbuf[4096];
        int n = fat32_read_file(fn, sbuf, 4096);
        if (n > 0) {
            char line[CMD_MAX]; int li = 0;
            for (int i = 0; i <= n; i++) {
                if (i == n || sbuf[i] == '\n') {
                    line[li] = '\0';
                    if (li > 0) execute_command(line);
                    li = 0;
                } else if (li < CMD_MAX-1) { line[li++] = sbuf[i]; }
            }
        } else {
            set_vga_color(C_ERROR); vga_puts("Nao encontrado: "); vga_puts(fn); vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        }
    }
    else if (strcmp(work, "jobs") == 0) {
        int any = 0;
        for (int i = 0; i < MAX_JOBS; i++) {
            if (job_table[i].used) {
                set_vga_color(C_OUTPUT);
                vga_puts("["); vga_putchar('0' + job_table[i].jid);
                vga_puts("]+ Running    "); vga_puts(job_table[i].cmd);
                vga_putchar('\n'); set_vga_color(C_OUTPUT);
                any = 1;
            }
        }
        if (!any) { set_vga_color(C_OUTPUT); vga_puts("(nenhum job)\n"); set_vga_color(C_OUTPUT); }
    }
    else if (strncmp(work, "fg ", 3) == 0) {
        int jid = work[3] - '0';
        int idx = job_find(jid);
        if (idx < 0) { set_vga_color(C_ERROR); vga_puts("Job nao encontrado\n"); set_vga_color(C_OUTPUT); }
        else { set_vga_color(C_OUTPUT); vga_puts("fg: "); vga_puts(job_table[idx].cmd); vga_putchar('\n'); set_vga_color(C_OUTPUT); }
    }
    else if (strcmp(work, "fg") == 0) { set_vga_color(C_ERROR); vga_puts("Uso: fg <job>\n"); set_vga_color(C_OUTPUT); }
    else if (strncmp(work, "bg ", 3) == 0) {
        int jid = work[3] - '0';
        int idx = job_find(jid);
        if (idx < 0) { set_vga_color(C_ERROR); vga_puts("Job nao encontrado\n"); set_vga_color(C_OUTPUT); }
        else { set_vga_color(C_OUTPUT); vga_puts("bg: "); vga_puts(job_table[idx].cmd); vga_putchar('\n'); set_vga_color(C_OUTPUT); }
    }
    else if (strcmp(work, "bg") == 0) { set_vga_color(C_ERROR); vga_puts("Uso: bg <job>\n"); set_vga_color(C_OUTPUT); }

    // ── PATH search ──────────────────────────────────────
    else if (*work != '\0') {
        const char *path = env_get("PATH");
        if (!path) path = "/BIN:/USR/BIN:/LOCAL/BIN";
        char pbuf[64]; int pi = 0; int found = 0;
        for (int i = 0; !found; i++) {
            if (path[i] == ':' || path[i] == '\0') {
                pbuf[pi] = '\0';
                if (pi > 0 && cmd_exec_in_dir(work, pbuf) == 0) found = 1;
                pi = 0;
                if (path[i] == '\0') break;
            } else {
                if (pi < 63) pbuf[pi++] = path[i];
            }
        }
        if (!found) {
            last_exit_code = 1;
            set_vga_color(C_ERROR);
            vga_puts("Comando nao encontrado: ");
            vga_puts(work);
            vga_putchar('\n');
            set_vga_color(C_OUTPUT);
        } else {
            last_exit_code = 0;
        }
    }

    // ── Flush redirection buffer to file ─────────────────
    if (redir_file && redir_active) {
        redir_active = 0;
        if (redir_append) {
            static uint8_t old[16384];
            int old_n = fat32_read_file(redir_file, old, 16384);
            if (old_n < 0) old_n = 0;
            int total = old_n + redir_len;
            if (total > 16384) total = 16384;
            static uint8_t merged[16384];
            for (int i = 0; i < old_n && i < 16384; i++) merged[i] = old[i];
            for (int i = 0; i < total - old_n; i++) merged[old_n + i] = redir_buf[i];
            fat32_write_file(redir_file, merged, total);
        } else {
            if (redir_len > 0) {
                fat32_create_file(redir_file);
                fat32_write_file(redir_file, (uint8_t*)redir_buf, redir_len);
            }
        }
    }

    if (pipe_mode) {
        set_vga_color(C_ERROR);
        vga_puts("[pipe not fully implemented yet]\n");
        set_vga_color(C_OUTPUT);
    }
}
