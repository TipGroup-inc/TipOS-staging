#include "process.h"
#include "memory.h"
#include "ring3.h"
#include "kernel.h"
#include <stdint.h>

pcb_t proc_table[MAX_PROC];
int current_pid = 0;
static int next_pid = 1;

void proc_init(void) {
    for (int i = 0; i < MAX_PROC; i++)
        proc_table[i].state = PROC_EMPTY;
    proc_table[0].pid = 0;
    proc_table[0].state = PROC_READY;
    proc_table[0].name[0] = '\0';
    proc_table[0].is_user = 0;
    proc_table[0].kernel_rsp = 0;
    proc_table[0].rsp0 = 0;
    proc_table[0].pml4 = pml4_get_current();
    next_pid = 1;
    current_pid = 0;
}

static int alloc_pid(void) {
    return next_pid++;
}

static int find_slot(void) {
    for (int i = 0; i < MAX_PROC; i++)
        if (proc_table[i].state == PROC_EMPTY) return i;
    return -1;
}

int proc_spawn(const char *name, void *entry, void *user_stack_top) {
    int slot = find_slot();
    if (slot < 0) return -1;

    void *kstack = mmap_user(0, STACK_SIZE, 3, 0);
    if (!kstack) return -1;

    uint64_t my_pml4 = pml4_create();
    if (!my_pml4) { munmap_user(kstack, STACK_SIZE); return -1; }

    pcb_t *p = &proc_table[slot];
    p->pid = alloc_pid();
    p->state = PROC_READY;
    p->is_user = 1;

    int ni = 0;
    while (name[ni] && ni < PROC_NAME_MAX - 1) { p->name[ni] = name[ni]; ni++; }
    p->name[ni] = '\0';

    p->rsp0 = (uint64_t)kstack + STACK_SIZE;
    p->pml4 = my_pml4;
    p->parent_pid = current_pid;
    p->exit_code = 0;

    // Build stack frame as it would look after IRQ0 handler + schedule call
    // Layout (from high to low):
    //   [iret frame: SS, user_RSP, RFLAGS, CS, RIP]
    //   [saved regs: r15,r14,r13,r12,rbp,rbx,r11,r10,r9,r8,rdi,rsi,rdx,rcx,rax]
    //   [return address for context_switch → schedule → irq0]
    uint64_t *sp = (uint64_t *)p->rsp0;

    // 1. iret frame
    *(--sp) = 0x23;                  // SS
    *(--sp) = (uint64_t)user_stack_top; // user RSP
    *(--sp) = 0x202;                 // RFLAGS (IF set)
    *(--sp) = 0x1B;                  // CS
    *(--sp) = (uint64_t)entry;       // RIP

    // 2. Saved registers (as irq0 pushes them)
    *(--sp) = 0; // r15
    *(--sp) = 0; // r14
    *(--sp) = 0; // r13
    *(--sp) = 0; // r12
    *(--sp) = 0; // rbp
    *(--sp) = 0; // rbx
    *(--sp) = 0; // r11
    *(--sp) = 0; // r10
    *(--sp) = 0; // r9
    *(--sp) = 0; // r8
    *(--sp) = 0; // rdi
    *(--sp) = 0; // rsi
    *(--sp) = 0; // rdx
    *(--sp) = 0; // rcx
    *(--sp) = 0; // rax

    // 3. Return address for context_switch → schedule → end of irq0
    // When context_switch returns (via ret), it goes to schedule, which
    // returns to irq0 (the `call schedule`), which then pops regs and iretq's.
    // But the return from context_switch pops the return address that
    // `call context_switch` pushed. We don't need to add it here;
    // it's created by the call instruction at runtime.
    //
    // So kernel_rsp = sp (pointing to the saved rax)
    // When context_switch loads this RSP and ret's, it returns to schedule()
    // which then returns to irq0, which pops everything and iretq's.

    p->kernel_rsp = (uint64_t)sp;
    p->s_rbx = 0;
    p->s_rbp = 0;
    p->s_r12 = 0;
    p->s_r13 = 0;
    p->s_r14 = 0;
    p->s_r15 = 0;

    return p->pid;
}

void proc_exit(int code) {
    if (current_pid < 0 || current_pid >= MAX_PROC) return;
    pcb_t *p = &proc_table[current_pid];
    p->exit_code = code;
    p->state = PROC_ZOMBIE;
    proc_wake_parent(p->pid);
    schedule();
    for (;;) __asm__ volatile("hlt");
}

void proc_wake_parent(int child_pid) {
    for (int i = 0; i < MAX_PROC; i++) {
        if (proc_table[i].state == PROC_BLOCKED) {
            // Check if this process has a child with the given PID
            for (int j = 0; j < MAX_PROC; j++) {
                if (proc_table[j].pid == child_pid &&
                    proc_table[j].parent_pid == proc_table[i].pid) {
                    proc_table[i].state = PROC_READY;
                    return;
                }
            }
        }
    }
}

int proc_waitpid(int pid, int *exit_code) {
    for (;;) {
        int found = 0;
        for (int i = 0; i < MAX_PROC; i++) {
            if (proc_table[i].pid == pid) {
                found = 1;
                if (proc_table[i].state == PROC_ZOMBIE) {
                    if (exit_code) *exit_code = proc_table[i].exit_code;
                    munmap_user((void *)(proc_table[i].rsp0 - STACK_SIZE), STACK_SIZE);
                    pml4_destroy(proc_table[i].pml4);
                    proc_table[i].state = PROC_EMPTY;
                    return pid;
                }
                proc_table[current_pid].state = PROC_BLOCKED;
                schedule();
                break; // woken up, retry
            }
        }
        if (!found) return -1;
    }
}

static int find_next_ready(void) {
    int start = current_pid;
    for (int i = 1; i < MAX_PROC; i++) {
        int idx = (start + i) % MAX_PROC;
        if (proc_table[idx].state == PROC_READY)
            return idx;
    }
    return current_pid;
}

void schedule(void) {
    int next_idx = find_next_ready();
    if (next_idx == -1 || next_idx == current_pid) return;
    int prev = current_pid;
    // Only mark as READY if it was RUNNING (not voluntarily BLOCKED)
    if (proc_table[prev].state == PROC_RUNNING)
        proc_table[prev].state = PROC_READY;
    proc_table[next_idx].state = PROC_RUNNING;
    current_pid = next_idx;
    context_switch(&proc_table[prev], &proc_table[next_idx]);
}

void proc_yield(void) {
    schedule();
}
