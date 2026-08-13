# docs/ — Índice da Documentação

> moe moe kyun <3 — aqui mora a sabedoria do TipOS. Leia o que é **atual** primeiro;
> os docs marcados como **histórico** contam o passado (não refletem o código de hoje).

## 📖 Por onde começar (novato)

| Ordem | Doc | Pra quê |
|-------|-----|---------|
| 1 | `../README.md` | Visão geral, subsistemas, syscalls, Linux ELF compat |
| 2 | `../AGENTS.md` | Contexto rápido + como buildar + convenções |
| 3 | `KERNEL.md` | **A bíblia do kernel** — boot, IDT, syscalls, drivers, FS, shell, ELF64 |
| 4 | `KANBAN.md` | **Board + regras de fluxo** (Kanban/Lean + XP, squads, sprints) |
| 5 | `tipos-tutorial.md` | Como escrever/compilar/rodar programas pro TipOS |
| 6 | `../TUTORIAL_APPS.txt` | Protocolo dos apps gráficos (DISP, memória compartilhada) |

## ✅ Atual

- **`KERNEL.md`** — kernel em detalhe (arquitetura, boot, IDT, syscalls, VGA/VESA,
  teclado, ATA, FAT32, PIT, RTC, shell, userland, ELF64 loader, debugging). ~1570 linhas de amor.
- **`tipos-tutorial.md`** — passo a passo de programas userland (libc própria, Mach-O).
- **`tipos-dev-stack.md`** — fluxo de desenvolvimento e stack (ferramentas, build).

## 📊 UML — Diagramas do projeto

Gerados com **PlantUML** a partir dos `.puml` (edite o `.puml` e rode `plantuml -tpng <arquivo>` pra regenerar):

| Diagrama | O que mostra |
|----------|--------------|
| `uml/arquitetura.png` | Componentes: kernel OvsbMk, userland, repos irmãos, QEMU |
| `uml/boot.png` | Sequência: GRUB → boot64.asm → kmain → shell_init (HELLO + DISP) |
| `uml/syscall.png` | Caminho do ring 3 ao ring 0 (int 0x80, XNU, tradução Linux) |
| `uml/exec_elf64.png` | Exec de ELF64 musl (child PML4, hugepages, aux vector) |
| `uml/processos.png` | Ciclo de vida dos processos (PCB, scheduler RR) |
| `uml/kernel_modulos.png` | Módulos do kernel e dependências (drivers, fs, lib) |
| `uml/deploy.png` | Build (make all/userland) → ISO + disk.img → QEMU |

Exemplos diretos: `![Arquitetura](uml/arquitetura.png)`

## 🕰️ Histórico / arquivado

> Documentos da época da migração OvsbMkM → TipOS, do planejamento ou de subsistemas
> que já morreram. Servem de contexto histórico — **não** use como referência de código.

- **`Kora os Doc v1.md`** — plano de desenvolvimento do projeto (antes se chamava "Kora os").
- **`tipos-plano.md`** — plano de migração OvsbMkM → TipOS (checklist concluída).
- **`tipos-vision.md`** — visão do sistema na fase de organização.
- **`tipos-equipes-e-estrutura.md`** — organização do código por equipes (fase inicial).
- **`tipos-doca.md`** — arquitetura da **Doca (Dock HAL)**; `src/dock` foi removido do repo.

## 🧹 Manutenção

- Ao mudar estrutura de diretórios ou APIs, atualize `KERNEL.md` e `../AGENTS.md`.
- Docs novos entram neste índice com o status correto (atual/histórico).
- Padrão de escrita: português BR, estilo moe moe kyun <3
