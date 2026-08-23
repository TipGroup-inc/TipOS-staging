# TipGroup Inc. <3

moe moe kyun~ A gente faz um **sistema operacional** do zero, no estilo mais doido possível: kernel em C + NASM + Zig, rodando x86_64 de verdade no QEMU (e quem sabe numa máquina real um dia, hihi~).

## Projeto principal

**[TipOS-staging](https://github.com/TipGroup-inc/TipOS-staging)** — kernel próprio com:
- Boot GRUB2 (Multiboot2) → long mode 64-bit
- Syscalls via `int 0x80` (30 syscalls, convenção XNU) + compat Linux ELF (roda binário musl estático!)
- Processos ring 3 com TSS, scheduler RR, PCB estático
- FAT32 completo + ext2 em andamento
- Drivers: ATA PIO, PS/2, PCI, Virtio GPU, USB (stub)
- GUI: VESA framebuffer, widget toolkit OWT, window manager
- Userland: libc freestanding, `graphy` (editor TUI), shell com 20+ comandos

## Squads (times)

| Squad | Domínio |
|-------|---------|
| [Core & Memória](https://github.com/orgs/TipGroup-inc/teams/core-memoria) | Kernel, fork/exec, scheduler, memória |
| [DevOps & QA](https://github.com/orgs/TipGroup-inc/teams/devops-qa) | CI, testes, board, docs, build |
| [FS & Terminal](https://github.com/orgs/TipGroup-inc/teams/fs-terminal) | VFS, ext2, TTY, shell, comandos |
| [Rede & Drivers](https://github.com/orgs/TipGroup-inc/teams/rede-drivers) | e1000, lwIP, socket, PCI, USB |
| [Userland & Ferramentas](https://github.com/orgs/TipGroup-inc/teams/userland-ferramentas) | graphy, make, libc, TUI |

## Para entrar

1. Entra no time do squad que te interessa (a gente te aprova rápido)
2. Lê o [board kanban](https://github.com/orgs/TipGroup-inc/projects/8) — card vago não existe, regras em `docs/KANBAN.md`
3. Procura as issues com label **good first issue** no TipOS-staging
4. Leia `AGENTS.md` e `CONTRIBUTING.md` do repo — o onboarding completo está lá

## Regras que não têm discussão (>_<)

- PR obrigatório, review de 2 pares (quando a proteção de branch existir, kyun~)
- Ninguém deleta arquivo sem aprovação de 2 pares
- PR com mais de 20 arquivos precisa reunião (nem pro Coelho, dnv!)
- Comentários em pt-BR, estilo moe moe kyun (padrão do projeto)