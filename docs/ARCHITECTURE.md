# Arquitetura do OvsbOS

## Camadas

```text
GRUB2 / Multiboot2
        |
Ovsb K - Ring 0
  boot, GDT, IDT, PIC, TSS, memoria, FAT32, drivers
        |
int 0x80 / paginas compartilhadas
        |
Ovsb WM e Ovsb OWT - Ring 3
  compositor, janelas, cursor e widgets
        |
Aplicacoes - Ring 3
  graphy, terminal e binarios ELF/Mach-O
```

## Boot

1. GRUB carrega o kernel Multiboot2.
2. `boot64.asm` entra em long mode e cria o mapeamento inicial.
3. `idt.c` registra excecoes, IRQs, teclado, mouse e syscall `0x80`.
4. O PIC remapeia IRQ0-15 para os vetores 32-47; IRQ12 fica no vetor 44.
5. O framebuffer Multiboot e validado como VESA/VBE 32 bpp.
6. O LFB e mapeado antes do console e do WM.
7. O kernel cria seu backbuffer e inicia o shell.
8. O shell inicia o compositor em processo Ring 3.

## Video

`OvsbMk/lib/gui/vesa.c` valida endereco, pitch, dimensoes e BPP, depois chama
`pml4_map_phys` para mapear o LFB. O console usa um backbuffer de celulas; o
compositor usa um backbuffer de pixels e chama `disp_flush`. O cursor e
renderizado depois das janelas e da taskbar, antes do flush.

## Processos e privilegios

O kernel e o shell executam em Ring 0 (`CS=0x08`). Processos criados por
`proc_spawn`, `SYS_spawn` ou `SYS_spawn_shared` recebem frame `CS=0x1B` e
`SS=0x23`, portanto executam em Ring 3. O PCB registra se o processo usa a
compatibilidade Linux ELF ou a ABI nativa Mach-O.

A comunicacao de userland com Ring 0 ocorre por syscall. Janelas podem
compartilhar paginas controladas pelo kernel, mas o compositor nao recebe
acesso direto a funcoes internas ou portas de hardware.

## Entrada

O driver PS/2 em `OvsbMk/kernel/drivers/mouse.zig` habilita a segunda porta do
8042, configura IRQ12 e monta pacotes de tres bytes. O cursor recebe deltas e
botoes por `mouse_read`; o compositor limita a posicao ao framebuffer e faz o
hit-test de titlebar, botoes e taskbar.
