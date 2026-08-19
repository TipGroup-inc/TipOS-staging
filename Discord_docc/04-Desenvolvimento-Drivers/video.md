<!-- moe moe kyun <3 -->
🖥️ **VÍDEO — TipOS**

**VGA modo texto 80x25 — já implementado:**
- Buffer: `0xB8000`, 80×25 = 2000 células de 2 bytes (char + atributo de cor)
- Cor padrão: `0x0A` (verde claro em fundo preto) · reverse video: `0x70`
- Cursor de hardware via portas `0x3D4`/`0x3D5`

**Parser ANSI/VT100 — já implementado**, dentro de `vga_putchar()`:

| Sequência | Efeito |
|---|---|
| `\x1b[H` | Cursor home |
| `\x1b[<r>;<c>H` | Posiciona cursor |
| `\x1b[2J` | Limpa tela |
| `\x1b[K` | Limpa até fim da linha |
| `\x1b[7m` / `\x1b[m` | Reverse video / reset |
| `\x1b[?25l` / `\x1b[?25h` | Esconde/mostra cursor |
| `\x1b[A/B/C/D` | Move cursor (setas) |

State machine com 4 variáveis: `esc_state` (0=normal, 1=ESC, 2=CSI, 3=params), `esc_params[4]`, `esc_np`, `esc_question`.

É esse parser que dá o syntax highlighting colorido no editor `graphy` (strings verdes, comentários vermelhos, preprocessor ciano, keywords amarelas, números magenta).

---

**Implementado:**
- **VESA framebuffer 1024×768 32-bit** via Multiboot2 tag (type 5) — driver em `src/lib/libgui/vesa.c`
- **Terminal nativo framebuffer** — `vga_putchar()` renderiza no VESA FB em vez de 0xB8000, com `fb_buf[][]` dinâmico e scroll atômico
- **Compositor VESA** — `disp` usa `vesa_draw_*` com resolução real, 8 janelas, cursor software, fundo azul escuro
- **Renderização atômica** — `vesa_draw_cell()` usa buffer local + memcpy para evitar flicker
- **VGA fallback** — modo 13h (320×200×256) via `vga_gfx.c` quando framebuffer inativo

**Em aberto / planejado:**
- Double buffering (backbuffer em RAM para eliminar tearing de scroll completamente)
- VSYNC wait (eliminar tearing residual)
- Aceleração gráfica (i915 / Mesa / Vulkan) — bem no futuro (Fase 7 do roadmap em equipe)

Prints e vídeos do compositor ou do modo gráfico rodando também valem post em #apresentações!
