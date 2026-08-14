# Mudanças Realizadas no TipOS

## Resumo
Correções de compatibilidade para permitir compilação em sistemas com versões mais recentes de GCC/binutils.

## Arquivos Modificados

### 1. Makefile (raiz)
- Repositorios irmaos (disp/term) agora sao opcionais

### 2. OvsbMk/drivers/ata.c
- Corrigido inline assembly: %edx -> %dx (16-bit)

### 3. OvsbMk/drivers/usb.c
- Corrigido inline assembly: %edx -> %dx (16-bit)

### 4. src/userland/Makefile
- Comentarios C -> comentarios Makefile
- PROGS = graphy (adicionado)

### 5. src/userland/libc/link.ld
- Comentarios // -> /* */

### 6. src/userland/tools/macho_pack.py
- Comentarios C -> comentarios Python

### 7. OvsbMk/Makefile
- Caminho do Zig: hardcoded -> $(HOME)

## Resultado
- Kernel compila sem erros
- graphy compilado (24396 bytes)
- TipOS roda no QEMU

## Como testar
make clean
make all
make disk.img
make userland
make run
