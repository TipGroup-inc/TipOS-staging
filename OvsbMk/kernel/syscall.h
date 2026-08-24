/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: syscall.h ~ funcoes anotadas: 0
 */
#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>

/* ~~ iovec pro writev/readv (ABI Linux x86_64) ~~ */
struct iovec {
    void     *iov_base;
    uint64_t  iov_len;
};

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
#define SYS_fork_real    214   /* ~~ fork POSIX de verdade (issue #72) ~~ */

/* ~~ Poll/Select pra st não morrer de tédio ~~ */
#define SYS_poll         7
#define SYS_select       23

/* ~~ Linux compat syscalls ~~ */
#define SYS_arch_prctl   158
#define SYS_clock_gettime 228
#define SYS_brk          231
#define SYS_nanosleep    234
#define SYS_set_tid_address 218
#define SYS_exit_group   212

/* ~~ misc syscalls (issue #52) ~~ */
#define SYS_pipe         22
#define SYS_dup          32
#define SYS_dup2         91
#define SYS_fcntl        72
#define SYS_uname        63
#define SYS_umask        400
#define SYS_getrusage    98
#define SYS_sysinfo      401
#define SYS_times        100
#define SYS_getppid      110
#define SYS_getpgid      121
#define SYS_clock_getres 229
#define SYS_getrandom    318
#define SYS_dup3         292
#define SYS_pipe2        293
#define SYS_msync        26
#define SYS_madvise      28
#define SYS_mremap       75

/* ~~ *at syscalls (issue #53) ~~ */
#define SYS_openat       257
#define SYS_mkdirat      258
#define SYS_newfstatat   262
#define SYS_unlinkat     263
#define SYS_renameat     264
#define SYS_readlinkat   267
#define SYS_chmod        402
#define SYS_fchmod       403
#define SYS_chown        404
#define SYS_fchown       405
#define SYS_statfs       406
#define SYS_fstatfs      407

/* ~~ writev (issue #55) ~~
 * Linux writev=20 colide c/ getpid=20 → 409 livre~ */
#define SYS_writev       409

/* ~~ readlink (89) ~ musl usa o syscall antigo, nao o *at ~~ */
#define SYS_readlink     89

/* ~~ sockets AF_UNIX (issue #49) ~~
 * números Linux: socket=41, connect=42, accept=43, sendto=44, recvfrom=45,
 * sendmsg=46, recvmsg=47, shutdown=48, bind=49, listen=50, getsockname=51,
 * socketpair=53, accept4=288 */
#define SYS_socket       41
#define SYS_connect      42
#define SYS_accept       43
#define SYS_sendto       44
#define SYS_recvfrom     45
#define SYS_sendmsg      46
#define SYS_recvmsg      92   /* 47 colide c/ getgid → mapeado pra 92 */
#define SYS_shutdown     408  /* 48 colide c/ getegid → mapeado pra 408 */
#define SYS_bind         49
#define SYS_listen       50
#define SYS_getsockname  51
#define SYS_socketpair   53
#define SYS_accept4      288

/* ~~ AF_UNIX constants ~~ */
#define AF_UNIX   1
#define SOCK_STREAM 1
#define SOCK_DGRAM  2

/* ~~ AT_FDCWD ~ dirfd relativo ao cwd atual (Linux -100) ~~ */
#define AT_FDCWD ((int)-100)

/* ~~ threads e sinais (issue #50) ~~
 * números Linux: rt_sigprocmask=14, rt_sigreturn=15, kill=62, wait4=61,
 * sigaltstack=131, prctl=157, futex=202, tgkill=234, set_robust_list=273,
 * rseq=334. Os que colidem c/ destinos do levemente viram números livres. */
#define SYS_rt_sigprocmask 14
#define SYS_rt_sigreturn   15
#define SYS_wait4          61
#define SYS_kill           62
#define SYS_sigaltstack    131
#define SYS_prctl          157
#define SYS_futex          96   /* 202 colide c/ mouse_read → 96 livre */
#define SYS_tgkill         103  /* 234 colide c/ nanosleep → 103 livre */
#define SYS_set_robust_list 273
#define SYS_get_robust_list 274
#define SYS_rseq           334

void syscall_init(void);
void syscall_handler(uint64_t *regs);

#endif

/* ♥ syscall.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
