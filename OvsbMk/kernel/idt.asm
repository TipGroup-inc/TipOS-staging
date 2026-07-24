/* moe moe kyun <3 */
; ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
; arquivo: idt.asm ~ funcoes anotadas: 0
; ~*~ idt.asm ~*~
; Hihi, assembly ~ mode difícil ativado!
; Se isso rodar, é milagre~ >_<
; ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

; ============================================================
; Ovsb.OS - Handlers de Interrupção (IDT) ~ kyun kyun~!
; "ISRs, IRQs, venham todos pro meu iretq!"
; Dica: ISR_ERR tem error code na stack, ISR_NOERR não~
; Cuidado pra não push a mais e quebrar o iretq! >_<
; ============================================================

bits 64

extern idt_handler

; Macro para criar handler sem código de erro
%macro ISR_NOERR 1
global isr%1
isr%1:
    push 0              ; Código de erro falso
    push %1             ; Número da interrupção
    jmp isr_common
%endmacro

; Macro para criar handler com código de erro
%macro ISR_ERR 1
global isr%1
isr%1:
    push %1             ; Número da interrupção
    jmp isr_common
%endmacro

; Handlers de exceção (0-31)
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; Syscall handler (int 0x80) — ring 3 friendly! "Vem de int 0x80, seu baka~"
global syscall_isr
extern syscall_handler
syscall_isr:
    ; A CPU ja empilhou SS, RSP, RFLAGS, CS, RIP (vindo do ring 3)
    push r15
    push r14
    push r13
    push r12
    push rbp
    push rbx
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rdx
    push rcx
    push rax
    mov rdi, rsp
    call syscall_handler
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
    iretq

; Timer IRQ handler (goes to C function directly)
global irq0
extern timer_tick_handler
irq0:
    ; Only call timer_tick_handler (which may schedule) if we came from
    ; user mode (ring 3). If we came from kernel mode (ring 0), just
    ; send EOI and return immediately — the scheduler must not switch
    ; processes while the kernel is in the middle of a syscall, because
    ; the interrupt frame has only 3 values (RIP, CS, RFLAGS) instead of
    ; the 5 values (SS, RSP, RFLAGS, CS, RIP) that context_switch's
    ; iretq expects.
    test byte [rsp+8], 3   ; Check CS.RPL: [rsp] = RIP, [rsp+8] = CS
    jnz .from_user
    ; From ring 0: send EOI to master PIC, then return.
    ; Se nao mandar EOI, o PIC fica esperando e NENHUMA outra IRQ
    ; (incluindo teclado e mouse) vai ser entregue. O usuario fica
    ; puto e o mouse vira enfeite. trust me, 3 horas de debug nisso.
    push rax
    mov al, 0x20
    out 0x20, al
    pop rax
    iretq
.from_user:
    ; From ring 3: CPU pushed SS, RSP, RFLAGS, CS, RIP (5 values).
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    ; RBX = state pointer (15 regs + 5 iretq). RBX is callee-saved,
    ; so it survives the timer_tick_handler → schedule → context_switch
    ; call chain. context_switch uses RBX to save kernel_rsp.
    mov rbx, rsp
    cld
    call timer_tick_handler
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

global irq1

irq1:
    push 0
    push 33
    jmp isr_common

isr_common:
    ; Salvar registradores
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ; Chamar handler C
    mov rdi, rsp        ; Ponteiro para stack frame
    call idt_handler
    
    ; Restaurar registradores
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ; Limpar código de erro e número
    add rsp, 16
    
    iretq

; ♥ idt.asm ~ se bugar me chama, se n bugar tb me chama ~ >u<
