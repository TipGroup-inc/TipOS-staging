#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define MAX_PROC 64
#define STACK_SIZE 4096
#define PROC_NAME_MAX 32

typedef enum {
    PROC_EMPTY = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE,
} proc_state_t;

typedef struct {
    int pid;
    proc_state_t state;
    char name[PROC_NAME_MAX];
    int is_user;             // 1=user mode (ring 3), 0=kernel mode (ring 0)
    uint64_t kernel_rsp;       // saved kernel stack pointer (points to saved regs frame)
    uint64_t rsp0;             // kernel stack top (for TSS.RSP0)
    uint64_t pml4;             // PML4 physical address
    uint64_t user_rsp;         // saved user RSP
    uint64_t user_rip;         // saved user RIP
    uint64_t user_rflags;      // saved RFLAGS
    uint64_t s_rbx, s_rbp, s_r12, s_r13, s_r14, s_r15;
    int parent_pid;
    int exit_code;
} pcb_t;

extern pcb_t proc_table[MAX_PROC];
extern int current_pid;

void proc_init(void);
int  proc_spawn(const char *name, void *entry, void *user_stack_top);
void proc_exit(int code);
void proc_wake_parent(int child_pid);
int  proc_waitpid(int pid, int *exit_code);
void proc_yield(void);
void schedule(void);

// Assembly function
void context_switch(pcb_t *current, pcb_t *next);

#endif