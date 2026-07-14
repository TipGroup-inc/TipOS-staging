# disp-wm — Window Manager para TipOS

Gerenciador de janelas gráfico para o kernel OvsbMkM (TipOS).
Executa como **programa userland (ring 3)** e se comunica com o kernel
através de syscalls dedicadas.

---

## API Contract — `libdisp.h`

O WM depende **apenas** destas 2 syscalls do kernel. Nada mais.

### `libdisp_init()`

```c
int libdisp_init(uint64_t *addr, uint32_t *width,
                  uint32_t *height, uint32_t *pitch);
```

**Syscall:** `200`

Mapeia o framebuffer VESA físico no espaço de endereçamento do processo.
Retorna o endereço virtual mapeado e as dimensões da tela.

| Parâmetro | Saída |
|-----------|-------|
| `addr`    | Ponteiro mapeado para o framebuffer (escrever pixels aqui aparece na tela) |
| `width`   | Largura em pixels (ex: 1024) |
| `height`  | Altura em pixels (ex: 768) |
| `pitch`   | Bytes por linha (ex: 4096) |

**Retorno:** `0` sucesso, `-1` erro (sem framebuffer)

### `libdisp_flush()`

```c
void libdisp_flush(void *backbuffer);
```

**Syscall:** `201`

Copia o backbuffer (fornecido pelo WM) para o framebuffer físico real.
Essa operação precisa de privilégio de kernel (ring 0) porque o
framebuffer físico está mapeado apenas no kernel — o WM userland
não tem acesso direto a ele.

| Parâmetro | Descrição |
|-----------|-----------|
| `backbuffer` | Ponteiro para o buffer de 32bpp do WM (largura × altura × 4 bytes) |

### Demais operações — usam syscalls existentes

| Operação | Syscall existente |
|----------|-------------------|
| Alocar backbuffer | `mmap` (197) com `MAP_ANON` |
| Ler teclado | `read(0, buf, 1)` (3) |
| Verificar tecla | `kbhit` (198) |
| Alocar memória | `mmap` (197), `munmap` (73) |
| Sair | `exit` (1) |

---

## Dependências

1. **TipOS kernel** com as syscalls `disp_get_fb` (200) e `disp_flush` (201)
2. **Toolchain TipOS** — `gcc` com flags `-ffreestanding -nostdlib -mno-red-zone`
3. **TipOS libc** — `link.ld`, `crt0.o` para linkedição do executável

---

## Build

```bash
# Com TIPOS_SDK apontando para a raiz do TipOS
export TIPOS_SDK=/caminho/para/TipOS
make

# O resultado é disp-wm.macho, pronto para copiar para o
# disco FAT32 do TipOS (geralmente em /BIN/)
make install    # copia pra $TIPOS_SDK/disk.img
```

---

## Arquitetura

```
┌──────────────────────────────────────────────────┐
│                  disp-wm (ring 3)                │
│                                                  │
│  ┌─────────────┐  ┌──────────────────────────┐  │
│  │ libdisp.h    │  │     compositor.c          │  │
│  │ ─ syscall 200│  │  ┌────────────────────┐   │  │
│  │ ─ syscall 201│  │  │ janelas (8 max)    │   │  │
│  └──────┬───────┘  │  │ drag / close / foco │   │  │
│         │          │  │ panel + botão [+]   │   │  │
│         │          │  │ cursor render       │   │  │
│         │          │  │ full redraw c/ flush│   │  │
│         │          │  └────────────────────┘   │  │
│         │          └──────────────────────────┘  │
└─────────┼────────────────────────────────────────┘
          │ syscall
┌─────────▼────────────────────────────────────────┐
│               TipOS kernel (ring 0)               │
│                                                  │
│  disp_api.c:                                     │
│  ├─ sys_disp_get_fb() ── mapeia fb + retorna     │
│  └─ sys_disp_flush()  ── memcpy p/ framebuffer   │
│                                                  │
│  vesa.c: framebuffer driver                      │
│  keyboard.c: PS/2 input                          │
└──────────────────────────────────────────────────┘
```

---

## Fluxo de inicialização

1. Usuário digita `disp` no shell
2. Shell executa `/BIN/disp-wm.macho` (userland ring 3)
3. WM chama `libdisp_init()` → syscall 200 → mapeia framebuffer
4. WM aloca backbuffer via `mmap(MAP_ANON)`
5. WM entra no loop: `read(0)` para teclado → desenha → `libdisp_flush()`
6. ESC → WM chama `exit(0)` → volta ao shell

---

## Exemplo mínimo

```c
#include "libdisp.h"
#include <stdint.h>

void _start(void) {
    uint64_t fb_addr;
    uint32_t w, h, pitch;

    if (libdisp_init(&fb_addr, &w, &h, &pitch) < 0) {
        write(1, "no fb\n", 6);
        exit(1);
    }

    uint32_t *fb = (uint32_t *)(uintptr_t)fb_addr;
    uint32_t *back = mmap(0, w * h * 4, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANON, -1, 0);

    // Preenche fundo azul
    for (uint32_t i = 0; i < w * h; i++) back[i] = 0x00001030;

    libdisp_flush(back);
    read(0, (char[1]){0}, 1);  // espera uma tecla
    exit(0);
}
```

---

## Histórico de versões

| Versão | Kernel requerido | Mudanças |
|--------|-----------------|----------|
| 1.0    | TipOS ≥ v0.7.2 | Primeira versão standalone |
