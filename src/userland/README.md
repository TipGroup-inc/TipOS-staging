# TipOS Userland

Como escrever e executar programas userland no TipOS (kernel OvsbMkM Fase 2).

---

## Quickstart

```bash
make all       # compila kernel + userland + ISO
make run       # sobe QEMU
```

No shell do kernel: `exec HELLO`

---

## Como escrever um programa

### 1. Crie `progs/meuprog.c`

```c
void _start(void) {
    __asm__ volatile (
        "mov $4, %%rax\n\t"          // SYS_write
        "mov $1, %%rdi\n\t"          // fd = stdout
        "lea 1f(%%rip), %%rsi\n\t"   // string
        "mov $13, %%rdx\n\t"         // tamanho
        "int $0x80\n\t"
        "ret\n\t"
        "1: .ascii \"Ola userland!\\n\""
        : : : "rax","rdi","rsi","rdx"
    );
}
```

### 2. Adicione no `Makefile`

```makefile
PROGS = hello meuprog
```

### 3. Compile e instale

```bash
make install
```

### 4. Execute no QEMU

```
OvsbMkM> exec MEUPROG
```

---

## Syscalls (int 0x80, XNU convention)

| #  | Nome     | rax | rdi          | rsi          | rdx         | rcx |
|----|----------|-----|--------------|--------------|-------------|-----|
| 3  | read     | 3   | fd (0)       | buffer       | tamanho     | -   |
| 4  | write    | 4   | fd (1 ou 2)  | string       | tamanho     | -   |
| 5  | open     | 5   | path         | flags        | mode        | -   |
| 6  | close    | 6   | fd           | -            | -           | -   |
| 197| mmap     | 197 | addr (0)     | length       | prot        | flags |
| 73 | munmap   | 73  | addr         | length       | -           | -   |

Registradores preservados: `rbx, rbp, r12-r15`. Demais podem ser alterados.

---

## Formato binário

O kernel carrega **Mach-O 64-bit**:

- **magic**: `0xFEEDFACF`
- **cputype**: `0x01000007` (x86_64)
- **filetype**: `2` (MH_EXECUTE)
- **slide base**: `0x2000000` (33 MB)
- **entry**: via `LC_MAIN` (`entryoff` = offset do código no arquivo)

O empacotador `tools/macho_pack.py` converte flat binary (.bin) em Mach-O.

---

## Fluxo de build

```
meuprog.c
    │ gcc -ffreestanding -nostdlib -static -fPIC -e _start -Wl,-Ttext,0
    ↓
meuprog.elf
    │ objcopy -O binary -j .text
    ↓
meuprog.bin              (flat binary, só .text)
    │ tools/macho_pack.py
    ↓
meuprog.macho            (header + LC_SEGMENT_64 __TEXT + LC_MAIN + code)
    │ mcopy -i disk.img
    ↓
FAT32 ::MEUPROG          (nome 8.3 maiúsculo)
```

Limitação atual: arquivos de até **512 bytes** (FAT32 driver lê 1 setor).

---

## Display Server / WM (planejado)

Em `src/userland/disp/` — a implementar:
- Modo VESA framebuffer (ou VGA modo X)
- Gerenciamento de janelas
- Input de teclado/mouse
- Compositor

Dependências: syscall `mmap` para alocar framebuffer, `read` para input.
