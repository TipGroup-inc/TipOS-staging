/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: string.h ~ funcoes anotadas: 0
 */
/* ~*~ string.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef STRING_H
#define STRING_H

int strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, int n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, int n);
char *strcat(char *dst, const char *src);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);
char *strtok(char *str, const char *delim);
void *memset(void *s, int c, int n);
void *memcpy(void *dst, const void *src, int n);
void *memmove(void *dst, const void *src, int n);
char *strdup(const char *s);

#endif




/* ♥ string.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
