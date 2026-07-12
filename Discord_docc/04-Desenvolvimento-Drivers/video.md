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

**Em aberto / planejado:**
- VGA graphics 320x200x256 (modo 13h-style) — driver existe (`vga_gfx.c`) mas ainda não integrado ao fluxo principal
- Compositor (8 janelas, title bar, cursor por software) — protótipo em `src/userland/disp/compositor.c`
- Framebuffer via VESA — futuro
- Aceleração gráfica (i915 / Mesa / Vulkan) — bem no futuro (Fase 7 do roadmap em equipe)

Prints e vídeos do compositor ou do modo gráfico rodando também valem post em #apresentações!
