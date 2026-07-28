/* moe moe kyun <3 */
⏱️ **ESCALONADOR — TipOS**

**Status atual: não existe.** O TipOS hoje não tem múltiplos processos rodando concorrentemente — é um kernel monolítico com um único fluxo de execução (shell + o que ele chama).

O que já existe é o **timer**, que vai servir de base pro escalonador:

**PIT (Programmable Interval Timer) — já implementado:**
```c
static void pit_init(void) {
    uint32_t div = 1193182 / 100; // 100 Hz
    outb(0x43, 0x36);              // canal 0, modo 3 (square wave)
    outb(0x40, div & 0xFF);
    outb(0x40, (div >> 8) & 0xFF);
}

volatile uint64_t timer_ticks = 0; // incrementa a ~100Hz via IRQ0
```

`sleep_ms()` hoje é **busy-wait** (`while (timer_ticks < target) pause;`) — sem scheduler, a CPU fica 100% ocupada durante qualquer sleep. Isso muda assim que o escalonador existir.

---

**Decisão já tomada (`tipos-dev-stack.md`):**
> Round-robin com prioridades — começar simples. Alternativas descartadas por ora: lottery scheduling, O(1).

**Plano (Fase 2, junto com #processos):**
- `kernel/sched/scheduler.c` (novo)
- Time-slice via PIT (já rodando a 100Hz — dá ~10ms por tick, base natural pro slice)
- Troca de contexto: salvar/restaurar registradores (ainda não implementado — depende de PCB existir)
- Fila de prontos (READY) e bloqueados (BLOCKED)
- Prioridades + nice
- CPU affinity — só relevante quando/se o TipOS for multi-core (bem no futuro)

**Bloqueio direto:** escalonador depende do PCB (#processos) existir primeiro — sem estrutura de processo, não tem o que escalonar.

Quem curte scheduling, essa e #processos andam juntas — bora desenhar as duas em conjunto.
