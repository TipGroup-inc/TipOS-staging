# TipOS — Contexto para a gentes (haha a gentes entendeuu?)

## Stack
- **Kernel**: C + NASM (`gcc`, `nasm`) + Zig (`zig build-obj`)
- **Userland**: C freestanding, linkado como Mach-O 64-bit
- **Build**: Makefile raiz em `/home/vinberkuko/repo/TipOS-staging/Makefile`
- **Alvo**: `x86_64-freestanding` (sem libc, sem red-zone, sem SSE/MMX)
- **Emulador**: QEMU (`qemu-system-x86_64`)
- **ISO**: GRUB2 (Multiboot2) via `grub-mkrescue`
- **Disco**: FAT32 64MB (`disk.img`, via `mkfs.fat` + `mtools`)

## Estrutura do Projeto

```
/
├── OvsbMk/          ← KERNEL (ring 0)
│   ├── kernel/      boot64.asm, kernel.c, idt.c, syscall.c, process.c,
│   │                memory.c, vm_map.c, console.c, shell.c, switch.asm,
│   │                syscall_entry.asm, tss.c, utils.c, pic.c, pit.c,
│   │                rtc.c, serial.c, elf64.zig, syscall_linux.zig,
│   │                mach_o.c, owt_app.c, linker.ld
│   ├── kernel/drivers/  keyboard.zig, mouse.zig (drivers em Zig)
│   ├── drivers/     ata.c, pci.c, usb.c, virtio_gpu.c, keyboard_asm.asm
│   ├── fs/          fat32.c, ext2.zig, initramfs.zig
│   ├── lib/
│   │   ├── gui/     vesa.c (framebuffer 1024x768 32-bit)
│   │   ├── owt/     widget toolkit (button, label, textbox, etc.)
│   │   └── wm/      window manager (multi-janela, backbuffer)
│   └── iso/         grub.cfg + kernel.elf para ISO
│
├── src/
│   └── userland/    ← ring 3 (libc, progs/graphy) + repos irmãos ../disp, ../term
│
├── docs/            docs técnicos (KERNEL.md, tutorial de apps, histórico)
├── Discord_docc/    docs do Discord do projeto
└── AGENTS.md        ← este arquivo
```

> **Repos irmãos:** `../disp` (disp-wm, compositor ring 3) e `../term` (terminal) —
> o Makefile raiz os instala no disco via `TIPOS_SDK=$(CURDIR)`.

## 3 Subsistemas

### 1. OvsbMk — Kernel (ring 0)
- **Linguagens**: C (~70%) + ASM (~10%) + Zig (~20%)
- **Boot**: GRUB2 via Multiboot2, setup de long mode 64-bit, PML4 identity mapping
- **Syscalls**: int 0x80 (30 syscalls, convenção XNU: rax=nº, rdi-rsi-rdx-rcx=args)
- **Syscall Linux compat**: syscall_linux.zig traduz Linux→TipOS (read=0→3, write=1→4, exit_group=231→212, etc.)
- **Processos**: Ring 3 com TSS, scheduler RR, PCB estático (64 slots), spawn/exit/waitpid
- **Memória**: Bump allocator (boot), buddy allocator (frames 4KB), SLAB allocator, mmap_user
- **FS**: FAT32 completo (read/write/create/delete/mkdir), ext2 parcial (Zig), initramfs
- **Drivers**: ATA PIO (LBA28), PS/2 keyboard+mouse (Zig), PCI enumeration, Virtio GPU, USB (stub)
- **GUI**: VESA framebuffer 1024x768x32, OWT widgets, WM com backbuffer
- **Shell**: MkM> prompt, 20+ comandos, history, autocomplete, PATH, aliases

### 2. src/userland — Userland (ring 3)
- **Linguagem**: C freestanding
- **libc**: stdio (printf, fopen/fclose/fread/fwrite), stdlib (malloc freelist), string, ctype, tui
- **Programas**: graphy (editor TUI com syntax highlight C); disp-wm e term ficam nos repos irmãos
- **Build**: .c → .elf → .bin → .macho (Mach-O 64-bit, loaded em 0x2000000)

## Novo no projeto? Comece por aqui <3
1. `make kernel && make run` — se o QEMU bootar, o ambiente tá certo
2. Leia `docs/README.md` (índice) e `docs/KERNEL.md` — a vida do kernel em detalhe
3. Quer fazer um programa? `docs/tipos-tutorial.md` e `TUTORIAL_APPS.txt`
4. `README.md` — subsistemas, tabela de syscalls e compat Linux ELF
5. Diagramas UML: `docs/uml/` (PlantUML — arquitetura, boot, syscall, exec, processos)
6. Repos irmãos: `../disp` (compositor) e `../term` (terminal) — precisam estar clonados
7. Cuidado com docs históricos: os marcados como "histórico/arquivado" contam o passado, não o presente

## Como Buildar

```bash
make kernel          # só o kernel (OvsbMk/build/kernel.elf)
make iso             # + ISO bootável (TipOS.iso)
make userland        # compila userland + instala no disk.img
make all             # kernel + ISO
make run             # QEMU (VGA std, serial no terminal)
make run-curses      # QEMU (display curses, sem X11)
```

## Syscalls (int 0x80)
rax=nº, rdi=a1, rsi=a2, rdx=a3, rcx=a4. Retorno em rax.
Tabela completa no README.md ou em kernel/syscall.c.

## Convenções de Código
- C: freestanding, sem libc, sem red-zone, sem SSE/MMX
- Comentários em português (br), estilo moe moe kyun
- Makefiles: $(BUILD_DIR)/%.o: %.c → gcc $(CFLAGS) -c -o $@ $<
- ASM: NASM syntax, elf64, bits 64
- Zig: zig build-obj -target x86_64-freestanding

## Linux ELF Compatibility
- **ELF64 loader** (`elf64.zig`): loads musl static PIE into child PML4, 2MB hugepages
- **Syscall translation** (`syscall_linux.zig`): Linux→TipOS number mapping
- **Aux vector** (`process.c`): `setup_linux_user_stack()` pushes AT_RANDOM, AT_PAGESZ, AT_SECURE, AT_PHNUM, AT_PHENT, AT_PHDR
- **FS.base TLS** (`switch.asm`): save/restore MSR_FS_BASE per process
- **U/S bit mgmt** (`memory.c`): `clone_identity_tables` strips U/S; spawn paths add U/S explicitly
- **Bugfix** (`elf64.zig`): `mapped[32]` VA|PA OR fixed to `(va >> 32) << 32 | phys`
- **Demo**: `exec HELLO` prints "Hello from musl ELF!" (shell_init runs HELLO first, then DISP)

## Teste no QEMU
```bash
make run             # VGA + serial stdio
make run-curses      # modo texto no terminal
make run-test        # headless, log em /tmp/tipos-boot.log
```

## Comandos GIT ´´´ bom, ao menos isso espero que vcs saibam nee ´´´
- Commits com `git commit -m "msg"` (sem assinatura GPG)
- Push: `git push origin master` (token no remote URL)
- AutorA atual: Haruna Himekawa <whimekasyharuna@yahoo.com> # sendme amail
