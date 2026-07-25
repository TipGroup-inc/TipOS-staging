/* moe moe kyun <3 */
; ♥ driver ~ conversando com o hardware, seu chato!
; arquivo: keyboard_asm.asm ~ funcoes anotadas: 0
; ~*~ keyboard_asm.asm — Entrada da IRQ1 do teclado~*~
; Salva todos os registradores, chama o handler C, restaura tudo~
; Nao esqueca de mandar EOI pro PIC~ senao vai travar tudo!
; "Push tudo, menos a cozinha!" >_<
bits 64
global keyboard_irq_handler
extern keyboard_handler

keyboard_irq_handler:
    ; Salva o contexto ~ guarda tudo que eh seu!
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
    call keyboard_handler  ; Chama o handler C ~ "faz o teu!"
    ; Restaura tudo ~ pega de volta!
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
    iretq  ; Volta pro interrompido ~ "de volta pra realidade!"

; ~*~ mouse_irq_handler ~*~
; Mesma coisa que o teclado, mas pra IRQ12 (mouse).
; Salva registradores, chama mouse_handler(), restaura, iretq ~~
global mouse_irq_handler
extern mouse_handler

mouse_irq_handler:
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
    call mouse_handler
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
    iretq

; ♥ keyboard_asm.asm ~ feito com carinho (e uma raiva controlada) ~ kyun!
