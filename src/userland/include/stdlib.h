/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: stdlib.h ~ funcoes anotadas: 0
 */
/* ~*~ stdlib.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe OvsbOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef STDLIB_H
#define STDLIB_H

// ~~ stdlib.h ~~ Funções utilitárias padrão~
// Aqui tem alocação, conversão e término de processo

// ── Conversão de strings ───────────────────────────────────
int atoi(const char *s);            // String → int ("42" vira 42, óbvio~)
char *itoa(int n, char *buf);       // Int → string (o contrário do atoi~)

// ── Alocação de memória ────────────────────────────────────
void *malloc(int n);                // Aloca n bytes (ou NULL se sem memória~)
void *calloc(int n, int size);      // Aloca + zera (n * size bytes nil)
void *realloc(void *p, int n);      // Realoca (copia se precisar crescer~)
void free(void *p);                 // Libera (não esquece de chamar, hein!)

// ── Mapeamento de memória ──────────────────────────────────
void *mmap(void *addr, int length, int prot, int flags);  // Mapeia páginas
int munmap(void *addr, int length);  // Desmapeia páginas (limpeza~)

// ── Término ────────────────────────────────────────────────
void exit(int code);                 // Sai do processo (tchau tchau~)

#endif




/* ♥ stdlib.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
