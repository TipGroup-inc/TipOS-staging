/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: tui.h ~ funcoes anotadas: 0
 */
/* ~*~ tui.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef TUI_H
#define TUI_H

// ── Dimensões padrão do terminal ──────────────────────────
// 80 colunas × 25 linhas (padrão VGA desde sempre~)
#define TUI_COLS 80
#define TUI_ROWS 25

/* ── Cores VGA (16 cores) ──────────────────────────── */
// Paleta de 16 cores estilo VGA text mode.
// bg<<4 | fg forma o attribute byte (cada célula tem seu estilo~)
#define TUI_BLACK       0
#define TUI_BLUE        1
#define TUI_GREEN       2
#define TUI_CYAN        3
#define TUI_RED         4
#define TUI_MAGENTA     5
#define TUI_BROWN       6
#define TUI_LIGHT_GRAY  7
#define TUI_DARK_GRAY   8
#define TUI_LIGHT_BLUE  9
#define TUI_LIGHT_GREEN 10
#define TUI_LIGHT_CYAN  11
#define TUI_LIGHT_RED   12
#define TUI_LIGHT_MAGENTA 13
#define TUI_YELLOW      14
#define TUI_WHITE       15

/* ── Cores de sistema (match colors.h) ─────────────── */
// Esquema de cores consistente pra toda a UI~
#define TUI_C_PROMPT     TUI_LIGHT_GRAY
#define TUI_C_BG_PROMPT  TUI_BLACK
#define TUI_C_ERROR      TUI_LIGHT_RED
#define TUI_C_BG_ERROR   TUI_BLACK
#define TUI_C_SUCCESS    TUI_LIGHT_GREEN
#define TUI_C_BG_SUCCESS TUI_BLACK

/* ── Keys ──────────────────────────────────────────── */
// Códigos de tecla (ASCII + valores >255 pra teclas especiais~)
#define TUI_KEY_ESC     27
#define TUI_KEY_BS      127
#define TUI_KEY_TAB     9
#define TUI_KEY_ENTER   10

#define TUI_KEY_UP      300
#define TUI_KEY_DOWN    301
#define TUI_KEY_LEFT    302
#define TUI_KEY_RIGHT   303
#define TUI_KEY_HOME    304
#define TUI_KEY_END     305
#define TUI_KEY_PGUP    306
#define TUI_KEY_PGDN    307
#define TUI_KEY_INS     308
#define TUI_KEY_DEL     309
#define TUI_KEY_F1      310
#define TUI_KEY_F2      311
#define TUI_KEY_F3      312
#define TUI_KEY_F4      313
#define TUI_KEY_F5      314
#define TUI_KEY_F6      315
#define TUI_KEY_F7      316
#define TUI_KEY_F8      317
#define TUI_KEY_F9      318
#define TUI_KEY_F10     319
#define TUI_KEY_F11     320
#define TUI_KEY_F12     321

/* ── Window flags ──────────────────────────────────── */
// Flags de criação de janela: BORDER = borda, SCROLL = scroll lock
#define TUI_BORDER   1
#define TUI_SCROLL   4

/* ── Event types ───────────────────────────────────── */
// Tipos de evento pro poll: KEY = tecla, IDLE = nenhum evento~ zzz
#define TUI_EV_KEY   1
#define TUI_EV_IDLE  2

// ~~ tui_event_t ~~
// Estrutura de evento: type diz o tipo, key é o código da tecla
typedef struct {
    int type;
    int key;
} tui_event_t;

/* ── Opaque handle ─────────────────────────────────── */
// Handles opacos pra contexto TUI e janelas (não mexe na struct, baka~)
typedef struct tui_s tui_t;
typedef struct tui_win_s tui_win_t;

/* ── Screen management ─────────────────────────────── */
tui_t *tui_init(void);            // Inicializa TUI (modo alternativo, cursor oculto)
void   tui_end(tui_t *t);         // Finaliza TUI (restaura cursor, limpa tela~)
void   tui_refresh(tui_t *t);     // Renderiza diferenças no terminal real

/* ── Window management ─────────────────────────────── */
tui_win_t *tui_win_new(tui_t *t, int x, int y, int w, int h, int flags); // Cria janela
void      tui_win_close(tui_t *t, tui_win_t *w);  // Fecha e libera janela
void      tui_win_move(tui_win_t *w, int x, int y);  // Move janela
void      tui_win_resize(tui_win_t *w, int nw, int nh);  // Redimensiona janela
void      tui_win_raise(tui_t *t, tui_win_t *w);  // Traz janela pra frente
void      tui_win_title(tui_win_t *w, const char *s);  // Troca título

/* ── Drawing ───────────────────────────────────────── */
void tui_win_gotoxy(tui_win_t *w, int y, int x);     // Move cursor na janela
void tui_win_addch(tui_win_t *w, char ch);           // Escreve char na posição atual
void tui_win_addch_attr(tui_win_t *w, char ch, int fg, int bg); // Char com cor específica
void tui_win_addstr(tui_win_t *w, const char *s);    // Escreve string
void tui_win_printf(tui_win_t *w, const char *fmt, ...); // Printf formatado na janela
void tui_win_color(tui_win_t *w, int fg, int bg);    // Muda paleta atual
void tui_win_bold(tui_win_t *w, int on);             // Liga/desliga bold
void tui_win_clear(tui_win_t *w);                    // Limpa janela inteira
void tui_win_clrtoeol(tui_win_t *w);                 // Limpa do cursor até o fim da linha
void tui_win_box(tui_win_t *w, const char *title);   // Desenha borda ao redor da janela
void tui_win_fill(tui_win_t *w, char ch);            // Preenche janela com caractere

/* ── Scrolling ─────────────────────────────────────── */
void tui_win_scroll(tui_win_t *w, int lines);   // Rola o conteúdo pra cima
void tui_win_scroll_ok(tui_win_t *w, int on);   // Habilita/desabilita scroll automático

/* ── Input ─────────────────────────────────────────── */
int tui_getch(tui_t *t);                     // Lê tecla (bloqueante, parsing ANSI~)
int tui_poll_event(tui_t *t, tui_event_t *ev); // Polling não-bloqueante de eventos

/* ── Utilities ─────────────────────────────────────── */
int tui_width(tui_t *t);         // Largura da tela (sempre 80, por enquanto)
int tui_height(tui_t *t);        // Altura da tela (sempre 25)
int tui_win_width(tui_win_t *w); // Largura da janela
int tui_win_height(tui_win_t *w);// Altura da janela

/* ── High-level widgets ────────────────────────────── */
int  tui_dialog(tui_t *t, const char *title, const char *msg, const char *buttons);  // Caixa de diálogo
int  tui_prompt(tui_t *t, const char *label, char *out, int max);  // Prompt de entrada
void tui_msgbox(tui_t *t, const char *title, const char *msg);    // MessageBox simples

#endif




/* ♥ tui.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
