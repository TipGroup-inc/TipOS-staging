; moe moe kyun <3
; moe moe kyun <3
; moe moe kyun <3
; ♥ codigo do OvsbMkM ~ mais uma peca do quebra-cabeca! kyun~
; arquivo: user_prog.asm ~ funcoes anotadas: 0
; ~*~ user_prog.asm ~*~
; Hihi, assembly ~ mode difícil ativado!
; Se isso rodar, é milagre~ >_<
; ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

; ♥ User program ring 3 ~ Demonstra syscalls! kyun~
; Esse programa roda em ring 3 (modo usuário) ~ entra via enter_user_mode()~
; Usa int 0x80 pra chamar o kernel ~ syscall numbers em syscall.h~
;   rax = syscall number ~ rdi, rsi, rdx = argumentos (até 3)~
; O kernel retorna resultado em rax~
; 
; Mapa de memória:
;   org 0x200000 ~ carregado no endereço 2MB (acima do kernel 1MB)~
;   stack ~ fornecida pelo kernel no campo user_rsp do PCB~
;   sbrk ~ syscall 4 expande o heap (data segment)~
;
; Syscalls usadas:
;   1 (SYS_write) = write(fd, buf, count) ~ fd=1=stdout~
;   5 (SYS_time) = time() ~ retorna tick counter~
;   4 (SYS_sbrk) = sbrk(increment) ~ retorna novo break~
;   2 (SYS_getpid) = getpid() ~ retorna PID do processo~
;   3 (SYS_read) = read(fd, buf, count) ~ fd=0=stdin, não-blocking~
;   0 (SYS_exit) = exit(status) ~ termina o processo~
org 0x200000
bits 64

start:
    ; ---- write(1, msg1, 23) ----
    ; escreve "Ring 3 syscall test!\n" no console~
    ; rax=1=SYS_write, rdi=1=stdout, rsi=msg1, rdx=23=bytes~
    mov rax, 1
    mov rdi, 1
    mov rsi, msg1
    mov rdx, 23
    int 0x80

    ; ---- time() → test syscall 5 ----
    ; kernel retorna tick counter (ms desde boot) em rax~
    mov rax, 5
    xor rdi, rdi
    int 0x80
    mov [saved_tick], rax

    ; ---- write(1, msg2, 21) ----
    ; escreve "  time() tick=" ~ mostra que time() funcionou~
    mov rax, 1
    mov rdi, 1
    mov rsi, msg2
    mov rdx, 21
    int 0x80

    ; ---- sbrk(4096) → test syscall 4 ----
    ; expande heap em 4096 bytes (1 página)~
    ; retorna endereço base da nova área em rax~
    mov rax, 4
    mov rdi, 4096
    int 0x80
    mov [saved_brk], rax

    ; ---- getpid() → test syscall 2 ----
    ; retorna PID do processo atual (identificador único)~
    mov rax, 2
    int 0x80
    mov [saved_pid], rax

    ; ---- write(1, msg3, 23) ----
    ; escreve "  sbrk/pid test OK!\n" ~ confirma que passou~!
    mov rax, 1
    mov rdi, 1
    mov rsi, msg3
    mov rdx, 23
    int 0x80

    ; ---- Teste de leitura do teclado ----
    ; Tenta ler até 500 vezes (busy-loop com syscall)~
    ; Se alguma tecla for pressionada, lê e faz echo~
    ; Se ESC (0x1B = 27), sai do programa~
    ; Se Enter (0x0A = 10), faz echo de CR e continua lendo~
    ; Se timeout (500 tentativas sem tecla), escreve msg e sai~
    mov r15, 500         ; contador de tentativas ~ timeout de ~500 iterações

read_loop:
    mov rax, 3          ; rax=3=SYS_read
    mov rdi, 0          ; rdi=0=stdin (teclado)
    mov rsi, buf        ; rsi=buffer de 1 byte
    mov rdx, 1          ; rdx=1 byte
    int 0x80

    cmp rax, 1          ; se rax==1, leu um caractere
    je got_char

    dec r15             ; decrementa contador
    jnz read_loop       ; se ainda >0, tenta de novo

    ; Timeout — escreve mensagem e sai
    mov rax, 1
    mov rdi, 1
    mov rsi, timeout_msg
    mov rdx, 20
    int 0x80
    jmp exit

got_char:
    ; Echo the char
    mov rax, 1          ; SYS_write
    mov rdi, 1          ; stdout
    mov rsi, buf        ; o char lido
    mov rdx, 1          ; 1 byte
    int 0x80

    mov al, [buf]       ; carrega o byte lido
    cmp al, 27          ; ESC (0x1B) → exit
    je exit
    cmp al, 10          ; Enter (0x0A) → newline, precisa de CR também
    jne read_loop

    ; Se Enter, escreve carriage return (13) também~
    ; Alguns terminais precisam de CR+LF pra voltar ao início da linha~
    mov rax, 1
    mov rdi, 1
    mov rsi, cr          ; byte 0x0D (CR)
    mov rdx, 1
    int 0x80
    jmp read_loop

exit:
    ; ---- SYS_exit(0) ----
    ; Termina o processo com status 0 (sucesso)~
    ; O kernel remove o PCB da fila e não escalona mais esse processo~
    mov rax, 0           ; SYS_exit
    xor rdi, rdi         ; status = 0
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
