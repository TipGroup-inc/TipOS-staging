#ifndef MOUSE_H
#define MOUSE_H
#include <stdint.h>
void mouse_init(void);
void mouse_handler(void);
void mouse_process_byte(uint8_t data);
int  mouse_get_x(void);
int  mouse_get_y(void);
int  mouse_get_dx(void);
int  mouse_get_dy(void);
int  mouse_get_buttons(void);
int  mouse_has_moved(void);
#endif
