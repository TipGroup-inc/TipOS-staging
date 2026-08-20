/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: string.h ~ funcoes anotadas: 0
 */
/* ~*~ string.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe OvsbOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef STRING_H
#define STRING_H

// ~~ string.h ~~ Operações de string e memória~
// Tudo que você precisa pra brincar com bytes~

// ── Comprimento e comparação ───────────────────────────────
int strlen(const char *s);                    // Tamanho da string (até o \0~)
int strcmp(const char *a, const char *b);     // Compara duas strings (0 = iguais)
int strncmp(const char *a, const char *b, int n); // Compara até n caracteres

// ── Cópia e concatenação ──────────────────────────────────
char *strcpy(char *dst, const char *src);     // Copia src pra dst (sem frescura~)
char *strncpy(char *dst, const char *src, int n); // Copia no máximo n chars
char *strcat(char *dst, const char *src);     // Concatena src no fim de dst

// ── Busca ──────────────────────────────────────────────────
char *strchr(const char *s, int c);           // Procura c em s (primeira ocorrência~)
char *strrchr(const char *s, int c);          // Procura c em s (última ocorrência~)
char *strstr(const char *haystack, const char *needle); // Procura substring (agulha no palheiro~)
char *strtok(char *str, const char *delim);   // Tokeniza string (estado global!)

// ── Manipulação de memória ─────────────────────────────────
void *memset(void *s, int c, int n);          // Preenche n bytes com c
void *memcpy(void *dst, const void *src, int n); // Copia n bytes (não overlapping!)
void *memmove(void *dst, const void *src, int n); // Copia n bytes (seguro pra overlap~)

// ── Duplicação ─────────────────────────────────────────────
char *strdup(const char *s);                  // Duplica string (malloc interna~)

#endif




/* ♥ string.h ~ feito com carinho (e uma raiva controlada) ~ kyun! */
