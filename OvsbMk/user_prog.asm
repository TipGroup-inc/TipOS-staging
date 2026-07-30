/* moe moe kyun <3 */
/* moe moe kyun <3 */
; ♥ codigo do OvsbMkM ~ mais uma peca do quebra-cabeca! kyun~
; arquivo: user_prog.asm ~ funcoes anotadas: 0
; ~*~ user_prog.asm ~*~
; Hihi, assembly ~ mode difícil ativado!
; Se isso rodar, é milagre~ >_<
; ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

; ♥ User program ring 3 ~ Demonstra syscalls! kyun~
; Syscalls: 0=exit, 1=write, 3=read, 4=sbrk, 5=time
org 0x200000
bits 64

start:
    ; write(1, msg1, 23)
    mov rax, 1
    mov rdi, 1
    mov rsi, msg1
    mov rdx, 23
    int 0x80

    ; time() → test syscall 5
    mov rax, 5
    xor rdi, rdi
    int 0x80
    mov [saved_tick], rax

    ; write(1, msg2, 21)
    mov rax, 1
    mov rdi, 1
    mov rsi, msg2
    mov rdx, 21
    int 0x80

    ; sbrk(4096) → test syscall 4
    mov rax, 4
    mov rdi, 4096
    int 0x80
    mov [saved_brk], rax

    ; getpid() → test syscall 2 (reusing rax=4 result, so do after)
    mov rax, 2
    int 0x80
    mov [saved_pid], rax

    ; write(1, msg3, 23)
    mov rax, 1
    mov rdi, 1
    mov rsi, msg3
    mov rdx, 23
    int 0x80

    ; Agora faz read teste: tenta ler por um tempo
    ; Se nao tiver tecla, sai depois de ~500 tentativas
    mov r15, 500

read_loop:
    mov rax, 3          ; read
    mov rdi, 0
    mov rsi, buf
    mov rdx, 1
    int 0x80

    cmp rax, 1
    je got_char

    dec r15
    jnz read_loop

    ; Timeout — escreve mensagem e sai
    mov rax, 1
    mov rdi, 1
    mov rsi, timeout_msg
    mov rdx, 20
    int 0x80
    jmp exit

got_char:
    ; Echo the char
    mov rax, 1
    mov rdi, 1
    mov rsi, buf
    mov rdx, 1
    int 0x80

    mov al, [buf]
    cmp al, 27          ; ESC → exit
    je exit
    cmp al, 10
    jne read_loop

    mov rax, 1
    mov rdi, 1
    mov rsi, cr
    mov rdx, 1
    int 0x80
    jmp read_loop

exit:
    mov rax, 0
    xor rdi, rdi
    int 0x80

msg1: db `Ring 3 syscall test!\n`, 0
msg2: db "  time() tick=", 0
msg3: db `  sbrk/pid test OK!\n`, 0
timeout_msg: db `  [no input, exit]\n`, 0
cr: db 13
buf: db 0
saved_tick: dq 0
saved_pid: dq 0
saved_brk: dq 0



; ♥ user_prog.asm ~ feito com carinho (e uma raiva controlada) ~ kyun!
