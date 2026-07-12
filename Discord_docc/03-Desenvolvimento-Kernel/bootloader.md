⚙️ **BOOTLOADER — TipOS**

**Arquivo:** `OvsbMkM/src/kernel/boot64.asm` (87 linhas) + `linker.ld` (31 linhas)

**Fluxo:**
```
MBR → GRUB stage 1+2 → kernel.elf (Multiboot2) → boot64.asm → kmain()
```

**O que `boot64.asm` já faz:**
1. Recebe controle em protected mode 32-bit
2. Monta **PML4 → PDP → PD** com páginas de 2MB (identity mapping do primeiro 1GB)
3. Habilita **PAE**, **long mode** (`EFER.LME = 1`), **paging** (`CR0.PG = 1`)
4. Carrega **GDT** 64-bit (CS/DS)
5. `far jmp` pro código 64-bit
6. Inicializa stack em `0x90000`, zera BSS, chama `kmain()`

**Page tables:** `PML4[0] → PDP[0] → PD[0..511]`, cada `PD[i]` mapeia 2MB → total 1GB identity-mapped. **Não há paginação por processo ainda** — kernel e userland compartilham o mesmo espaço de endereçamento.

---

**Status:** ✅ 100% funcional, considerado "pronto" na migração pro TipOS (vai direto pra `kernel/arch/x86_64/boot.asm`).

**Em aberto / discussão:**
- Suporte a UEFI (hoje só BIOS Legacy via GRUB)
- Avaliar migrar de GRUB pra **Limine** se precisar de UEFI/GOP nativo (`tipos-dev-stack.md`)
- Multiboot2 vs. bootloader próprio

Se for mexer no boot, testa sempre com `make debug` (QEMU + GDB parado em `kmain`) antes de commitar — um erro aqui trava o boot inteiro sem log nenhum.
