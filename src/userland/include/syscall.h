/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland syscall wrapper ~ syscall instruction (linux abi) ~<3 */

#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include <stdint.h>

/* ~~ _syscall ~~
 * syscall instruction wrapper (linux x86_64 abi):
 *   rax = num, rdi = a1, rsi = a2, rdx = a3, r10 = a4, r8 = a5, r9 = a6
 * clobbers: rcx (return rip), r11 (return rflags)
 * returns: rax
 * usa "syscall" em vez de "int 0x80" ~ mais rapido, preserva IF <3 */
static inline long _syscall(long num, long a1, long a2, long a3, long a4) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(a4)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/* ~~ _syscall6 ~~ versao com 6 args (r8, r9) ~~ */
static inline long _syscall6(long num, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(a4), "r"(a5), "r"(a6)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#endif

/* ♥ syscall.h ~ arquivo fofinho do userland! kyun~ <3 */