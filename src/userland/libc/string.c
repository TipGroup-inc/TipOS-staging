/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: string.c ~ funcoes anotadas: 14
 */
/* ~*~ string.c ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#include <stdlib.h>

/* ~~ strlen ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
int strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

/* ~~ Comparando~~ qual é maior? os dois são pequenos~ <3 */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

/* ~~ Comparando~~ qual é maior? os dois são pequenos~ <3 */
/* ~ essa demorou pra debugar, respeita ~ */
int strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

/* ~ cuidado que essa aqui morde ~ */
char *strcpy(char *dst, const char *src) {
    char *p = dst;
    while ((*p++ = *src++));
    return dst;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
char *strncpy(char *dst, const char *src, int n) {
    char *p = dst;
    while (n-- && (*p++ = *src++));
    return dst;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
char *strcat(char *dst, const char *src) {
    char *p = dst + strlen(dst);
    while ((*p++ = *src++));
    return dst;
}

/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
char *strchr(const char *s, int c) {
    while (*s) { if (*s == c) return (char *)s; s++; }
    return 0;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
char *strrchr(const char *s, int c) {
    const char *p = 0;
    while (*s) { if (*s == c) p = s; s++; }
    return (char *)p;
}

/* ~ essa demorou pra debugar, respeita ~ */
char *strstr(const char *haystack, const char *needle) {
    int nl = strlen(needle);
    if (!nl) return (char *)haystack;
    while (*haystack) {
        if (strncmp(haystack, needle, nl) == 0) return (char *)haystack;
        haystack++;
    }
    return 0;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
char *strtok(char *str, const char *delim) {
    static char *next;
    if (str) next = str;
    if (!next) return 0;
    while (*next && strchr(delim, *next)) next++;
    if (!*next) return 0;
    char *start = next;
    while (*next && !strchr(delim, *next)) next++;
    if (*next) { *next++ = '\0'; }
    return start;
}

/* ~ essa demorou pra debugar, respeita ~ */
void *memset(void *s, int c, int n) {
    for (int i = 0; i < n; i++) ((unsigned char *)s)[i] = (unsigned char)c;
    return s;
}

/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void *memcpy(void *dst, const void *src, int n) {
    for (int i = 0; i < n; i++) ((unsigned char *)dst)[i] = ((const unsigned char *)src)[i];
    return dst;
}

/* ~ essa demorou pra debugar, respeita ~ */
void *memmove(void *dst, const void *src, int n) {
    if ((const char *)src > (const char *)dst) {
        for (int i = 0; i < n; i++) ((char *)dst)[i] = ((const char *)src)[i];
    } else if (src != dst) {
        for (int i = n - 1; i >= 0; i--) ((char *)dst)[i] = ((const char *)src)[i];
    }
    return dst;
}

/* ~ simples mas essencial, n mexe sem saber oq ta fazendo */
char *strdup(const char *s) {
    if (!s) return 0;
    int n = strlen(s);
    char *p = malloc(n + 1);
    if (!p) return 0;
    for (int i = 0; i <= n; i++) p[i] = s[i];
    return p;
}




/* ♥ string.c ~ feito com carinho (e uma raiva controlada) ~ kyun! */
