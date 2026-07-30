; moe moe kyun <3
; moe moe kyun <3
; moe moe kyun <3
; ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
; arquivo: enter_umode.asm ~ funcoes anotadas: 0
; ~*~ enter_umode.asm ~*~
; Hihi, assembly ~ mode difícil ativado!
; Se isso rodar, é milagre~ >_<
; ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

; ♥ ENTER_UMODE ~ "Vai pra ring 3 direto sem context_switch!"
; Dica: void enter_user_mode(void *rip, void *rsp);
; Monta o frame iretq certinho e pula pro modo usuario~ kyun!
; 
; Detalhes técnicos do iretq pra ring 3:
;   A CPU espera 5 valores na stack (do topo pra base):
;     [RSP+0]  RIP     ~ entry point do usuário (onde começa a executar)
;     [RSP+8]  CS      ~ code segment selector + RPL = 0x1B (GDT[3] + RPL3)
;     [RSP+16] RFLAGS  ~ flags: IF=1 (interrupts enabled), IOPL=0 (no IN/OUT)
;     [RSP+24] RSP     ~ user stack pointer (cresce pra baixo na ring 3)
;     [RSP+32] SS      ~ stack segment selector + RPL = 0x23 (GDT[4] + RPL3)
;   A CPU POPa esses 5 valores e faz um far return pra ring 3~
;   Se CS.RPL != 0 (ring != 0), a CPU recarrega SS.RPL também~
;   Isso ativa o privilégio de usuário ~ qualquer instrução privilegiada
;   (LGDT, MOV CR3, IN/OUT se IOPL=0) causa #GP na ring 3~
;   Perfeito pra isolar o kernel dos processos de usuário~ kyun~!
;
; Segment selectors:
;   0x1B = 0x18 | 3 = GDT index 3 (ring 3 code, 64-bit, exec/read) + RPL 3
;   0x23 = 0x20 | 3 = GDT index 4 (ring 3 data, read/write) + RPL 3
bits 64
global enter_user_mode

enter_user_mode:
    ; rdi = RIP (entry point) ~ endereço virtual do user code (ex: 0x200000)
    ; rsi = RSP (user stack top) ~ topo da pilha do usuário (cresce pra baixo)

    ; Carrega segmentos de dados ring 3 (0x23 = ring 3 data segment)~
    ; Se não carregar DS/ES/FS/GS, a CPU pode manter segmentos da ring 0~
    ; O que não quebra (segmentação é desabilitada em 64-bit), mas é boa prática~
    mov rax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Monta frame iretq na pilha do kernel (stack atual ainda é ring 0)~
    ; Ordem: SS, RSP, RFLAGS, CS, RIP (do topo pra base, empilhado ao contrário)~
    push 0x23          ; SS = ring 3 data segment + RPL3
    push rsi           ; RSP do usuario (topo da pilha alocada pro user process)
    pushfq             ; RFLAGS atual (push 8 bytes)
    pop rax            ; recupera em RAX pra modificar
    or rax, 0x200      ; Força IF (Interrupt Flag, bit 9) = 1 ~
                       ; Sem IF=1, o processador ignora INTR e o timer não dispara~
                       ; O scheduler depende do timer IRQ0 pra preempção~
                       ; Se esquecer IF, o processo roda pra sempre~ hihi~!
    push rax           ; RFLAGS modificado de volta
    push 0x1B          ; CS = ring 3 code segment + RPL3 (GDT[3] = 0x18 | RPL3)
    push rdi           ; RIP = entry point do user process

    iretq              ; iretq mágico! CPU POPa RIP, CS, RFLAGS, RSP, SS~
                       ; E continua executando em ring 3 no entry point~
                       ; Nunca mais volta daqui~ fim de linha~ bye bye kernel!



; ♥ enter_umode.asm ~ arquivo fofinho do OvsbMkM! kyun~ <3
