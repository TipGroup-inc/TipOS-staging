/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ driver ~ conversando com o hardware, seu chato!
 * arquivo: usb.h ~ funcoes anotadas: 0
 */
/* ♥ usb.h — UHCI driver pro QEMU usb-tablet ♥
 * Só o mínimo: init, poll, e atualizar estado do mouse ~~
 * Se não tiver USB, as funções são no-op. <3 */

#ifndef USB_H
#define USB_H

int usb_init(void);              /* Inicializa UHCI + detecta tablet */
void usb_poll(void);             /* Poll USB (chamar no main loop) */
void usb_update_mouse(void);     /* Atualiza estado do mouse a partir do tablet */

#endif

/* ♥ usb.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
