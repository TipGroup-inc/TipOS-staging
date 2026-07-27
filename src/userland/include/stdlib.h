/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: stdlib.h ~ funcoes anotadas: 0
 */
/* ~*~ stdlib.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef STDLIB_H
#define STDLIB_H

int atoi(const char *s);
char *itoa(int n, char *buf);
void *malloc(int n);
void *calloc(int n, int size);
void *realloc(void *p, int n);
void free(void *p);
void *mmap(void *addr, int length, int prot, int flags);
int munmap(void *addr, int length);
void exit(int code);

#endif




/* ♥ stdlib.h ~ se bugar me chama, se n bugar tb me chama ~ >u< */
