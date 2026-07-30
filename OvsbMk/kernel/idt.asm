; moe moe kyun <3
; moe moe kyun <3
; moe moe kyun <3
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

; ♥ ISR_NOERR ~ macro pra handlers de interrupção SEM error code automático ~
; Exceções de 0-7, 9-12, 15-31 (e outras) não empurram erro na stack~
; A CPU deixa a stack assim: [RIP, CS, RFLAGS, RSP, SS] (se ring→ring)~
; Mas nosso isr_common espera [int_no, err_code] antes dos regs salvos~
; Então push 0 como código de erro falso pra manter formato consistente~
; %1 = número da interrupção (0-31 pra exceções, 32-255 pra IRQ/syscall)~
%macro ISR_NOERR 1
global isr%1
isr%1:
    push 0              ; Código de erro falso (0) ~ pad pra stack frame uniforme
    push %1             ; Número da interrupção ~ idt_handler switch() usa isso
    jmp isr_common
%endmacro

; ♥ ISR_ERR ~ macro pra handlers COM error code automático ~
; Exceções 8, 10, 11, 12, 13, 14, 17, 30 empurram um error code na stack~
; Error code tem formato: bits 0-1 = EXT (external event), bit 2 = IDT (vector)~
;   bits 3-15 = segment selector index (se aplicável)~
; A CPU já empurrou o erro então só pushamos o número da interrupção~
; E jumpamos pra isr_common ~
; CUIDADO: se usar ISR_ERR numa exceção que não produz erro, a stack fica
;   desalinhada e o iretq restaura RIP/CS errado~ crash garantido~ hihi~!
%macro ISR_ERR 1
global isr%1
isr%1:
    push %1             ; Número da interrupção (error code já está na stack pela CPU)
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

; ♥ syscall_isr ~ handler da interrupção 0x80 (syscall gate) ~
; Chamado por processos ring 3 via `int 0x80` com:
;   rax = syscall number (SYS_* de syscall.h)
;   rdi, rsi, rdx, rcx, r8, r9 = argumentos (até 6, padrão System V ABI)
; A CPU já empilhou 5 valores (vindo de ring 3): SS, RSP, RFLAGS, CS, RIP~
; Precisamos salvar os 15 registradores gerais (rax..r15) pra preservar o estado~
; E chamar syscall_handler_zig(regs_struct) em C, que interpreta os registradores~
; O retorno em rax é passado de volta pro user process via pop rax + iretq~
; Ordem de push/pop importa! É simétrica ~ push na ordem decrescente, pop crescente~
; Por que push r15 primeiro? Pra que o struct em C tenha r15 no offset 0~
; (primeiro push = topo da stack = último offset no struct)~
global syscall_isr
extern syscall_handler_zig
syscall_isr:
    ; A CPU ja empilhou SS, RSP, RFLAGS, CS, RIP (vindo do ring 3)
    push r15            ; salva todos os 15 GPRs ~ struct começa com r15
    push r14
    push r13
    push r12
    push rbp
    push rbx
    push r11
    push r10
    push r9
    push r8
    push rdi             ; rdi = arg1 da syscall (fd, buf, etc)
    push rsi             ; rsi = arg2
    push rdx             ; rdx = arg3
    push rcx             ; rcx = arg4 (count, flags)
    push rax             ; rax = syscall number
    mov rdi, rsp         ; rdi = ponteiro pra struct registers na stack
    call syscall_handler_zig  ; syscall_handler_zig(regs) em zig/c
    pop rax              ; restaura rax (pode ter sido modificado como return value)
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
    iretq                ; retorna pra ring 3 com rax = syscall return value

; ♥ irq0 ~ Timer IRQ handler (IRQ0 = PIT channel 0, int 0x20) ~
; O PIT (Programmable Interval Timer) gera IRQ0 a ~1000 Hz (1ms)~
; Se vier de ring 3 (usuário): salva 15 regs, chama timer_tick_handler()~
;   que pode chamar schedule() → context_switch() pra preempção~
; Se vier de ring 0 (kernel): só manda EOI pro PIC e retorna~
;   Por que? O kernel pode estar no meio de uma syscall, e a stack tem
;   só 3 valores (RIP, CS, RFLAGS) ao invés de 5 (sem SS, RSP)~
;   context_switch espera 5 valores (iretq pra ring 3)~
;   Se tentar swichar durante kernel, iretq ia restaurar SS=CS, RSP=RIP~
;   Crash total! Já aconteceu~ confia em mim~ 3 horas de debug~ hihi~
;   Então... se ring 0: EOI + iretq rápido~ sem schedule!~
;
; PIC (8259A) EOI:
;   Out 0x20, 0x20 no master PIC (IRQ 0-7)~
;   Se não mandar EOI, o PIC não gera mais IRQs~
;   Mouse, teclado, tudo morre~ o usuario fica puto e o mouse vira enfeite~
;   Acredite, 3 horas de debug nisso (experiência própria)~ >_<
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

; ♥ irq1 ~ Keyboard IRQ handler (IRQ1 = int 0x21) ~
; O teclado PS/2 gera IRQ1 quando uma tecla é pressionada ou solta~
; O byte lido da porta 0x60 é o scan code (make/break)~
; Push 0 (err code falso) + 33 (vetor = IRQ1, número 1 + 32 offset PIC)~
; Depois jmp pra isr_common, que salva regs e chama idt_handler() em C~
; O handler C lê a porta 0x60 e coloca o caractere no buffer do teclado~
global irq1

irq1:
    push 0               ; err code falso (IRQ1 não tem erro automático)
    push 33              ; IRQ1 vector = 32 (PIC offset) + 1 = 33
    jmp isr_common

; ♥ isr_common ~ handler genérico de interrupção (usado por ISR_NOERR/ISR_ERR/IRQ) ~
; A stack ao entrar aqui tem:
;   [RSP+0]   = int_number (push %1 no ISR_*)
;   [RSP+8]   = err_code (0 ou real, pushado pela CPU ou pelo ISR_NOERR)
;   [RSP+16]  = RIP (da CPU)
;   [RSP+24]  = CS
;   [RSP+32]  = RFLAGS
;   [RSP+40]  = RSP (só se veio de ring diferente)
;   [RSP+48]  = SS (só se veio de ring diferente)
; Salvamos TODOS os 15 GPRs (rax..r15) pra C poder mexer sem medo~
; A stack frame fica: [r15, r14, ..., rax, int_no, err_code, RIP, CS, ...]~
; rdi = rsp (ponteiro pro struct) passado pra idt_handler(regs_struct*)~
; C decide: enviar EOI, lidar com exceção, matar processo, etc~
; No retorno, restauramos TUDO na ordem inversa (LIFO)~
; add rsp, 16 = descarta int_number + err_code ~ deixa só o frame iretq~
; iretq POPa RIP, CS, RFLAGS [, RSP, SS] e retorna pro código interrompido~
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
    mov rdi, rsp        ; Ponteiro para stack frame (struct registers*)
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
    
    ; Limpar código de erro e número (2 × 8 bytes = 16 bytes)
    add rsp, 16
    
    iretq

; ♥ idt.asm ~ se bugar me chama, se n bugar tb me chama ~ >u<
