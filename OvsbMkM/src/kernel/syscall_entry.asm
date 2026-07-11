bits 64
global syscall_handler_entry
extern syscall_handler

syscall_handler_entry:
    ; Salvar registradores
    push rbp
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    
    ; Passar argumentos para C
    ; XNU: rax=syscall num, rdi=arg1, rsi=arg2, rdx=arg3, rcx=arg4
    mov r8,  rcx    ; arg4
    mov rcx, rdx    ; arg3
    mov rdx, rsi    ; arg2
    mov rsi, rdi    ; arg1
    mov rdi, rax    ; syscall number
    call syscall_handler
    
    ; Restaurar registradores
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rbp
    
    iretq
