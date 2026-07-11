# TipOS — Plano de Desenvolvimento

Base: OvsbMkM (https://github.com/Bugsappetit-inc/OvsbMkM) — kernel 64-bit com boot, IDT, PS/2, VGA, syscalls XNU, Mach-O.

**Filosofia:** Sistema mínimo, TUI, focado em desenvolvimento de software (editor, terminal, compilador). Nada de janelas complexas, GPU, áudio, rede por enquanto.

---

## 1. Fundação (sem mexer em quase nada)

O código original do OvsbMkM **já funciona** — boota no QEMU, mostra "OVSBMKM 64-bit", aceita comandos. Toda a migração é mais organizacional que funcional.

### Estrutura de diretórios (simplificada)

```
tipos/
├── kernel/                    # Ring 0
│   ├── arch/x86_64/
│   │   ├── boot.asm
│   │   ├── idt.c / idt_asm.asm
│   │   ├── pic.c
│   │   └── syscall_entry.asm
│   ├── include/
│   ├── init/kmain.c
│   ├── mm/pmm.c
│   ├── ipc/mach_ipc.c          # Novo — IPC mínimo
│   ├── sched/scheduler.c       # Novo
│   └── syscall/syscall.c       # Expandido
├── drivers/
│   ├── vga.c                   # Separado do kmain
│   └── ps2_keyboard.c
├── apps/
│   ├── shell.c                 # Mini-shell expandido
│   ├── tui.c                   # Novo — terminal multiplexado
│   └── edit.c                  # Novo — editor de texto
├── libs/
│   ├── libc/                   # printf, sprintf, strlen, mem*
│   └── libtipos/                # syscall wrappers
├── build/
│   ├── Makefile
│   ├── kernel.ld
│   └── grub.cfg
└── iso/
```

---

## 2. O código que está pronto e o que precisa mudar

| O que | Status | Ação |
|-------|--------|------|
| boot.asm, IDT, PIC, syscall_entry | ✅ Pronto | Copiar igual |
| kmain.c (VGA + shell) | ⚠️ Misturado | Separar VGA → driver, shell → app |
| keyboard.c, keyboard_asm.asm | ✅ Pronto | Copiar igual |
| syscall.c (15 syscalls) | ⚠️ Básico | Manter + adicionar spawn, exec, yield |
| memory.c (bump allocator) | ⚠️ Básico | Manter, melhorar depois |
| mach_o.c (carregador) | ⚠️ Básico | Manter, ELF virá depois |
| SMC / NVRAM | 🔸 Stub | Manter como placeholder |
| Linker / Makefile / GRUB | ✅ Pronto | Copiar, ajustar caminhos |

**A primeira versão do TipOS é o OvsbMkM reorganizado — não reescrito.**

---

## 3. Roadmap (uma pessoa)

### Marco 1 — Organização (1-2 dias)
- [ ] Criar estrutura `tipos/` com diretórios
- [ ] Copiar arquivos do OvsbMkM para os lugares certos
- [ ] Separar VGA de kmain.c → `drivers/vga.c`
- [ ] Separar shell de kmain.c → `apps/shell.c`
- [ ] Adaptar Makefile com novos caminhos
- [ ] Testar `make run` — mostrar "TipOS" no VGA

### Marco 2 — Processos e IPC (1-2 semanas)
- [ ] IPC mínimo: portas e mensagens (base Mach)
- [ ] Escalonador round-robin simples
- [ ] Syscall spawn → criar processo user-space
- [ ] Shell rodando como processo separado

### Marco 3 — Terminal TUI (1 semana)
- [ ] Multiplexar terminal VGA em múltiplas "janelas" de texto
- [ ] Teclado via IPC
- [ ] Prompt funcional com PATH
- [ ] `help` com lista de comandos

### Marco 4 — Editor de Texto (1 semana)
- [ ] Editor modal (inspirado no micro/nano)
- [ ] Salvar/carregar (RAM disk primeiro)
- [ ] Destaque de sintaxe básico (C)

### Marco 5 — Armazenamento (2 semanas)
- [ ] RAM disk + servidor VFS
- [ ] FAT32 (leitura)
- [ ] `ls`, `cat`, `echo >` funcional

### Marco 6 — Carregador ELF (1-2 semanas)
- [ ] Fazer ELF loader rodar
- [ ] Executar binários ELF estáticos (busybox?)
- [ ] Compatibilidade Linux básica

### Marco 7 — Auto-hospedagem (meta final)
- [ ] Compilar um .c com um compilador portado
- [ ] Rodar o editor como app nativa
- [ ] O SO é usável para desenvolvimento

---

## 4. Milestone atual (IMEDIATO)

**Fazer o OvsbMkM reorganizado bootar com o nome novo.**

É o menor passo possível que ainda é progresso real:
1. Cria a estrutura
2. Move os arquivos
3. Ajusta nomes
4. `make run` mostra a splash "TipOS" + prompt

Depois disso, decide qual marco atacar: IPC? Terminal? Editor?

---

Quer começar? Posso montar a estrutura agora — copiar os arquivos do OvsbMkM, criar os diretórios e fazer o `make run` funcionar em menos de 5 minutos.
