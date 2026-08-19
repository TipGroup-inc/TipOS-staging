/* moe moe kyun <3 */
/* moe moe kyun <3 */
/* ♥ userland ~ programinha de ring 3, longe do kernel!
 * arquivo: stat.h ~ funcoes anotadas: 0
 */
/* ~*~ stat.h ~*~
 * Hihi, olha esse arquivo aqui~ Que lindo, né? >_<
 * Escrito com muito amor (e gambiarras) pela equipe TipOS!
 * Se quebrar, a culpa é sua~ <3
 *~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*~*/

#ifndef SYS_STAT_H
#define SYS_STAT_H

// ~~ sys/stat.h ~~ Status de arquivos (stat/fstat)~
// Versão minimalista (só o essencial pra sobreviver~)

// ~~ stat ~~
// Estrutura de informação de arquivo.
// st_size: tamanho em bytes (o que interessa~)
// st_mode: tipo e permissões (bitmask, sou preguiçosa pra explicar~)
struct stat {
    unsigned int st_size;
    unsigned int st_mode;
};

// stat: informações por path (usa syscall)
int stat(const char *path, struct stat *buf);

// fstat: informações por fd (pra quando já abriu~)
int fstat(int fd, struct stat *buf);

#endif




/* ♥ stat.h ~ arquivo fofinho do OvsbMkM! kyun~ <3 */
