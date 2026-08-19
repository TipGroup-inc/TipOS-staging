/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: mouse.h ~ funcoes anotadas: 0
 */
/* ~*~ mouse.h — Header do driver de mouse PS/2~*~ */
/* "Aqui tem rato!" ~ funcoes pro mouse PS/2
   mouse_get_x()/get_y() dao a posicao atual~
   Botoes: bit 0 = esquerdo, bit 1 = direito~ <3 */
#ifndef MOUSE_H
#define MOUSE_H
#include <stdint.h>
extern int mouse_x, mouse_y;         /* Posicao atual do mouse */
extern int mouse_buttons;            /* Estado dos botoes do mouse */
void mouse_init(void);
void mouse_handler(void);         /* Handler de interrupcao do mouse */
void mouse_process_byte(uint8_t data);  /* Processa byte do PS/2 */
int  mouse_get_x(void);           /* Posicao X do mouse */
int  mouse_get_y(void);           /* Posicao Y do mouse */
int  mouse_get_dx(void);          /* Delta X desde ultima chamada */
int  mouse_get_dy(void);          /* Delta Y desde ultima chamada */
int  mouse_get_buttons(void);     /* Estado dos botoes */
int  mouse_has_moved(void);       /* Mouse se mexeu? */
void mouse_read_delta(int *dx, int *dy, int *buttons);  /* Le delta + botoes de uma vez */
#endif

/* ♥ mouse.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
