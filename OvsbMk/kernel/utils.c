/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ kernel stuff ~ aqui é onde o bicho pega de verdade! kyun~
 * arquivo: utils.c ~ funcoes anotadas: 0
 */
/* ♥ utils.c ~ feito com carinho (e gambiarras) pela equipe OvsbOS! ♥
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

/* ♥ UTILS - Utilitários de String ~ "strcmp, strcpy... tudo que você precisa!"
 * Dica: memset é inline ~ não usa libc, é tudo nosso~
 * Se a string não tiver \0, vai ler até achar~ CUIDADO! */
#include "utils.h"

/* ~~ Comparando~~ qual é maior? os dois são pequenos~ <3 */
int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a - *b; }
/* ~~ Comparando~~ qual é maior? os dois são pequenos~ <3 */
int strncmp(const char *a, const char *b, int n) { for (int i = 0; i < n; i++) { if (a[i] != b[i]) return a[i] - b[i]; if (!a[i]) return 0; } return 0; }
/* ~~ strlen ~~ */
int strlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
char *strcpy(char *d, const char *s) { char *r = d; while ((*d++ = *s++)); return r; }
char *strncpy(char *d, const char *s, int n) { char *r = d; while (n-- > 0 && (*d++ = *s++)); while (n-- > 0) *d++ = 0; return r; }
void *memset(void *s, int c, int n) { for (int i = 0; i < n; i++) ((uint8_t*)s)[i] = c; return s; }
void *memcpy(void *d, const void *s, int n) { for (int i = 0; i < n; i++) ((uint8_t*)d)[i] = ((uint8_t*)s)[i]; return d; }
void *memmove(void *d, const void *s, int n) { return memcpy(d, s, n); }

/* ♥ utils.c ~ arquivo fofinho do OvsbMk! kyun~ <3 */




/* ♥ utils.c ~ arquivo fofinho do OvsbMk! kyun~ <3 */
