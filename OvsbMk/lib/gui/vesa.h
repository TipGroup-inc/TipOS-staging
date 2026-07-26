/* moe moe kyun <3 */
/* ♥ gui/vesa ~ pixel by pixel, sem pressa, kyun~
 * arquivo: vesa.h ~ funcoes anotadas: 0
 */
/* ♥ vesa.h ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ VESA_H ~ "Header VESA framebuffer~"
 * Dica: framebuffer_t é a struct mágica do display~
 * pitch = width * (bpp/8) ~ não confunde! */
#ifndef VESA_H
#define VESA_H

#include <stdint.h>

typedef struct {
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
} framebuffer_t;

int  vesa_init(framebuffer_t *fb);
void vesa_fill_screen(uint32_t color);
void vesa_draw_pixel(int x, int y, uint32_t color);
void vesa_draw_rect(int x, int y, int w, int h, uint32_t color);
void vesa_draw_char(int x, int y, char c, uint32_t color);
void vesa_draw_cell(int x, int y, char c, uint32_t fg, uint32_t bg);
void vesa_draw_text(int x, int y, const char *text, uint32_t color);
void vesa_set_backbuffer(void *buf);
int  vesa_has_backbuffer(void);
void vesa_flush(void);

#endif




/* ♥ vesa.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
