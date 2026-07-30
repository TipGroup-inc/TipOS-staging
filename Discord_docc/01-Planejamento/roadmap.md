/* moe moe kyun <3 */
/* moe moe kyun <3 */
📍 **ROADMAP — TipOS**

Base do projeto: **OvsbMkM** (kernel 64-bit, monolítico, single-address-space) → evoluindo pra **TipOS** (userland MIT + kernel reorganizado).

🔗 Docs completas no repo: `KERNEL.md`, `tipos-plano.md`, `tipos-dev-stack.md`

---

**Estado atual (v0.5.0.0):**
> STAGE 5 = Editor maduro + RTC + timer + repeat + shell editing + history + PATH + redirect + syntax highlight + undo + clipboard

Já funciona: boot GRUB/Multiboot2 → long mode → 30 syscalls (convenção XNU) → VGA com parser ANSI → teclado PS/2 com repeat → ATA PIO → FAT32 completo → shell `MkM>` com history/autocomplete → editor TUI `graphy`.

---

**🗺️ Marcos (tipos-plano.md — 1 pessoa):**

| Marco | Escopo | Prazo estimado |
|---|---|---|
| 1 — Organização | Reestruturar OvsbMkM → `tipos/`, separar VGA/shell de `kmain.c` | 1-2 dias |
| 2 — Processos e IPC | IPC mínimo (Mach), scheduler round-robin, `spawn` | 1-2 semanas |
| 3 — Terminal TUI | Multiplexação de janelas de texto, teclado via IPC | 1 semana |
| 4 — Editor de Texto | Editor modal, syntax highlight básico | 1 semana |
| 5 — Armazenamento | VFS + FAT32 real (`ls`, `cat`, `echo >`) | 2 semanas |
| 6 — Carregador ELF | Rodar binários ELF estáticos (busybox) | 1-2 semanas |
| 7 — Auto-hospedagem | Compilar `.c` dentro do próprio TipOS | meta final |

**🎯 Milestone imediato:** OvsbMkM reorganizado bootando com o nome "TipOS" (splash + prompt).

---

**🧵 Roadmap com times (tipos-equipes-e-estrutura.md — versão em equipe, 8 fases / 40 semanas):**

Fase 1 Fundações → Fase 2 Processos/Memória → Fase 3 Armazenamento → Fase 4 Rede → Fase 5 Compat. Linux (ELF) → Fase 6 Modos de Performance → Fase 7 Vídeo/Áudio → Fase 8 Virtualização.

Detalhe completo de cada fase e dependências entre times: ver `tipos-equipes-e-estrutura.md`, seção 5.

---

**Matriz de dependências (resumo):**
```
boot.asm → kmain() → idt_init() → pic_init() → syscall gate (int 0x80)
                                              → IRQ1 (teclado)
        → memory_init() → ata_init() → fat32_init() → shell_loop()
```
Ordem importa: PIC antes de habilitar IRQs · ATA antes de FAT32 · FAT32 antes do shell.

---

💬 Toda semana atualizamos esse post com o progresso real. Sugestão de prioridade pra próxima sprint? Comenta aqui 👇
