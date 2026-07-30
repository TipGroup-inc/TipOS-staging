; moe moe kyun <3
; moe moe kyun <3
; moe moe kyun <3
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

; ♥ Cabeçalho Multiboot2 (spec 0.6.96) ~ o bootloader lê isso pra saber como nos bootar ~
; Magic: 0xE85250D6 (Multiboot2) ~ Flags = 0 (nada especial) ~
; Tamanho total + checksum (0x100000000 - total) pra verificar integridade do header~
; O GRUB2 verifica que magic + flags + tamanho + checksum = 0 (mod 2^32)~
section .multiboot
align 8
mb2_start:
    dd 0xE85250D6
    dd 0
    dd mb2_end - mb2_start
    dd 0x100000000 - (0xE85250D6 + 0 + (mb2_end - mb2_start))

    ; --- Tag: framebuffer (type 5) ~ pede framebuffer linear pro bootloader ---
    ; dw 5 = tag type, dw 1 = flags (preferred), dd 24 = tag size
    ; dd 0 = width (0 = any), dd 0 = height (0 = any), dd 32 = bpp, dd 0 = reserved
    dw 5
    dw 1
    dd 24
    dd 0
    dd 0
    dd 32
    dd 0

        ; --- Tag: end (type 0) ~ termina o cabeçalho Multiboot2 ---
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

; ♥ _start ~ entry point 32-bit chamado pelo GRUB2 (Multiboot2) ~
; eax = 0x36D76289 (magic Multiboot2) ~ se for outro valor, não foi GRUB quem bootou~
; ebx = ponteiro físico (32-bit, abaixo de 4GB) pra struct multiboot_info_t ~
;   Essa struct contém tags: memory map, framebuffer info, modules, etc~
;   kmain() recebe saved_magic e saved_mbinfo e parseia as tags~
; Stack: GRUB pode ter deixado RSP em estado indefinido ~ setamos stack_top (16KB .bss)~
_start:
    mov esp, stack_top
    mov [saved_magic], eax
    mov [saved_mbinfo], ebx

    ; 1. PAE (bit 5) + OSFXSR (bit 9, FXSAVE/FXRSTOR) + OSXMMEXCPT (bit 10, SIMD #XM)~
    ; PAE (Physical Address Extension) é OBRIGATÓRIO pro Long Mode (AMD e Intel)~
    ; Sem PAE, o processador nem tenta entrar em 64-bit~ tenta e da GPF~ hihi~
    ; OSFXSR habilita SSE instructions (movaps, paddb, etc.) ~ FXSAVE/FXRSTOR~
    ; OSXMMEXCPT habilita SIMD floating-point exception handling (#XM)~
    mov eax, cr4
    or eax, (1 << 5) | (1 << 9) | (1 << 10)
    mov cr4, eax

    ; 2. PML4 (Page Map Level 4) ~ carrega o endereço físico da PML4 table em CR3 ~
    ; CR3 aponta pra PML4 (4KB alinhada) ~ estrutura de 4 níveis:
    ;   PML4 → PDP → PD → PT (cada um 512 entradas de 8 bytes = 4KB)
    ; Aqui usamos 2MB huge pages (PS=1 no PD) ~ mapeia 512×2MB = 1GB identity~
    ; Entrada 0 da PML4 → PDP[0] → PD[0..511] cobre endereços 0x00000000-0x3FFFFFFF~
    ; Sem distinction entre user/kernel por enquanto ~ tudo identity mapped~
    mov eax, pml4_table
    mov cr3, eax

    ; 3. Long Mode Enable (LME) no MSR IA32_EFER (0xC0000080) ~
    ; EFER.LME (bit 8) = 1 ativa Long Mode no processador~
    ; EFER.LMA (bit 10) vai ser setado pela CPU quando paging for ativado~
    ; É OBRIGATÓRIO setar LME ANTES de ligar paging (CR0.PG)~
    ; Se inverter a ordem, GPF imediato~ já vi isso acontecer~ hihi~
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; 4. Ativar Paging (PG=bit31) + Protected Mode Enable (PE=bit0) no CR0 ~
    ; 0x80000001 = CR0.PG | CR0.PE ~
    ; A ordem importa: LME já setado, PML4 já em CR3, PG pode ser ativado~
    ; No momento do mov cr0, a CPU valida o page table hierarchy:
    ;   CR3 → PML4[0] → PDP[0] → PD[0] (2MB page, Present=1) ~
    ; Se PML4 for inválida (ex: endereço não alinhado), #INV (triple fault)~
    mov eax, cr0
    or eax, 0x80000001
    mov cr0, eax

    ; 5. Carregar GDT 64-bit (Global Descriptor Table) ~
    ; Necessário pra ter segmentos 64-bit (code ring 0, data ring 0, ring 3 code/data)~
    ; lgdt carrega GDTR com base + limite da tabela GDT ~
    ; Após lgdt, CS ainda é 32-bit ~ precisa de far jump pra recarregar CS~
    lgdt [gdt64_ptr]

    ; 6. Far jump para 64-bit (long jump) ~
    ; 0x08 = seletor GDT[1] (ring 0 code, 64-bit, exec/read)~
    ; start64 é o label no segmento .text ~ RIP relativo a 0 (identity)~
    ; Esse far jump recarrega CS com o descritor 64-bit, ativando Long Mode~
    jmp 0x08:start64

bits 64
extern syscall_entry
start64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, stack_top

    ; ~~ MSR setup for 'syscall' instruction (Linux x86_64 ABI) ~~
    ; Habilita IA32_EFER.SCE (Syscall Enable, bit 0)~
    ; Se não ligar isso, 'syscall' dá #UD (undefined instruction)~
    ; E o musl chora~ ninguém merece~ >_<
    mov ecx, 0xC0000080      ; IA32_EFER
    rdmsr
    or eax, 1                ; bit 0 = SCE (Syscall Enable)
    wrmsr

    ; IA32_STAR (0xC0000081):
    ;   bits 47:32 = SYSCALL_CS (kernel CS = 0x08, SS = CS+8 = 0x10)
    ;   bits 63:48 = SYSRET base (não usado — usamos iretq pra retornar)
    ; STAR = 0x0000_0008_0000_0000
    ;   EDX[15:0] = bits 47:32 = 0x0008 (SYSCALL_CS)
    ;   EDX[31:16] = bits 63:48 = 0x0000 (don't care)
    mov ecx, 0xC0000081
    xor eax, eax               ; low 32 bits = 0
    mov edx, 0x00000008        ; high: EDX[15:0] = 0x0008 = SYSCALL_CS
    wrmsr

    ; IA32_LSTAR (0xC0000082) = RIP do handler 'syscall'~
    ; O pulo mais importante do kernel~ carrega o endereço de syscall_entry~
    mov ecx, 0xC0000082
    mov rax, syscall_entry
    mov rdx, rax
    shr rdx, 32
    wrmsr

    ; IA32_FMASK (0xC0000084) = bits de RFLAGS pra limpar ao entrar~
    ; Limpa IF (bit 9) e DF (bit 10) — sem interrupção durante syscall~
    mov ecx, 0xC0000084
    xor eax, eax
    mov eax, (1 << 9) | (1 << 10)  ; IF + DF
    xor edx, edx
    wrmsr

    mov edi, [saved_magic]
    mov esi, [saved_mbinfo]
    call kmain
    cli
    hlt

section .data
align 16
global gdt64
global gdt_tss_slot
; ♥ GDT 64-bit ~ Global Descriptor Table pro Long Mode ~
; No x86-64, segmentação é quase "legacy" (desabilitada pelo CS.L=1)~
; Mas ainda precisamos de descritores minimamente válidos pra:
;   - CS (code segment) com L=1 (64-bit), D/B=0 (32-bit default off)
;   - SS/DS/ES/FS/GS com W=1 (writable) pra ring 3~
;   - DPL (Descriptor Privilege Level) certo pra ring 3 ~ se errar, #GP~
; Formato do descritor de 8 bytes (64-bit):
;   bits 0-15  = limit (ignorado em 64-bit)
;   bits 16-31 = base (ignorado em 64-bit)
;   bits 32-39 = base[24..31] + flags
;   bits 40-43 = type (code=0xA, data=0x2, etc)
;   bits 44-46 = S, DPL, P
;   bits 47-55 = mais flags (L=bit 53, D/B=bit 54)
;   bits 56-63 = base[32..39]
gdt64:
    dq 0                         ; 0x00: null descriptor ~ obrigatório, CPU espera entry 0 = 0
    dq 0x0020980000000000        ; 0x08: ring 0 code ~ P=1, DPL=0, S=1, type=code+exec/read, L=1
    dq 0x0000920000000000        ; 0x10: ring 0 data ~ P=1, DPL=0, S=1, type=data+read/write
    dq 0x0020FA0000000000        ; 0x18: ring 3 code ~ P=1, DPL=3, S=1, type=code+exec/read, L=1
    dq 0x0000F20000000000        ; 0x20: ring 3 data ~ P=1, DPL=3, S=1, type=data+read/write
gdt_tss_slot:
    dq 0, 0                      ; 0x28: TSS descriptor (16 bytes) ~ preenchido em C por tss_init()
                                 ; TSS é necessário pra IST (Interrupt Stack Table) e I/O Bitmap~
gdt64_end:
; ♥ GDTR pra lgdt ~ 6 bytes: 2 bytes limit + 8 bytes base (mas lgdt lê só 10 bytes total)
gdt64_ptr:
    dw gdt64_end - gdt64 - 1     ; limit = tamanho total - 1 (0 a 5 = 6 entries?)
    dq gdt64                     ; base = endereço físico (identity mapped, então funciona)

; ♥ Page tables 4-level paging (identity map 0x00000000 - 0x3FFFFFFF) ~
; PML4: 512 entradas × 8 bytes = 4KB ~ entrada 0 → PDP, resto = 0 (not present)
; PDP:  512 entradas × 8 bytes = 4KB ~ entrada 0 → PD, resto = 0
; PD:   512 entradas × 8 bytes = 4KB ~ cada entrada = 2MB huge page (PS=1)
; Flags nas entradas: 0x07 = Present (P=bit0) | R/W (bit1) | User (bit2)
;   0x80 = PS (bit7, Page Size) = 1 → 2MB page (não precisa de PT)
;   Total: 0x87 = P|R/W|U|PS
; Mapeamento: PD[i] → endereço (i * 0x200000) + 0x87~
;   PD[0] → 0x00000000 + 0x87 (primeiro 2MB: 0x000000-0x1FFFFF)~
;   PD[1] → 0x00200000 + 0x87 (2MB: 0x200000-0x3FFFFF)~
;   ... até PD[511] → 0xFFE00000 + 0x87 (último 2MB do 1GB)~
; 512 × 2MB = 1GB identity mapped ~ suficiente pro kernel, framebuffer, etc~
section .paging
align 4096
pml4_table:
    dq pdp_table + 7              ; PML4[0] = PDP base + 0x07 flags (P|R/W|U)
    times 511 dq 0                ; PML4[1..511] = not present ~ se acessar, #PF
pdp_table:
    dq pd_table + 7              ; PDP[0] = PD base + 0x07 flags (P|R/W|U)
    times 511 dq 0                ; PDP[1..511] = not present
pd_table:
    %assign i 0
    %rep 512
        dq (i * 0x200000) + 0x87    ; 0x87 = Present | R/W | User | PS (2MB page)
        %assign i i+1
    %endrep

section .bss
align 16
stack_bottom: resb 16384
global stack_top
stack_top:



; ♥ boot64.asm ~ se bugar me chama, se n bugar tb me chama ~ >u<
