/* moe moe kyun <3 */
/* moe moe kyun <3 */
; ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
; arquivo: boot64.asm ~ funcoes anotadas: 0
; ~*~ boot64.asm ~*~
; Hihi, assembly ~ mode difícil ativado!
; Se isso rodar, é milagre~ >_<
; ~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~

; ============================================================
; Ovsb.OS - Boot 64-bit ~ moe moe kyun~!
; "Acorda, PC! Vamos pro long mode, seu lento!"
; ============================================================

bits 32

section .multiboot
align 8
mb2_start:
    dd 0xE85250D6
    dd 0
    dd mb2_end - mb2_start
    dd 0x100000000 - (0xE85250D6 + 0 + (mb2_end - mb2_start))

    ; --- Tag: framebuffer (type 5) ---
    dw 5
    dw 1
    dd 24
    dd 0
    dd 0
    dd 32
    dd 0

    ; --- Tag: end ---
    dw 0
    dw 0
    dd 8
mb2_end:

section .data  ; NOVO: variáveis globais para GRUB info
global saved_magic
global saved_mbinfo
saved_magic:  dd 0
saved_mbinfo: dd 0

section .text
global _start
extern kmain

_start:
    mov esp, stack_top
    mov [saved_magic], eax
    mov [saved_mbinfo], ebx

    ; 1. PAE + SSE (OSFXSR bit 9, OSXMMEXCPT bit 10)
    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)
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
    mov edi, [saved_magic]
    mov esi, [saved_mbinfo]
    call kmain
    cli
    hlt

section .data
align 16
global gdt64
global gdt_tss_slot
gdt64:
    dq 0                         ; 0x00: null
    dq 0x0020980000000000        ; 0x08: ring 0 code
    dq 0x0000920000000000        ; 0x10: ring 0 data
    dq 0x0020FA0000000000        ; 0x18: ring 3 code (0xFA = P,DPL=3,S,code,exec/read)
    dq 0x0000F20000000000        ; 0x20: ring 3 data (0xF2 = P,DPL=3,S,data,read/write)
gdt_tss_slot:
    dq 0, 0                      ; 0x28: TSS descriptor (16 bytes, filled by C)
gdt64_end:
gdt64_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64

section .paging
align 4096
pml4_table:
    dq pdp_table + 7
    times 511 dq 0
pdp_table:
    dq pd_table + 7
    times 511 dq 0
pd_table:
    %assign i 0
    %rep 512
        dq (i * 0x200000) + 0x87    ; 0x87 = Present, R/W, User, 2MB page
        %assign i i+1
    %endrep

section .bss
align 16
stack_bottom: resb 16384
global stack_top
stack_top:



; ♥ boot64.asm ~ se bugar me chama, se n bugar tb me chama ~ >u<
