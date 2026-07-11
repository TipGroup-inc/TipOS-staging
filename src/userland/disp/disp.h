#ifndef DISP_H
#define DISP_H
int disp_create_window(int x, int y, int w, int h, const char *title);
void disp_update_mouse(int dx, int dy);
void disp_render(void);
void disp_init(void);
#endif