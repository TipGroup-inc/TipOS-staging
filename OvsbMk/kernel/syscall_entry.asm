; moe moe kyun <3
; syscall_entry.asm — 'syscall' instruction handler (Linux ABI)
;
; Configurado via MSR LSTAR. Compatível com o frame do syscall_isr (int 0x80)
; porque usa iretq pra retornar (não sysretq — nossa GDT não é compatível com
; >_< mas iretq funciona perfeitamente~)
;
; Layout da stack (20 qwords, igual int 0x80):
;   regs[0..14] = rax..r15  (15 GPRs, push order decrescente)
;   regs[15]    = RIP (return)
;   regs[16]    = CS  (0x1B = ring 3 code)
;   regs[17]    = RFLAGS
;   regs[18]    = RSP user
;   regs[19]    = SS  (0x23 = ring 3 data)

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

    ; Empilha iretq frame: SS, RSP, RFLAGS, CS, RIP (do topo pra base)
    push 0x23                   ; SS = ring 3 data
    push qword [current_rsp0]   ; RSP user (salvo no xchg)
    push r11                    ; RFLAGS return
    push 0x1B                   ; CS = ring 3 code
    push rcx                    ; RIP return

    ; Empilha 15 GPRs: r15 → rax (decrescente, igual syscall_isr)
    push r15
    push r14
    push r13
    push r12
    push rbp
    push rbx
    push r11                    ; r11 (clobbered, mas salva pro frame)
    push r10                    ; arg4 original
    push r9                     ; arg6
    push r8                     ; arg5
    push rdi                    ; arg1
    push rsi                    ; arg2
    push rdx                    ; arg3
    push rcx                    ; rcx (clobbered, dummy pro frame)
    push rax                    ; syscall number

    mov rdi, rsp                ; rdi = struct registers* pra C
    call syscall_handler_zig

    ; Restaura tudo (igual syscall_isr)
    pop rax
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop rbx
    pop rbp
    pop r12
    pop r13
    pop r14
    pop r15

    ; Restaura current_rsp0 pro kernel stack (pra próxima syscall)
    ; RSP atual aponta pro início do iretq frame (RIP)
    ; RSP + 40 = logo após o frame = kernel stack top
    lea rax, [rsp + 40]
    mov [current_rsp0], rax

    iretq                       ; POPa RIP, CS, RFLAGS, RSP, SS → ring 3~ ☆
