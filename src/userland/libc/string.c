/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: string.c ~ funcoes anotadas: 14
 */
 /*~*~ string.c ~*~
  * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
  * Escrito com muito amor (e gambiarras) pela equipe TipOS!
  * Se quebrar, a culpa é sua~ <3
  *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

// ~~ string.c ~~
// Implementação da libc para manipulação de strings e memória.
// Tudo byte-a-byte (sem SIMD, sem otimizações mágicas~).
// Funções: strlen, strcmp, strncmp, strcpy, strncpy, strcat,
// strchr, strrchr, strstr, strtok, memset, memcpy, memmove, strdup.
// Nota: strtok usa estado global (static), não é thread-safe~ >_<

#include <stdlib.h>

/* ~~ strlen ~~ */
/* ~ essa demorou pra debugar, respeita ~ */
// ~~ strlen ~~
// Retorna o comprimento da string (não conta o \0 terminal).
// Percorre até achar o terminador nulo. O(n), simples e direto~ ☆
int strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

// ~~ strcmp ~~
// Compara duas strings lexicograficamente.
// Retorna <0, 0, ou >0 se a < b, a == b, ou a > b.
// Compara byte a byte até diferença ou \0. Clássico~ ☆
/* ~~ Comparando~~ qual é maior? os dois são pequenos~ <3 */
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

// ~~ strncmp ~~
// Compara até n caracteres de duas strings.
// Se uma terminar antes de n, a comparação para (a diferença é no \0~)
/* ~~ Comparando~~ qual é maior? os dois são pequenos~ <3 */
/* ~ essa demorou pra debugar, respeita ~ */
int strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (!a[i]) return 0;
    }
    return 0;
}

// ~~ strcpy ~~
// Copia src pra dst (incluindo \0). dst precisa ter espaço suficiente~
// Retorna dst (pra encadeamento~). Se der overflow, problema seu! >_<
/* ~ cuidado que essa aqui morde ~ */
char *strcpy(char *dst, const char *src) {
    char *p = dst;
    while ((*p++ = *src++));
    return dst;
}

// ~~ strncpy ~~
// Copia no máximo n caracteres. Se src for mais curto, preenche com \0
// até n. Se src for maior, não adiciona \0 terminal (CUIDADO~)
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
char *strncpy(char *dst, const char *src, int n) {
    char *p = dst;
    while (n-- && (*p++ = *src++));
    return dst;
}

// ~~ strcat ~~
// Concatena src no final de dst. dst precisa ter espaço pra str(dst)+str(src)+1.
// Retorna dst. É só um strcpy começando no \0 de dst~ ☆
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
char *strcat(char *dst, const char *src) {
    char *p = dst + strlen(dst);
    while ((*p++ = *src++));
    return dst;
}

// ~~ strchr ~~
// Procura a primeira ocorrência de c (como char) em s.
// Retorna ponteiro pra posição ou NULL se não achar.
// Se c == '\0', retorna o ponteiro pro \0 terminal~ (caso especial~)
/* ~ essa funcao aqui e a mais importante, presta atencao baka! */
char *strchr(const char *s, int c) {
    while (*s) { if (*s == c) return (char *)s; s++; }
    return 0;
}

// ~~ strrchr ~~
// Procura a ÚLTIMA ocorrência de c em s (r = reverse~).
// Percorre a string inteira atualizando o ponteiro a cada match.
// No fim, retorna a última posição ou NULL.
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
char *strrchr(const char *s, int c) {
    const char *p = 0;
    while (*s) { if (*s == c) p = s; s++; }
    return (char *)p;
}

// ~~ strstr ~~
// Procura a substring `needle` dentro de `haystack`.
// Algoritmo ingênuo O(n*m), sem KMP ou Boyer-Moore (sou preguiçosa~).
// Se needle for vazio, retorna haystack (ninguém mexe com vazio~)
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

// ~~ strtok ~~
// Tokeniza string por delimitadores. Estado global (static next)!
// Primeira chamada: str = string, retorna primeiro token.
// Chamadas subsequentes: str = NULL, continua de onde parou.
// Modifica a string original (insere \0 no lugar dos delimitadores~)
// Não é thread-safe! Se usar em duas threads, vai dar briga~ >_<
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

// ~~ memset ~~
// Preenche n bytes a partir de s com o byte c (convertido pra unsigned char).
// Retorna s. Clássica, simples, todo mundo usa~ ☆
/* ~ essa demorou pra debugar, respeita ~ */
void *memset(void *s, int c, int n) {
    for (int i = 0; i < n; i++) ((unsigned char *)s)[i] = (unsigned char)c;
    return s;
}

// ~~ memcpy ~~
// Copia n bytes de src pra dst. Não lida com overlapping!
// Se as regiões se sobrepuserem, use memmove (ou chore~)
// Implementação byte-a-byte (sem palavras de 32 bits, sou vintage~)
/* ~ kyun~ mais uma funcao pra fazer o kernel n morrer */
void *memcpy(void *dst, const void *src, int n) {
    for (int i = 0; i < n; i++) ((unsigned char *)dst)[i] = ((const unsigned char *)src)[i];
    return dst;
}

// ~~ memmove ~~
// Copia n bytes de src pra dst, segura pra overlapping!
// Se src > dst, copia do início pro fim (forward).
// Se src <= dst e src != dst, copia do fim pro início (backward~)
// Se src == dst, não faz nada (otimização esperta~) ☆
/* ~ essa demorou pra debugar, respeita ~ */
void *memmove(void *dst, const void *src, int n) {
    if ((const char *)src > (const char *)dst) {
        for (int i = 0; i < n; i++) ((char *)dst)[i] = ((const char *)src)[i];
    } else if (src != dst) {
        for (int i = n - 1; i >= 0; i--) ((char *)dst)[i] = ((const char *)src)[i];
    }
    return dst;
}

// ~~ strdup ~~
// Duplica uma string usando malloc (quem chama, libera~).
// Aloca n+1 bytes, copia byte a byte (incluindo \0).
// Retorna NULL se malloc falhar ou se s for NULL.
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
