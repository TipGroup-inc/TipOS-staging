# OvsbMkM — Kernel 64-bit com Ring 3

Kernel 64-bit minimalista com ring 3, TSS, paginação com User bit,
loader Mach-O, FAT32, syscalls int 0x80 e shell TUI completo.

## Status

Ring 3 funcional — programas userland rodam em CPL=3 com:
- GDT: segmentos ring 3 code (0x18, DPL=3) e data (0x20, DPL=3)
- TSS slot (0x28) + ltr + enter_ring3() via iretq
- Paginação: User bit em PML4E/PDPTE (bit 2 = +7) e PDE (0x87)
- SYS_exit: detectado por r15 no syscall handler, pula iretq, retorna ao ring 0
- mmap_user para alocar pilha user (ring 3, RW)
- Mach-O loader bem-sucedido com CPL=3, RIP em 0x2000000+

## Build & Run

```bash
make              # kernel.elf
make iso          # OvsbMkM.iso (GRUB boot)
make run          # QEMU com disk.img
```

Dependências: `nasm`, `gcc`, `grub-mkrescue`, `qemu-system-x86_64`.

## Arquitetura rápida

1. GRUB carrega `build/kernel.elf` via Multiboot2
2. `boot64.asm`: long mode, PML4 identity, GDT (ring 0 + ring 3 + TSS)
3. `kmain()`: VGA, IDT (trap gate int 0x80 DPL=3), PIC, FAT32, TSS
4. Shell loop lê teclado, executa builtins ou `exec` com PATH search
5. `cmd_exec`: fat32_read_file → mach_o_load → mmap_user → enter_ring3
6. `enter_ring3`: salva RSP, monta frame iret (SS=0x23, CS=0x1B, RFLAGS=0x202), iretq
7. Programa userland roda em CPL=3, faz syscalls via int 0x80
8. SYS_exit: handler retorna sem iretq, ring3_exit_rsp_saved restaurado

## Principais arquivos

| Arquivo | Função |
|---------|--------|
| `src/kernel/kernel.c` | kmain, shell, VGA text mode, ANSI parser |
| `src/kernel/ring3.c` | TSS init, enter_ring3(), ring3_exit_rsp_saved |
| `src/kernel/ring3.h` | TSS struct, declarações |
| `src/kernel/boot64.asm` | Boot, GDT (ring0/ring3/TSS), paginação User bit |
| `src/kernel/syscall.c` | 30 syscalls int 0x80 (XNU convention) |
| `src/kernel/syscall_entry.asm` | Entry/exit do syscall, SYS_exit via r15 |
| `src/kernel/mach_o.c` | Loader Mach-O 64-bit |
| `src/kernel/memory.c` | Page allocator, mmap_user/munmap_user |
| `src/kernel/idt.c` | IDT com trap gate DPL=3 para int 0x80 |
| `src/kernel/dyld.c` | Dynamic linker (resolve + bind) |
| `src/drivers/ata.c` | ATA PIO LBA28 read/write com timeout |
| `src/drivers/keyboard.c` | PS/2 scancode → ASCII, shift, extended keys |
| `src/fs/fat32.c` | FAT32 completo (read, write, mkdir, rmdir, rename, stat, cd) |
| `src/commands/shell_cmds.c` | Builtins: help, ls, cat, exec (PATH search), edit... |

## Syscalls

Convenção XNU: `rax=nº, rdi=a1, rsi=a2, rdx=a3, rcx=a4; int $0x80`
Preserva: `rbx, rbp, r12-r15`.

30 syscalls implementadas: exit, read, write, open, close, unlink, access,
getpid, getuid, geteuid, getgid, getegid, ioctl, gettimeofday, sigaction,
sigreturn, mmap, munmap, mprotect, stat, fstat, lstat, kbhit, lseek,
mkdir2, rmdir2.

## FAT32

Driver completo para disco ATA primary master (portas 0x1F0-0x1F7):
- init, read, write, create, delete, mkdir, rmdir, rename, chdir, stat
- Suporte a cluster chains longas
- root_cluster FAT32 (não FAT12/16 fixo)
- name_to_83() com upper-case, padding, dot handling

O disco FAT32 é passado como `-drive file=disk.img,format=raw,index=0`.

## Limitações conhecidas

- Sem scheduler preemptivo (round-robin via PIT pendente)
- Sem fork/execve reais (exec atual roda no mesmo processo)
- Paginação identidade (kernel + user no mesmo espaço 1:1)
- Sem isolamento de kernel (toda memória tem User bit)
- Sem TLB flush na troca de contexto
- syscall/sysret ainda não implementado (usa int 0x80 + iretq)
- Sem sinais reais (stubs)
- Sem rede, sem USB, sem ACPI

## Próximos passos

- [ ] Scheduler preemptivo (PIT/APIC timer, round-robin)
- [ ] PCB + tabela de processos
- [ ] fork + execve + wait reais
- [ ] syscall/sysret (performance)
- [ ] Isolar kernel em página alta
- [ ] Paginação por processo
- [ ] Sinais (SIGKILL, SIGTERM, SIGINT, SIGSEGV)
