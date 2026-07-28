/* moe moe kyun <3 */
⌨️ **ENTRADA — TipOS**

**Teclado PS/2 — já implementado** (`keyboard.c` + `keyboard_asm.asm`):
- Controlador PS/2: dados `0x60`, status `0x64`, **IRQ1 (vetor 33)**
- Fluxo: `IRQ1 → keyboard_irq_handler (asm) → keyboard_handler() (C) → lê 0x60 → process_scancode() → buffer circular kb_buffer[256] → EOI`
- Scancode Set 1 → ASCII via arrays `norm[]`/`shf[]`. Shift: `0x2A`/`0x36` (press), `0xAA`/`0xB6` (release). Teclas estendidas (`0xE0` prefix) → sequências VT100 (`\x1b[A`, etc.)

**Repeat de tecla — já implementado:**
```c
// held = timer_ticks - press_tick
// se held > REPEAT_DELAY (800ms) e (timer_ticks - last_repeat_tick) >= REPEAT_RATE (12.5Hz):
//   emite repeat
```
Ajustável em `keyboard.c` via `REPEAT_DELAY` e `REPEAT_RATE`.

> ⚠️ **Importante:** a leitura de `0x60` é feita **exclusivamente pelo ISR**. `keyboard_read()` NÃO faz polling direto da porta — isso já causou duplicação de caracteres no passado (ISR e polling lendo o mesmo scancode). Se for mexer aqui, não reintroduza polling da porta.

**Sequências VT100 emitidas** (setas, Home/End, PgUp/PgDn, Ins/Del, F1-F12) — tabela completa no #shell e em `tipos-tutorial.md`.

---

**Em aberto:**
- Mouse PS/2 — futuro
- USB HID — futuro (depende de #usb)

Testou em hardware real e o repeat rate ficou estranho? Posta aqui com o modelo do teclado e o comportamento observado.
