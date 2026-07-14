#include "vga.h"
#include "colors.h"
#include "../lib/gui/vesa.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_ADDR  0xB8000
#define FB_MAX_COLS 160
#define FB_MAX_ROWS 64

framebuffer_t g_fb = {0};
int g_fb_active = 0;
volatile unsigned short *vga = (unsigned short *)VGA_ADDR;
int cx = 0, cy = 0;
uint8_t vga_attr = 0x07;

static int fb_cols = 80, fb_rows = 25;
static uint8_t fb_attr = 0x07;
static int fb_cur_visible = 1;
static struct { char ch; uint8_t attr; } fb_buf[FB_MAX_ROWS][FB_MAX_COLS];

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

static uint32_t vga_to_rgb(uint8_t attr) {
    static const uint32_t pal[16] = {0x000000,0x0000AA,0x00AA00,0x00AAAA,0xAA0000,0xAA00AA,0xAA5500,0xAAAAAA,0x555555,0x5555FF,0x55FF55,0x55FFFF,0xFF5555,0xFF55FF,0xFFFF55,0xFFFFFF};
    return pal[attr & 0x0F];
}
static void fb_render_cell(int col, int row) { vesa_draw_cell(col*8, row*16, fb_buf[row][col].ch, vga_to_rgb(fb_buf[row][col].attr), 0x000000); }
static void fb_redraw_all(void) { if(!g_fb_active) return; for(int r=0;r<fb_rows;r++) for(int c=0;c<fb_cols;c++) fb_render_cell(c,r); if(vesa_has_backbuffer()) vesa_flush(); }
static void fb_scroll(void) { if(!g_fb_active) return; for(int r=0;r<fb_rows-1;r++) for(int c=0;c<fb_cols;c++) fb_buf[r][c]=fb_buf[r+1][c]; for(int c=0;c<fb_cols;c++){fb_buf[fb_rows-1][c].ch=' ';fb_buf[fb_rows-1][c].attr=fb_attr;} vesa_scroll(16); for(int c=0;c<fb_cols;c++) fb_render_cell(c,fb_rows-1); if(vesa_has_backbuffer()) vesa_flush(); }
void fb_reset(void) {
    fb_cols=(int)(g_fb.width/8); fb_rows=(int)(g_fb.height/16);
    if(fb_cols>FB_MAX_COLS)fb_cols=FB_MAX_COLS; if(fb_rows>FB_MAX_ROWS)fb_rows=FB_MAX_ROWS;
    fb_attr=0x07; fb_cur_visible=1;
    for(int r=0;r<fb_rows;r++) for(int c=0;c<fb_cols;c++){fb_buf[r][c].ch=' ';fb_buf[r][c].attr=fb_attr;}
    vesa_fill_screen(0x000000);
}
static void fb_store(int col, int row, char c, uint8_t attr) { if(col<0||col>=fb_cols||row<0||row>=fb_rows)return; fb_buf[row][col].ch=c; fb_buf[row][col].attr=attr; fb_render_cell(col,row); }
static void fb_line_clear(int row, int from_col) { for(int c=from_col;c<fb_cols;c++) fb_store(c,row,' ',fb_attr); }
static int term_cols(void) { return g_fb_active?fb_cols:VGA_WIDTH; }
static int term_rows(void) { return g_fb_active?fb_rows:VGA_HEIGHT; }

void set_vga_color(uint8_t color) { vga_attr=color; fb_attr=color; }

static void vga_set_cursor(void) { unsigned short pos=cy*term_cols()+cx; outb(0x3D4,0x0F); outb(0x3D5,pos&0xFF); outb(0x3D4,0x0E); outb(0x3D5,(pos>>8)&0xFF); }

static void vga_scroll(void) {
    int cols=term_cols(), rows=term_rows();
    if(g_fb_active){
        cy=rows-1; fb_scroll();
        for(int i=0;i<VGA_WIDTH*(VGA_HEIGHT-1);i++) vga[i]=vga[i+VGA_WIDTH];
        for(int i=VGA_WIDTH*(VGA_HEIGHT-1);i<VGA_WIDTH*VGA_HEIGHT;i++) vga[i]=(vga_attr<<8)|' ';
        return;
    }
    for(int i=0;i<cols*(rows-1);i++) vga[i]=vga[i+cols];
    for(int i=cols*(rows-1);i<cols*rows;i++) vga[i]=(vga_attr<<8)|' ';
    cy=rows-1;
}

void vga_putchar(char c) {
    int cols=term_cols(), rows=term_rows();
    if(c=='\n'){ cx=0; cy++; }
    else if(c=='\b'){
        if(cx>0){
            cx--;
            if(g_fb_active) fb_store(cx,cy,' ',vga_attr);
            if(cy<VGA_HEIGHT && cx<VGA_WIDTH) vga[cy*VGA_WIDTH+cx]=(vga_attr<<8)|' ';
        }
    }
    else if(c=='\r'){ cx=0; }
    else {
        if(g_fb_active) fb_store(cx,cy,c,vga_attr);
        if(cy<VGA_HEIGHT && cx<VGA_WIDTH) vga[cy*VGA_WIDTH+cx]=(vga_attr<<8)|c;
        cx++;
    }
    if(cx>=cols){ cx=0; cy++; }
    if(cy>=rows) vga_scroll();
    if(!g_fb_active) vga_set_cursor();
}

void vga_puts(const char *s) { while(*s) vga_putchar(*s++); }

void vga_clear(void) {
    if(g_fb_active){ fb_reset(); }
    for(int i=0;i<VGA_WIDTH*VGA_HEIGHT;i++) vga[i]=(vga_attr<<8)|' ';
    cx=cy=0;
    if(g_fb_active && vesa_has_backbuffer()) vesa_flush();
}
