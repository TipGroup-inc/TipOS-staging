; moe moe kyun <3
; syscall_entry.asm — handler da instrução 'syscall' (Linux x86_64 ABI)
;
; Configurado via MSR LSTAR. Retorna pro userspace via IRETQ (sysretq
; não fecha com a GDT do TipOS — ver nota no fim do arquivo)~
; STAR configurado: SYSCALL_CS=0x08 (entrada), SYSRET_CS=0x18 (não usado)
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

    ; ~~ Salva return RIP/RFLAGS fora do frame de 15 regs ~~
    ; O handler (syscall_linux.zig) sobrescreve o slot rcx do frame
    ; (regs[1]) com o arg4 (r10 → rcx p/ syscalls Linux traduzidas)~
    ; Se a gente restaurasse rcx do frame, o sysret voltaria pro arg4
    ; (o XORG caiu em 0x22 = flags do mmap! rssrsrs)~
    ; Guardamos return RIP/RFLAGS num local dedicado do kernel stack:
    sub rsp, 16
    mov [rsp], rcx              ; return RIP original
    mov [rsp + 8], r11          ; return RFLAGS original

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

    ; ~~ Restaura return RIP/RFLAGS originais (ignora rcx do frame) ~~
    ; O rcx popado acima pode ter sido corrompido pelo handler (arg4 fix)~
    ; Aqui pegamos o valor original salvo ANTES do frame~
    ; (tipo guardar a senha num cofre antes de sair de casa~)
    mov r11, [rsp + 8]          ; return RFLAGS original
    mov rcx, [rsp]              ; return RIP original
    add rsp, 16                 ; rsp volta pro topo do kernel stack

    ; Restaura current_rsp0 pro kernel stack (pra próxima syscall)
    ; E restaura o user RSP que o xchg salvou na variável~
    ; xchg de novo: troca rsp (kernel top) com [current_rsp0] (user RSP)
    ; numa instrução só, SEM clobberar RAX (return value do syscall)!
    ; (o return do mmap virou o RSP uma vez e o musl surtou~ rssrsrs)
    xchg rsp, [current_rsp0]    ; rsp = user RSP; current_rsp0 = kernel top ✓

    ; ~~ Retorno pro userspace: IRETQ (sysretq é furada com essa GDT!) ~~
    ; O HARDWARE (Intel SDM, SYSRET; AMD APM Vol.2 5.9.3) faz no retorno
    ; 64-bit (REX.W): CS = IA32_STAR[63:48]+16 e SS = IA32_STAR[63:48]+8.
    ; Não é bug do QEMU — ele segue o spec certinho! (documentado~ ☆)
    ; Com STAR[63:48]=0x18: sysretq daria CS=0x28 (slot do TSS!) e o iretq
    ; do próximo IRQ validaria CS=0x28 no GDT → #GP err=0x28 (vimos isso)~
    ; E o layout pro sysretq (user data em X+8, user code em X+16) não
    ; fecha com a GDT do TipOS (code 0x18, data 0x20 — invertido)~ >_<
    ; IRETQ carrega CS/SS do frame (0x1B/0x23) validados no GDT de
    ; verdade — igual o context_switch faz. SEMPRE iretq! ~ ☆
    ; Frame: RIP CS RFLAGS RSP SS (5 qwords) montado no stack do user~
    ; (rsp = user RSP original aqui → campo RSP = rsp exato, sem conta!)
    mov [rsp - 40], rcx         ; RIP (return original, não o arg4!)
    mov qword [rsp - 32], 0x1B  ; CS (user code ring 3 — GDT 0x18)
    mov [rsp - 24], r11         ; RFLAGS (return original)
    mov [rsp - 16], rsp         ; RSP (user RSP original)
    mov qword [rsp - 8], 0x23   ; SS (user data ring 3 — GDT 0x20)
    sub rsp, 40
    iretq

