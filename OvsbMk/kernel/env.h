/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: env.h ~ funcoes anotadas: 0
 */
/* ♥ env.h ~ feito com carinho (e gambiarras) pela equipe OvsbOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef ENV_H
#define ENV_H
#include <stddef.h>

/* ♥ ENV ~ Variaveis de ambiente do kernel! "PATH, HOME, PS1..."
 * Dica: max 32 vars, max 256 chars por valor~
 * Aliases tambem! max 32 aliases~
 * Inicializa com PATH=/BIN:/USR/BIN, HOME=/, SHELL=Mk~
 * PS1 = [\\w]\\$ ~ hihi prompt bonitinho! */

void env_init(void);
const char *env_get(const char *name);
void env_set(const char *name, const char *val);

const char *alias_get(const char *name);
void alias_set(const char *name, const char *val);
int alias_unset(const char *name);

int env_count_get(void);
const char *env_name_get(int i);
const char *env_val_get(int i);

int alias_count_get(void);
const char *alias_name_get(int i);
const char *alias_val_get(int i);

#endif




/* ♥ env.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
