<!-- moe moe kyun <3 -->
🏗️ **ARQUITETURA — TipOS**

**Base:** OvsbMkM — x86-64, long mode, ring 0, GRUB/Multiboot2, **single-address-space** (kernel e userland compartilham o mesmo espaço de endereçamento — ainda não há ring 3 nem proteção de página).

```
┌─────────────────────────────────────────┐
│             USERLAND (ring 0)            │
│   graphy (editor)  |  bash (embedded)    │
│   ls (embedded)    |  dyld (embedded)    │
└──────────────┬────────────────────────────┘
               │ int 0x80 (convenção XNU)
┌──────────────┴────────────────────────────┐
│             KERNEL (ring 0)               │
│   syscall dispatcher → 30 syscalls        │
│   VGA (ANSI) | Teclado | ATA | FAT32      │
│   Shell (history, line edit, PATH, >)     │
│   PIT timer | RTC | Compositor            │
└─────────────────────────────────────────┘
```

**Monolítico por enquanto** — drivers, FS e shell tudo inline no kernel. A migração pra microkernel (drivers em user-space via IPC) é o objetivo de médio prazo (ver #ideias → Doca/Dock HAL).

---

**Boot step-by-step:**
```
MBR → GRUB stage 1+2 → kernel.elf (multiboot2) → boot64.asm → kmain()
```
`boot64.asm`: protected mode 32-bit → PML4/PDP/PD (páginas de 2MB, identity map 1GB) → PAE → long mode (EFER.LME) → paging (CR0.PG) → GDT 64-bit → far jmp → stack em `0x90000` → zera BSS → `kmain()`.

`kmain()` inicializa nesta ordem (importa!): IDT → PIC → syscall gate → IRQ1 teclado → PIT (100Hz) → heap (bump allocator 4MB) → `sti` → SMC/NVRAM stubs → serial COM1 → ATA → FAT32 → `shell_loop()` (nunca retorna).

---

**Layout de memória:**
```
0x00100000  Kernel ELF (1MB+)
0x00200000  Page tables (PML4/PDP/PD)
0x00900000  Stack do kernel
0x00800000–0xC00000  Heap (bump allocator, 4MB)
0xB8000     VGA text buffer (80×25)
0x02000000  Programas userland carregados aqui
```

---

**Decisões arquiteturais atuais:**
- IPC: ainda inexistente no kernel atual → planejado estilo Mach (portas, mensagens, memória compartilhada)
- Syscalls: `int 0x80`, convenção XNU (RAX=num, RDI/RSI/RDX/RCX=args) → migração futura pra `syscall`/`sysret`
- Binário nativo: **Mach-O 64-bit** (`LC_SEGMENT_64` + `LC_MAIN`), slide base `0x2000000`
- Versionamento: `v0.STAGE.RELEASE.FEATURE`

**Estrutura de diretórios (alvo, `tipos-plano.md`):**
```
tipos/
├── kernel/arch/x86_64/   (boot, idt, pic, syscall_entry)
├── kernel/init/kmain.c
├── kernel/mm/pmm.c
├── kernel/ipc/mach_ipc.c      (novo)
├── kernel/sched/scheduler.c   (novo)
├── kernel/syscall/syscall.c
├── drivers/ (vga.c, ps2_keyboard.c)
├── apps/ (shell.c, tui.c, edit.c)
├── libs/libc/, libs/libtipos/
└── build/ (Makefile, kernel.ld, grub.cfg)
```

**Build & deploy:** `make` → kernel + userland + ISO + disco → `make run` (QEMU + serial) / `make run-curses`.

Discussão aberta: quando migrar de single-address-space pra paginação isolada por processo (ring 3 + TSS)? Isso destrava fork/exec real.
