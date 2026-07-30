/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ OWT widget ~ toolkit bonitinho pra desenhar coisa na tela!
 * arquivo: owt_theme.h ~ funcoes anotadas: 0
 */
/* ~*~ owt_theme.h ~ "Tema dark/light ~ escolha seu lado da forca!" ~*~
 * Define as cores do tema (bg, texto, botao, borda, etc).
 * Tem THEME_DARK (roxo escuro ~ padrao) e THEME_LIGHT (claro ~ se
 * voce odeia seus olhos). Usa macros THEME_BG, THEME_TEXT etc pra
 * ficar mais facil de usar. Feito por alguem que usa tema dark desde
 * 2005 e nao pretende mudar~ >_< nyan! <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef OWT_THEME_H
#define OWT_THEME_H
#include <stdint.h>

typedef struct {
    uint32_t bg_primary;
    uint32_t bg_secondary;
    uint32_t bg_button;
    uint32_t bg_button_hover;
    uint32_t bg_button_pressed;
    uint32_t text_primary;
    uint32_t text_secondary;
    uint32_t accent;
    uint32_t border;
    int border_radius;
} owt_theme_t;

extern owt_theme_t THEME_DARK;
extern owt_theme_t THEME_LIGHT;

void owt_theme_set(owt_theme_t *theme);
owt_theme_t *owt_theme_get(void);

#define THEME_BG        (owt_theme_get()->bg_primary)
#define THEME_BG2       (owt_theme_get()->bg_secondary)
#define THEME_TEXT      (owt_theme_get()->text_primary)
#define THEME_TEXT2     (owt_theme_get()->text_secondary)
#define THEME_ACCENT    (owt_theme_get()->accent)
#define THEME_BORDER    (owt_theme_get()->border)

#endif



/* ♥ owt_theme.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
