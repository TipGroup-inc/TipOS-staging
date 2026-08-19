/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: env.c ~ funcoes anotadas: 6
 */
/* ♥ env.c ~ feito com carinho (e gambiarras) pela equipe TipOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ ENV ~ Variaveis de ambiente do kernel!
 * PATH, HOME, PS1 e mais~ tudo que o shell precisa~
 * Aliases pra nao digitar comandos gigantes~
 * "ll" = "ls -l" ~ hihi que bonitinho!
 * Max 32 vars, 32 aliases~ se passar, chora~ */

#include <stddef.h>
#include "env.h"
#include "utils.h"

#define ENV_MAX 32
#define ALIAS_MAX 32

static char env_name[ENV_MAX][32];
static char env_val[ENV_MAX][256];
static int env_count = 0;

static char alias_name[ALIAS_MAX][64];
static char alias_val[ALIAS_MAX][256];
static int alias_count = 0;

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
const char *env_get(const char *name) {
    for (int i = 0; i < env_count; i++)
        if (strcmp(env_name[i], name) == 0) return env_val[i];
    return NULL;
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
void env_set(const char *name, const char *val) {
    for (int i = 0; i < env_count; i++)
        if (strcmp(env_name[i], name) == 0) {
            strncpy(env_val[i], val, 255);
            return;
        }
    if (env_count < ENV_MAX) {
        strncpy(env_name[env_count], name, 31);
        strncpy(env_val[env_count], val, 255);
        env_count++;
    }
}

/* ~~ Inicializando~~ torce pra não dar panic! */
/* ~ essa demorou pra debugar, respeita ~ */
void env_init(void) {
    env_set("PATH", "/BIN:/USR/BIN:/LOCAL/BIN");
    env_set("HOME", "/");
    env_set("EDITOR", "edit");
    env_set("SHELL", "Mk");
    env_set("PS1", "[\\w]\\$ ");
}

/* ~ essa demorou pra debugar, respeita ~ */
const char *alias_get(const char *name) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(alias_name[i], name) == 0) return alias_val[i];
    return NULL;
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void alias_set(const char *name, const char *val) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(alias_name[i], name) == 0) {
            strncpy(alias_val[i], val, 255);
            return;
        }
    if (alias_count < ALIAS_MAX) {
        strncpy(alias_name[alias_count], name, 63);
        strncpy(alias_val[alias_count], val, 255);
        alias_count++;
    }
}

/* ~~ Setando~~ espero que saiba o que ta fazendo! */
/* ~ essa demorou pra debugar, respeita ~ */
int alias_unset(const char *name) {
    for (int i = 0; i < alias_count; i++)
        if (strcmp(alias_name[i], name) == 0) {
            for (int j = i; j < alias_count - 1; j++) {
                strcpy(alias_name[j], alias_name[j+1]);
                strcpy(alias_val[j], alias_val[j+1]);
            }
            alias_count--;
            return 1;
        }
    return 0;
}

/* ~~ Pegando o valor~ só confia que ta certo~ */
int env_count_get(void) { return env_count; }
const char *env_name_get(int i) { return env_name[i]; }
const char *env_val_get(int i) { return env_val[i]; }
/* ~~ Pegando o valor~ só confia que ta certo~ */
int alias_count_get(void) { return alias_count; }
const char *alias_name_get(int i) { return alias_name[i]; }
const char *alias_val_get(int i) { return alias_val[i]; }




/* ♥ env.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
