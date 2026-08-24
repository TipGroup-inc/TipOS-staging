; moe moe kyun <3
; moe moe kyun <3
; moe moe kyun <3
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
; Detalhes da troca:
;   rdi = ponteiro pro PCB do processo atual (current)
;   rsi = ponteiro pro PCB do próximo processo (next)
;   RBX = pra user processes: ponteiro pro estado salvo no irq0 handler~
;         (a stack contém 15 regs + 5 iretq = 20 × 8 = 160 bytes)
;   Para kernel processes: RSP é salvo/restaurado diretamente
;
; ⚠️ CRÍTICO: O CR3 (page tables) é trocado AQUI~
;   Se as page tables dos processos forem diferentes (PML4 diferentes),
;   a partir de MOV CR3, rax TODOS os endereços são reinterpretados~
;   Por isso a stack atual RSP precisa ser válida NO NOVO espaço~
;   (Solução: kernel mapeado em todos PML4 no mesmo endereço ~
;    normalmente 0xFFFFFFFF80000000+ ~ assim RSP continua válido após troca)
;
; pcb_t layout (definido em pcb.h, refletido aqui em constantes):
;   0x00: int pid             0x04: int state
;   0x08: char name[32]
;   0x28: int is_user         0x2C: padding
;   0x30: uint64_t kernel_rsp 0x38: uint64_t rsp0
;   0x40: uint64_t pml4       0x48: uint64_t user_rsp
;   0x50: uint64_t user_rip   0x58: uint64_t user_rflags
;   0x60: s_rbx  0x68: s_rbp  0x70: s_r12
;   0x78: s_r13  0x80: s_r14  0x88: s_r15
;   0x90: parent_pid          0x94: exit_code
;   0x98: fs_base (uint64_t) — MSR_FS_BASE pra TLS do usuário~
; ============================================================================

bits 64
global context_switch
extern tss
extern current_rsp0

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
; ~~ FS_BASE = offsetof(pcb_t, fs_base) = 0xB8 ~~
; (era 0xA8 — defasado desde que vm_map/vmspace entraram no PCB!
;  O wrmsr tava carregando o PONTEIRO vm_map como TLS do musl~
;  e o %fs:0 do filho do fork lia lixo → RDX=0 → PF@0x98! kyun~)
FS_BASE  equ 0xB8
IN_KERN  equ 0x1C8      ; ~~ pcb->in_kern: bloqueio dentro de syscall~~

global context_switch_kern
context_switch_kern:
    ; ♥ context_switch_kern(cur=rdi, next=rsi) ~ troca DENTRO de syscall~
    ; Chamado do C (schedule) quando o processo ATUAL bloqueou no meio
    ; do fluxo C (wait4/read pipe). Salva o RSP apontando pro PRÓPRIO
    ; return address — o `ret` final devolve o controle pro caller
    ; (schedule) exatamente onde parou! kyun~
    mov dx, 0x3F8
    mov al, '<'
    out dx, al
    mov al, 'K'
    out dx, al
    mov al, '>'
    out dx, al
    mov [rdi + KRNL_RSP], rsp
    mov dword [rdi + IN_KERN], 1
    mov [rdi + S_RBX], rbx
    mov [rdi + S_RBP], rbp
    mov [rdi + S_R12], r12
    mov [rdi + S_R13], r13
    mov [rdi + S_R14], r14
    mov [rdi + S_R15], r15

    ; ~~ restore: depende de COMO o next foi salvo!~~
    cmp dword [rsi + IN_KERN], 0
    jne .do_resume_kern

    ; ---- next salvo em MODO USUÁRIO: caminho clássico ----
    mov dx, 0x3F8
    mov al, 'N'
    out dx, al
    mov rsp, [rsi + KRNL_RSP]
    mov rax, [rsi + PML4]
    mov cr3, rax
    mov rax, [rsi + RSP0]
    mov [tss + 4], rax
    mov [current_rsp0], rax
    mov rbx, [rsi + S_RBX]
    mov rbp, [rsi + S_RBP]
    mov r12, [rsi + S_R12]
    mov r13, [rsi + S_R13]
    mov r14, [rsi + S_R14]
    mov r15, [rsi + S_R15]
    cmp dword [rsi + IS_USER], 0
    je .kern_ret
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
.kern_ret:
    sti
    ret

.do_resume_kern:
.restore_in_kern:
    mov dx, 0x3F8
    mov al, '<'
    out dx, al
    mov al, 'R'
    out dx, al
    mov al, '>'
    out dx, al
    ; ~~ restauração de processo em modo kernel (compartilhada!) ~~
    ; KRNL_RSP aponta pra stack de C válida (return address no topo)
    mov rsp, [rsi + KRNL_RSP]
    mov rax, [rsi + PML4]
    mov cr3, rax
    mov rax, [rsi + RSP0]
    mov [tss + 4], rax
    mov [current_rsp0], rax
    mov rbx, [rsi + S_RBX]
    mov rbp, [rsi + S_RBP]
    mov r12, [rsi + S_R12]
    mov r13, [rsi + S_R13]
    mov r14, [rsi + S_R14]
    mov r15, [rsi + S_R15]
    mov dword [rsi + IN_KERN], 0
    mov dx, 0x3F8
    mov al, '<'
    out dx, al
    mov al, 'r'
    out dx, al
    mov al, '>'
    out dx, al
    ret                              ; continua o C do next!

context_switch:
    ; Save current process state
    ; For user processes: use RBX (= state pointer from interrupt handler).
    ; The interrupt handler (irq0 or syscall_isr) set RBX = RSP after pushing
    ; the 15 registers — this points directly to the clean 15-reg + 5-iretq
    ; block, WITHOUT the extra return addresses from the C call chain.
    ; For kernel-mode processes (idle): fall back to current RSP.
    cmp dword [rdi + IS_USER], 0   ; if current->is_user == 0?
    je .save_kernel                ; if kernel mode: save current RSP
    mov [rdi + KRNL_RSP], rbx      ; if user mode: RSP salvado no handler (RBX)
    jmp .save_regs
.save_kernel:
    mov [rdi + KRNL_RSP], rsp      ; kernel process: salva RSP direto
.save_regs:
    ; Salva callee-saved registers (RBX, RBP, R12-R15) pro PCB do atual~
    mov [rdi + S_RBX], rbx
    mov [rdi + S_RBP], rbp
    mov [rdi + S_R12], r12
    mov [rdi + S_R13], r13
    mov [rdi + S_R14], r14
    mov [rdi + S_R15], r15

    ; Save FS.base (MSR 0xC0000100) — pra TLS do usuário~
    mov ecx, 0xC0000100      ; MSR_FS_BASE — o "endereço" magico~
    rdmsr                     ; edx:eax = fs_base (64 bits em dois 32-bit)
    shl rdx, 32
    or rax, rdx               ; rax = fs_base completo (junta as partes)
    mov [rdi + FS_BASE], rax  ; salva no PCB atual pro futuro~

    ; Restore next process state
    cmp dword [rsi + IN_KERN], 0   ; ~~ próximo bloqueou EM SYSCALL?~~
    jne context_switch_kern.restore_in_kern   ; volta NO MEIO do fluxo C~

    mov dx, 0x3F8
    mov al, '<'
    out dx, al
    mov al, 'U'
    out dx, al
    mov al, '>'
    out dx, al
    mov rsp, [rsi + KRNL_RSP]      ; 1. Troca RSP pro kernel stack do next ~
    mov rax, [rsi + PML4]          ; 2. Troca page tables (CR3) ~
    mov cr3, rax                   ;    ATENÇÃO: endereços mudam! Mas kernel
                                   ;    mapeado em todo PML4, então RSP ainda é válido~
    mov rax, [rsi + RSP0]          ; 3. Atualiza TSS.RSP0 (stack ring 3 pra syscalls) ~
    mov [tss + 4], rax             ;    TSS[4] = RSP0 no formato IA32e TSS~

    ; 3a. Atualiza current_rsp0 (pra 'syscall' instruction) ~
    mov [current_rsp0], rax        ;    syscall_entry usa xchg com essa variável~

    ; 3b. Restaura FS.base (MSR 0xC0000100) — TLS do usuário~
    mov rax, [rsi + FS_BASE]       ;    carrega FS_BASE salvo do next processo~
    mov rcx, 0xC0000100            ;    MSR_FS_BASE
    mov rdx, rax
    shr rdx, 32                    ;    edx = upper 32 bits
    wrmsr                          ;    escreve MSR (e o musl sorri~)

    mov rbx, [rsi + S_RBX]         ; Restaura callee-saved do next
    mov rbp, [rsi + S_RBP]
    mov r12, [rsi + S_R12]
    mov r13, [rsi + S_R13]
    mov r14, [rsi + S_R14]
    mov r15, [rsi + S_R15]

    ; Check if next process is user mode
    cmp dword [rsi + IS_USER], 0   ; if next->is_user == 0?
    je .kernel_proc                ; if kernel mode: just ret

    ; User process: pop registers and iretq to ring 3
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
    iretq                ; iretq mágico: POPa RIP, CS, RFLAGS, RSP, SS~

.kernel_proc:
    ; Kernel process: just return (context was saved/restored)
    sti
    ret



; ♥ switch.asm ~ arquivo fofinho do OvsbMkM! kyun~ <3
