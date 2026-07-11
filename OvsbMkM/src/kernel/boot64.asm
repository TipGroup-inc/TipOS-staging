bits 32

section .multiboot
align 8
multiboot_header:
    dd 0xE85250D6
    dd 0
    dd header_end - multiboot_header
    dd -(0xE85250D6 + 0 + (header_end - multiboot_header))
    dw 0, 0, 8
header_end:

section .text
global _start
extern kmain

_start:
    mov esp, stack_top

    ; 1. PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; 2. PML4
    mov eax, pml4_table
    mov cr3, eax

    ; 3. Long Mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; 4. Paging + Protected Mode
    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax

    ; 5. Carregar GDT 64-bit
    lgdt [gdt64_ptr]

    ; 6. Far jump para 64-bit
    jmp 0x08:start64

bits 64
start64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, stack_top
    call kmain
    cli
    hlt

section .data
align 16
gdt64:
    dq 0
    dq 0x0020980000000000   ; code 64-bit
    dq 0x0000920000000000   ; data
gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64

section .paging
align 4096
pml4_table:
    dq pdp_table + 3
    times 511 dq 0
pdp_table:
    dq pd_table + 3
    times 511 dq 0
pd_table:
    %assign i 0
    %rep 512
        dq (i * 0x200000) + 0x83
        %assign i i+1
    %endrep

section .bss
align 16
stack_bottom: resb 16384
stack_top:
