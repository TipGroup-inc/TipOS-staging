/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: syscall.h ~ funcoes anotadas: 0
 */
#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>

#define SYS_exit         1
#define SYS_read         3
#define SYS_write        4
#define SYS_open         5
#define SYS_close        6
#define SYS_unlink       10
#define SYS_getpid       20
#define SYS_getuid       24
#define SYS_geteuid      25
#define SYS_access       33
#define SYS_getgid       47
#define SYS_getegid      48
#define SYS_ioctl        54
#define SYS_munmap       73
#define SYS_mprotect     74
#define SYS_mkdir2       136
#define SYS_rmdir2       137
#define SYS_sigaction    134
#define SYS_sigreturn    173
#define SYS_gettimeofday 116
#define SYS_mmap         197
#define SYS_kbhit        198
#define SYS_lstat        199
#define SYS_disp_get_fb  200
#define SYS_disp_flush   201
#define SYS_mouse_read   202
#define SYS_kb_mod       203
#define SYS_lseek        204
#define SYS_disp_flush_rect 205
#define SYS_stat         188
#define SYS_fstat        189

#define SYS_readdir      207
#define SYS_execve       208
#define SYS_shell_cmd    209
#define SYS_spawn        210
#define SYS_spawn_shared 211

/* ~~ Poll/Select pra st não morrer de tédio ~~ */
#define SYS_poll         7
#define SYS_select       23

/* ~~ Linux compat syscalls ~~ */
#define SYS_arch_prctl   158
#define SYS_clock_gettime 228
#define SYS_brk          231
#define SYS_nanosleep    234
#define SYS_set_tid_address 258
#define SYS_exit_group   212

void syscall_init(void);
void syscall_handler(uint64_t *regs);

#endif

/* ♥ syscall.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
