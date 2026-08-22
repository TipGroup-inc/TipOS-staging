; moe moe kyun <3
; syscall_entry.asm — handler da instrução 'syscall' (Linux x86_64 ABI)
;
; Configurado via MSR LSTAR. Usa sysret pra retornar (rápido, preserva IF).
; STAR configurado: SYSCALL_CS=0x08, SYSRET_CS=0x18, SYSRET_SS=0x20
;
; Convenção Linux x86_64 syscall:
;   rax = syscall number
;   rdi = arg1, rsi = arg2, rdx = arg3, r10 = arg4, r8 = arg5, r9 = arg6
;   rcx = return RIP (salvo pela CPU), r11 = return RFLAGS (salvo pela CPU)
;   rsp = user stack pointer
;
; Clobbers: rcx, r11 (salvos pela CPU em return RIP/RFLAGS)

bits 64

section .data
global current_rsp0
current_rsp0: dq 0          ; kernel stack top do processo atual
                             ; setado pelo context_switch

section .text
global syscall_entry
extern syscall_handler_zig

syscall_entry:
    ; rcx = return RIP do userspace (syscall clobbered)
    ; r11 = return RFLAGS do userspace (syscall clobbered)
    ; rsp = user stack pointer
    ; rax = syscall number
    ; rdi = arg1, rsi = arg2, rdx = arg3, r10 = arg4, r8 = arg5, r9 = arg6

    ; Troca stack: salva user RSP em current_rsp0, carrega kernel stack
    xchg rsp, [current_rsp0]    ; rsp=kernel_stack, mem=user_rsp
                                 ; IF já tá limpo pelo FMASK, sem IRQ aninhada~ ☆

    ; Salva registradores que a C espera preservados (callee-saved)
    ; + registradores voláteis que precisamos passar pro handler
    push r15
    push r14
    push r13
    push r12
    push rbp
    push rbx
    push r11                    ; RFLAGS do userspace (salvo pela CPU)
    push r10                    ; arg4 original
    push r9                     ; arg6
    push r8                     ; arg5
    push rdi                    ; arg1
    push rsi                    ; arg2
    push rdx                    ; arg3
    push rcx                    ; RIP return (salvo pela CPU)
    push rax                    ; syscall number

    mov rdi, rsp                ; rdi = struct registers* pra C
    call syscall_handler_zig

    ; Restaura registradores (ordem inversa)
    pop rax
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11                     ; restaura RFLAGS do userspace
    pop rbx
    pop rbp
    pop r12
    pop r13
    pop r14
    pop r15

    ; Restaura current_rsp0 pro kernel stack (pra próxima syscall)
    ; RSP atual aponta logo após o frame salvo
    lea rax, [rsp + 8]          ; pula o rax que popamos
    mov [current_rsp0], rax

    ; Prepara sysret:
    ;   rcx = user RIP (já está correto, popamos pro lugar certo)
    ;   r11 = user RFLAGS (já restaurado acima)
    ;   rsp = user RSP (salvo em [current_rsp0 - 8] antes do xchg)
    ; sysret restaura:
    ;   RIP <- rcx
    ;   RFLAGS <- r11
    ;   CS  <- STAR[63:48] (0x18 = user code)
    ;   SS  <- STAR[63:48] + 8 (0x20 = user data)
    ;   RSP <- rsp (current_rsp0 aponta pro user RSP salvo)

    mov rsp, [current_rsp0]     ; restaura user RSP
    sysret                      ; retorna pra ring 3 via syscall/sysret ~ ☆