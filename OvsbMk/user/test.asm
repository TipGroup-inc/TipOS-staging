; moe moe kyun <3
; Minimal ELF64 executable for TipOS — Linux-compatible (GPL v2)
; Uses int 0x80 for syscalls (our kernel's current mechanism)
BITS 64

VIRT_BASE equ 0x400000
org VIRT_BASE

; ELF header
ehdr:
    db 0x7F, "ELF"     ; magic
    db 2                ; 64-bit
    db 1                ; little-endian
    db 0                ; e_ident[6]
    times 8 db 0        ; padding
    dw 2                ; e_type = ET_EXEC
    dw 0x3E             ; e_machine = x86_64
    dd 1                ; e_version
    dq VIRT_BASE + (_start - $$) ; e_entry (absolute VA)
    dq phdr - ehdr      ; e_phoff
    dq 0                ; e_shoff
    dd 0                ; e_flags
    dw ehdr_size        ; e_ehsize
    dw phdr_size        ; e_phentsize
    dw 1                ; e_phnum
    dw 0                ; e_shentsize
    dw 0                ; e_shnum
    dw 0                ; e_shstrndx
ehdr_size equ $ - ehdr

; Program header
phdr:
    dd 1                ; p_type = PT_LOAD
    dd 5                ; p_flags = R+X
    dq 0                ; p_offset
    dq VIRT_BASE        ; p_vaddr
    dq VIRT_BASE        ; p_paddr
    dq filesize         ; p_filesz
    dq filesize         ; p_memsz
    dq 0x1000           ; p_align
phdr_size equ $ - phdr

; Code
_start:
    ; write(1, msg, 6) via int 0x80
    ; TipOS SYS_write = 4, arg1=fd(ebx), arg2=buf(ecx), arg3=count(edx)
    mov eax, 4          ; SYS_write
    mov ebx, 1          ; fd = stdout
    mov ecx, msg        ; buf
    mov edx, 6          ; count
    int 0x80

    ; exit(0) — TipOS SYS_exit = 1
    mov eax, 1          ; SYS_exit
    mov ebx, 0          ; code = 0
    int 0x80

msg: db "Hello", 10

filesize equ $ - ehdr