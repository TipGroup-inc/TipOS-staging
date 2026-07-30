/* moe moe kyun <3 */
/* moe moe kyun <3 */
🧩 **PROCESSOS — TipOS**

**Status atual: ainda não existe.** O TipOS hoje é **single-address-space**, sem PCB, sem scheduler, sem isolamento entre processos — kernel e userland rodam tudo em ring 0, compartilhando o mesmo espaço de memória.

Isso é o próximo grande marco de kernel (Marco 2 do `tipos-plano.md` / Fase 2 do roadmap em equipe).

---

**O que precisa ser criado do zero:**

**PCB (Process Control Block)** — planejado, ainda sem implementação:
- pid, estado, registradores salvos, page table própria (PML4), prioridade

**Tabela de processos** — estrutura de dados a definir

**Scheduler** — round-robin preemptivo (decisão já tomada em `tipos-dev-stack.md`, seção 6: "Round-robin com prioridades — começar simples")

**Syscalls necessárias:** `fork`, `execve`, `wait`, `waitpid`, `yield`, `getpid` (hoje `getpid` é stub, sempre retorna 1 — `syscall.c:198`), `kill`

**Estados planejados:** RUNNING, READY, BLOCKED, ZOMBIE

---

**Pré-requisito técnico:** paginação por processo (hoje só existe identity mapping de 1GB, sem separação). Sem isso, `fork` real e isolamento de memória não rolam — é bloqueante pra tudo aqui.

**Dependência:** transição Ring 3 → Ring 0 via `syscall`/`sysret` (hoje é `int 0x80`, DPL=3, mas sem TSS configurada pra troca de anel real).

Quem for atacar essa frente, comenta aqui o design antes de começar — é a peça mais estrutural que falta no kernel.
