/* moe moe kyun <3 */
/* moe moe kyun <3 */
; ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
; arquivo: switch.asm ~ funcoes anotadas: 0
; ~*~ switch.asm ~*~
; Hihi, assembly ~ mode difícil ativado!
; Se isso rodar, é milagre~ >_<
; ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

; ============================================================================
; ♥ SWITCH.ASM ~ "Troca de contexto: pula, gira, salva tudo!"
; Dica: salva todos registradores não voláteis ou o processo vai crashar~
; Se esquecer o RSP, volta pra lugar nenhum~ baka!
; ============================================================================
; void context_switch(pcb_t *current, pcb_t *next);
;
; Saves current process state, restores next process state.
; For user-mode processes (is_user=1): pops registers and iretq's
; to user space (or ring 3 entry point for first-run).
; For kernel-mode processes (is_user=0): just returns via ret.
;
; pcb_t layout:
;   0x00: int pid             0x04: int state
;   0x08: char name[32]
;   0x28: int is_user         0x2C: padding
;   0x30: uint64_t kernel_rsp 0x38: uint64_t rsp0
;   0x40: uint64_t pml4       0x48: uint64_t user_rsp
;   0x50: uint64_t user_rip   0x58: uint64_t user_rflags
;   0x60: s_rbx  0x68: s_rbp  0x70: s_r12
;   0x78: s_r13  0x80: s_r14  0x88: s_r15
;   0x90: parent_pid          0x94: exit_code
; ============================================================================

bits 64
global context_switch
extern tss

IS_USER  equ 0x28
KRNL_RSP equ 0x30
RSP0     equ 0x38
PML4     equ 0x40
S_RBX    equ 0x60
S_RBP    equ 0x68
S_R12    equ 0x70
S_R13    equ 0x78
S_R14    equ 0x80
S_R15    equ 0x88

context_switch:
    ; Save current process state
    ; For user processes: use RBX (= state pointer from interrupt handler).
    ; The interrupt handler (irq0 or syscall_isr) set RBX = RSP after pushing
    ; the 15 registers — this points directly to the clean 15-reg + 5-iretq
    ; block, WITHOUT the extra return addresses from the C call chain.
    ; For kernel processes (idle): fall back to current RSP.
    cmp dword [rdi + IS_USER], 0
    je .save_kernel
    mov [rdi + KRNL_RSP], rbx
    jmp .save_regs
.save_kernel:
    mov [rdi + KRNL_RSP], rsp
.save_regs:
    mov [rdi + S_RBX], rbx
    mov [rdi + S_RBP], rbp
    mov [rdi + S_R12], r12
    mov [rdi + S_R13], r13
    mov [rdi + S_R14], r14
    mov [rdi + S_R15], r15

    ; Restore next process state
    mov rsp, [rsi + KRNL_RSP]
    mov rax, [rsi + PML4]
    mov cr3, rax
    mov rax, [rsi + RSP0]
    mov [tss + 4], rax
    mov rbx, [rsi + S_RBX]
    mov rbp, [rsi + S_RBP]
    mov r12, [rsi + S_R12]
    mov r13, [rsi + S_R13]
    mov r14, [rsi + S_R14]
    mov r15, [rsi + S_R15]

    ; Check if next process is user mode
    cmp dword [rsi + IS_USER], 0
    je .kernel_proc

    ; User process: pop registers and iretq to ring 3
    ; Stack layout: [r15,r14,...,rax, RIP,CS,RFLAGS,RSP,SS]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

.kernel_proc:
    ; Kernel process: just return (context was saved/restored)
    ret



; ♥ switch.asm ~ arquivo fofinho do OvsbMkM! kyun~ <3
