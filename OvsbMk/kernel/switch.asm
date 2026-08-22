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
FS_BASE  equ 0xA8

context_switch:
    ; Save current process state
    ; For user processes: use RBX (= state pointer from interrupt handler).
    ; The interrupt handler (irq0 or syscall_isr) set RBX = RSP after pushing
    ; the 15 registers — this points directly to the clean 15-reg + 5-iretq
    ; block, WITHOUT the extra return addresses from the C call chain.
    ; For kernel processes (idle): fall back to current RSP.
    cmp dword [rdi + IS_USER], 0   ; if current->is_user == 0?
    je .save_kernel                ; if kernel mode: save current RSP
    mov [rdi + KRNL_RSP], rbx      ; if user mode: RSP salvado no handler (RBX)
    jmp .save_regs
.save_kernel:
    mov [rdi + KRNL_RSP], rsp      ; kernel process: salva RSP direto
.save_regs:
    ; Salva callee-saved registers (RBX, RBP, R12-R15) pro PCB do atual~
    ; Esses registradores são PRESERVADOS pela ABI C através de chamadas~
    ; Os registradores voláteis (RAX, RCX, RDX, RSI, RDI, R8-R11) NÃO são
    ; salvos porque o compilador já salvou/descartou antes de chamar
    ; context_switch() ~ isso é verdade pra kernel processes~
    ; Pra user processes, os voláteis estão no frame 15-reg da interrupção
    ; que é restaurado no fim (pop rax..r15 + iretq)~
    mov [rdi + S_RBX], rbx
    mov [rdi + S_RBP], rbp
    mov [rdi + S_R12], r12
    mov [rdi + S_R13], r13
    mov [rdi + S_R14], r14
    mov [rdi + S_R15], r15

    ; Save FS.base (MSR 0xC0000100) — pra TLS do usuário~
    ; Cada processo tem seu proprio TLS (quem mandou o musl ser
    ;  exigente?) — então a gente salva o MSR antes de trocar~
    ; Usa rdmsr em vez de rdfsbase pq isso precisaria CR4.FSGSBASE,
    ; e a gente prefere não depender disso~ rdmsr funciona sempre
    ; em ring 0~ ☆
    mov ecx, 0xC0000100      ; MSR_FS_BASE — o "endereço" magico~
    rdmsr                     ; edx:eax = fs_base (64 bits em dois 32-bit)
    shl rdx, 32
    or rax, rdx               ; rax = fs_base completo (junta as partes)
    mov [rdi + FS_BASE], rax  ; salva no PCB atual pro futuro~

    ; Restore next process state
    mov rsp, [rsi + KRNL_RSP]      ; 1. Troca RSP pro kernel stack do next ~
                                   ;    A partir daqui, a pilha é do next PCB!
    mov rax, [rsi + PML4]          ; 2. Troca page tables (CR3) ~
    mov cr3, rax                   ;    ATENÇÃO: endereços mudam! Mas kernel
                                   ;    mapeado em todo PML4, então RSP ainda é válido~
    mov rax, [rsi + RSP0]          ; 3. Atualiza TSS.RSP0 (stack ring 0 pra syscalls) ~
    mov [tss + 4], rax             ;    TSS[4] = RSP0 no formato IA32e TSS~
                                    ;    Quando o next process fizer int 0x80,
                                    ;    a CPU troca RSP pra esse valor~

    ; 3a. Atualiza current_rsp0 (pra 'syscall' instruction) ~
    mov [current_rsp0], rax        ;    syscall_entry usa xchg com essa variável~

    ; 3b. Restaura FS.base (MSR 0xC0000100) — TLS do usuário~
    ; Sem isso o musl chora "onde ta meu TLS?! >_<"
    ; rdmsr/wrmsr pra ler/escrever o MSR ~ funciona sempre em ring 0~
    mov rax, [rsi + FS_BASE]       ;    carrega FS_BASE salvo do next processo~
    mov rcx, 0xC0000100            ;    MSR_FS_BASE — o segredo da felicidade do musl
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
    ; Stack layout: [r15,r14,...,rax, RIP,CS,RFLAGS,RSP,SS] (20 × 8 = 160 bytes)
    ; O frame foi montado pelo syscall_isr ou irq0 handler original~
    ; Só POPamos de volta e iretq pro ring 3~
    ; NOTA: RBX foi restaurado antes ~ o pop rbx abaixo sobrescreve
    ; com o valor salvo do PCB (não o state pointer do handler)~
    ; Isso é correto: RBX do user process tem valor de antes da int~
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
                         ; e volta a executar o user process onde parou~
                         ; (ou na entry point se for o primeiro schedule)

.kernel_proc:
    ; Kernel process: just return (context was saved/restored)
    ; O next processo kernel continua no mesmo espaço de endereçamento~
    ; RSP já aponta pra kernel stack dele ~ simples assim~ :)
    ret



; ♥ switch.asm ~ arquivo fofinho do OvsbMkM! kyun~ <3
