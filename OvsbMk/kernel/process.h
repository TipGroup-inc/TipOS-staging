/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: process.h ~ funcoes anotadas: 0
 */
/* ♥ process.h ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef PROCESS_H
#define PROCESS_H
#include <stdint.h>

struct vm_map;
struct vmspace;

/* ♥ PROCESS ~ Gerenciamento de processos fofinho!
 * Dica: PCB = Process Control Block~ salva tudo do processo!
 * Agora com scheduler round-robin, waitpid, yield~
 * Estados: EMPTY, READY, RUNNING, BLOCKED, ZOMBIE~
 * Ce ta preparado? kyun~ <3 */

#define MAX_PROC 64
#define STACK_SIZE 8192
#define PROC_NAME_MAX 32

/* ♥ Estados do processo ~ "Tabela de estados! DECORE!" */
#define PROC_EMPTY   0
#define PROC_READY   1
#define PROC_RUNNING 2
#define PROC_BLOCKED 3
#define PROC_ZOMBIE  4

/* ♥ PCB ~ Tudo que o processo precisa!
 * A switch.asm acessa offsets fixos~ nao mude a ordem!
 * program_break e heap_start sao extras do OvsbMkM~
 * vm_map = mapa de enderecos estilo FreeBSD (vm_map.h) */
typedef struct pcb {
    int pid;
    int state;
    char name[PROC_NAME_MAX];
    int is_user;
    int padding;
    uint64_t kernel_rsp;
    uint64_t rsp0;
    uint64_t pml4;
    uint64_t user_rsp;
    uint64_t user_rip;
    uint64_t user_rflags;
    uint64_t s_rbx, s_rbp, s_r12, s_r13, s_r14, s_r15;
    int parent_pid;
    int exit_code;
    uint64_t program_break;
    uint64_t heap_start;
    struct vm_map *vm_map;
    struct vmspace *vmspace;
    uint64_t fs_base;
} pcb_t;

extern pcb_t pcb_table[MAX_PROC];
extern int current_pid;
extern int next_pid;

void process_init(void);
int  process_create_user(const char *name, void *entry, void *user_stack, uint64_t user_stack_size);
void process_switch_to(int pid);
void process_exit_current(int code);
pcb_t *process_current(void);
int    process_current_pid(void);

/* ♥ Novas funcoes do scheduler aprimorado! */
int  proc_spawn(const char *name, void *entry, void *user_stack_top);
void proc_exit(int code);
void proc_wake_parent(int child_pid);
int  proc_waitpid(int pid, int *exit_code);
void proc_yield(void);
void schedule(void);

void context_switch(pcb_t *current, pcb_t *next);

/* ~~ setup_linux_user_stack ~~
 * Monta a pilha Linux pro usuario! argc, argv, envp, auxv~
 * Tudo que o musl precisa pra nao reclamar da vida~
 * Recebe o topo da pilha e devolve o novo RSP~ */
uint64_t setup_linux_user_stack(pcb_t *pcb, uint64_t user_stack_top,
                                 uint64_t phdr, uint64_t phent, uint64_t phnum,
                                 uint64_t elf_base,
                                 char **argv, int argc,
                                 char **envp, int envc);

#endif

/* ♥ USER STACK ~ pilha do usuario em user accessible memory! */




/* ♥ process.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
