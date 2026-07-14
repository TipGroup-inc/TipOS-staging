bits 64
global syscall_handler_entry
extern syscall_handler
extern sigint_pending
extern proc_exit

syscall_handler_entry:
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

    mov r8,  rcx    ; arg4
    mov rcx, rdx    ; arg3
    mov rdx, rsi    ; arg2
    mov rsi, rdi    ; arg1
    mov rdi, rax    ; syscall number
    call syscall_handler

    ; After syscall_handler returns (SYS_exit doesn't return;
    ; proc_exit is called and never returns).
    ; Check sigint_pending (^C was pressed during a syscall)
    cmp dword [sigint_pending], 0
    jne .sigint_kill

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

.sigint_kill:
    mov dword [sigint_pending], 0
    mov rdi, -1      ; exit code = killed by ^C
    call proc_exit   ; never returns
