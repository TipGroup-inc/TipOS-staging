/* moe moe kyun <3 */
/* moe moe kyun <3 */
; ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
; arquivo: enter_umode.asm ~ funcoes anotadas: 0
; ~*~ enter_umode.asm ~*~
; Hihi, assembly ~ mode difícil ativado!
; Se isso rodar, é milagre~ >_<
; ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

; ♥ ENTER_UMODE ~ "Vai pra ring 3 direto sem context_switch!"
; Dica: void enter_user_mode(void *rip, void *rsp);
; Monta o frame iretq certinho e pula pro modo usuario~ kyun!
bits 64
global enter_user_mode

enter_user_mode:
    ; rdi = RIP (entry point)
    ; rsi = RSP (user stack top)

    ; Carrega segmentos de dados ring 3
    mov rax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Monta frame iretq na pilha do kernel
    push 0x23          ; SS = ring 3 data | RPL3
    push rsi           ; RSP do usuario
    pushfq             ; RFLAGS atual
    pop rax
    or rax, 0x200      ; Forca IF = 1
    push rax           ; RFLAGS
    push 0x1B          ; CS = ring 3 code | RPL3
    push rdi           ; RIP

    iretq



; ♥ enter_umode.asm ~ arquivo fofinho do OvsbMkM! kyun~ <3
