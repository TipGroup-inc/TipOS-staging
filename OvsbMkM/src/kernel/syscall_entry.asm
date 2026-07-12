bits 64
global syscall_handler_entry
extern syscall_handler
extern ring3_exit_rsp_saved

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
    mov r15, rdi    ; save syscall number (r15 is callee-saved in C ABI, preserved by syscall_handler)
    call syscall_handler

    cmp r15, 1      ; SYS_exit?
    je .exit

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

.exit:
    mov rsp, [ring3_exit_rsp_saved]
    ret
