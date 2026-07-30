/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ dock ~ gerenciando app/processo, baka fique quieto!
 * arquivo: dock.h ~ funcoes anotadas: 0
 */
/* ~*~ dock.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef DOCK_H
#define DOCK_H

// ~~ dock.h ~~ Header principal do sistema Dock~
// Interface de aplicações e módulos do TipOS.

// Versão do Dock (começando na 0.1.0 pq sou humilde~)
#define DOCK_VERSION "0.1.0"

// ~~ dock_err_t ~~ Códigos de erro do Dock.
// DOCK_OK = 0: tudo certo (óbvio~)
// DOCK_ERR_MEM = -1: sem memória (chora~)
// DOCK_ERR_ABI = -2: incompatibilidade de ABI (versão errada, sua culpa)
// DOCK_ERR_MANIFEST = -3: manifest inválido (parse falhou~)
// DOCK_ERR_LOAD = -4: falha ao carregar módulo (deu ruim~)
// DOCK_ERR_INIT = -5: falha na inicialização (tente de novo~)
typedef enum {
    DOCK_OK      = 0,
    DOCK_ERR_MEM = -1,
    DOCK_ERR_ABI = -2,
    DOCK_ERR_MANIFEST = -3,
    DOCK_ERR_LOAD = -4,
    DOCK_ERR_INIT = -5,
} dock_err_t;

#endif




/* ♥ dock.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
