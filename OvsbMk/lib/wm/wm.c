/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ window manager ~ quem manda nas janelinha aqui sou eu!
 * arquivo: wm.c ~ funcoes anotadas: 2
 */
/* ~*~ wm.c ~ "Aloca backbuffer com kmalloc, papo de iniciante mds" ~*~
 * Fiz o modulo do window manager, aloca um backbuffer bonitinho com kmalloc
 * (sim, eu sei, kmalloc pra framebuffer é meio ~gambiarra~ mas funciona!)
 * O wm_flush() copia pixel por pixel pro VESA (tava completamente desalocado
 * das ideia, tava insuportavel de lento). Agora ta top~ kyun! <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#include "wm.h"
#include "../../kernel/memory.h"

static uint32_t *backbuf;   /* ~~ backbuffer ~ onde tudo acontece ~~ */
static uint32_t *framebuf;  /* ~~ framebuffer fisico ~ VESA ou virtio ~~ */
static int scr_w, scr_h, stride; /* ~~ dimensoes da tela ~~ */

/* ~~ Inicializando o WM~ torce pra nao dar panic! ~~
 * Recebe o ponteiro do framebuffer, largura, altura e stride
 * Aloca um backbuffer do tamanho da tela e pinta de roxo escuro (tema dark~
 * porque quem usa tema claro em 2026? affs) */
/* ~ cuidado que essa aqui morde ~ */
void wm_init(uint32_t *fb, int w, int h, int st) {
    framebuf = fb;
    scr_w = w;
    scr_h = h;
    stride = st;
    size_t size = (size_t)w * h * sizeof(uint32_t);
    backbuf = (uint32_t *)kmalloc(size);
    if (backbuf) {
        for (int i = 0; i < w * h; i++)
            backbuf[i] = 0xFF1A1A2E;  /* ~~ roxo escuro ~ cor oficial do TipOS ~~ */
    }
}

/* ~~ Getter do backbuffer ~ "me da o ponteiro ai" ~~ */
uint32_t *wm_get_backbuf(void) { return backbuf; }
/* ~~ Pegando o stride~ só confia que ta certo~ ~~ */
uint32_t wm_get_stride(void) { return (uint32_t)stride; }
/* ~~ Largura da tela~ sem hardcode 1024! ~~ */
int wm_get_scr_w(void) { return scr_w; }
/* ~~ Altura da tela~ sem hardcode 768! ~~ */
int wm_get_scr_h(void) { return scr_h; }

/* ~~ wm_flush ~ joga o backbuffer pro framebuffer de verdade ~~
 * Percorre pixel por pixel (sim, é O(n^2), mas pro TipOS ta bom)
 * Se for virtio-gpu usa o endereco mapeado, senao vai pro 0xFFFFFFFF80000000
 * que é onde o kernel mapeia o VESA pro userspace */
extern int g_virtio_active;
/* ~ essa demorou pra debugar, respeita ~ */
void wm_flush(void) {
    if (!backbuf) return;
    volatile uint32_t *fb_virt;
    if (g_virtio_active)
        fb_virt = (volatile uint32_t *)(uintptr_t)framebuf;
    else
        fb_virt = (volatile uint32_t *)0xFFFFFFFF80000000ULL;
    for (int y = 0; y < scr_h; y++)
        for (int x = 0; x < scr_w; x++)
            fb_virt[y * stride + x] = backbuf[y * (uint32_t)scr_w + x];
}



/* ♥ wm.c ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
